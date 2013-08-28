/**
 * Faketime binary spawning commands with FTPL (faketime preload library).
 *
 * TODO: fill propery formated credits and license
 * Converted from shell script with the following credits and comments:
 *
 * Thanks to Daniel Kahn Gillmor for improvement suggestions.

 * It allows you to modify the date and time a program sees when using
 * system library calls such as time() and fstat().

 * This wrapper exposes only a small subset of the FTPL functionality.
 * Please see FTPL's README file for more details

 * Acknowledgment: Parts of the functionality of this wrapper have been
 * inspired by Matthias Urlichs' datefudge 1.14.

 * Compile time configuration: Path where the libfaketime libraries can be found
 * on Linux/UNIX . */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

#include "faketime_common.h"

const char version[] = "0.8";

#ifdef __APPLE__
static const char *date_cmd = "gdate";
#else
static const char *date_cmd = "date";
#endif

#define PATH_BUFSIZE 4096

/* semaphore and shared memory names */
char sem_name[PATH_BUFSIZE] = {0}, shm_name[PATH_BUFSIZE] = {0};


void usage(const char *name) {
  printf("\n");
  printf("Usage: %s [switches] <timestamp> <program with arguments>\n", name);
  printf("\n");
  printf("This will run the specified 'program' with the given 'arguments'.\n");
  printf("The program will be tricked into seeing the given 'timestamp' as its starting date and time.\n");
  printf("The clock will continue to run from this timestamp. Please see the manpage (man faketime)\n");
  printf("for advanced options, such as stopping the wall clock and make it run faster or slower.\n");
  printf("\n");
  printf("The optional switches are:\n");
  printf("  -m        : Use the multi-threaded version of libfaketime\n");
  printf("  -f        : Use the advanced timestamp specification format (see manpage)\n");
  printf("\n");
  printf("Examples:\n");
  printf("%s 'last friday 5 pm' /bin/date\n", name);
  printf("%s '2008-12-24 08:15:42' /bin/date\n", name);
  printf("%s -f '+2,5y x10,0' /bin/bash -c 'date; while true; do echo $SECONDS ; sleep 1 ; done'\n", name);
  printf("%s -f '+2,5y x0,50' /bin/bash -c 'date; while true; do echo $SECONDS ; sleep 1 ; done'\n", name);
  printf("%s -f '+2,5y i2,0' /bin/bash -c 'date; while true; do date; sleep 1 ; done'\n", name);
  printf("In this single case all spawned processes will use the same global clock\n");
  printf("without restaring it at the start of each process.\n\n");
  printf("(Please note that it depends on your locale settings whether . or , has to be used for fractions)\n");
  printf("\n");

}

/** Clean up shared objects */
static void cleanup_shobjs () {
  if (-1 == sem_unlink(sem_name)) {
    perror("sem_unlink");
  }
  if (-1 == shm_unlink(shm_name)) {
    perror("shm_unlink");
  }
}

int main (int argc, char **argv)
{
  pid_t child_pid;
  int curr_opt = 1;
  bool use_mt = false, use_direct = false;
  int pfds[2];
  long offset;

  while(curr_opt < argc) {
    if (0 == strcmp(argv[curr_opt], "-m")) {
      use_mt = true;
      curr_opt++;
      continue;
    } else if (0 == strcmp(argv[curr_opt], "-f")) {
      use_direct = true;
      curr_opt++;
      continue;
    } else if ((0 == strcmp(argv[curr_opt], "-v")) ||
	       (0 == strcmp(argv[curr_opt], "--version"))) {
      printf("\n%s: Version %s\n"
	     "For usage information please use '%s --help\n'.",
	     argv[0], version, argv[0]);
      exit(EXIT_SUCCESS);
    } else if ((0 == strcmp(argv[curr_opt], "-h")) ||
	       (0 == strcmp(argv[curr_opt], "-?")) ||
	       (0 == strcmp(argv[curr_opt], "--help"))) {
      usage(argv[0]);
      exit(EXIT_SUCCESS);
    } else {
      /* we parsed all options */
      break;
    }
  }

  /* we need at least a timestamp string and a command to run */
  if (argc - curr_opt < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if (!use_direct) {
    // TODO get seconds
    pipe(pfds);
    int ret = EXIT_SUCCESS;

    if (0 == (child_pid = fork())) {
      close(1);       /* close normal stdout */
      dup(pfds[1]);   /* make stdout same as pfds[1] */
      close(pfds[0]); /* we don't need this */
      if (EXIT_SUCCESS != execlp(date_cmd, date_cmd, "-d", argv[curr_opt], "+%s",(char *) NULL)) {
	perror("Running (g)date failed");
	exit(EXIT_FAILURE);
      }
    } else {
      char buf[256] = {0}; /* e will have way less than 256 digits */
      close(pfds[1]);   /* we won't write to this */
      read(pfds[0], buf, 256);
      waitpid(child_pid, &ret, 0);
      if (ret != EXIT_SUCCESS) {
	printf("Error: Timestamp to fake not recognized, please re-try with a "
	       "different timestamp.\n");
	exit(EXIT_FAILURE);
      }
      offset = atol(buf) - time(NULL);
      ret = snprintf(buf, sizeof(buf), "%s%ld", (offset >= 0)?"+":"", offset);
      setenv("FAKETIME", buf, true);
    }
  } else {
    /* simply pass format string along */
    setenv("FAKETIME", argv[curr_opt], true);
  }

  /* we just consumed the timestamp option */
  curr_opt++;

  {
    /* create semaphores and shared memory */
    int shm_fd;
    sem_t *sem;
    struct ft_shared_s *ft_shared;
    char shared_objs[PATH_BUFSIZE];

    snprintf(sem_name, PATH_BUFSIZE -1 ,"/faketime_sem_%d", getpid());
    snprintf(shm_name, PATH_BUFSIZE -1 ,"/faketime_shm_%d", getpid());

    if (SEM_FAILED == (sem = sem_open(sem_name, O_CREAT|O_EXCL, S_IWUSR|S_IRUSR, 1))) {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }

    /* create shm */
    if (-1 == (shm_fd = shm_open(shm_name, O_CREAT|O_EXCL|O_RDWR, S_IWUSR|S_IRUSR))) {
      perror("shm_open");
      if (-1 == sem_unlink(argv[2])) {
	perror("sem_unlink");
      }
      exit(EXIT_FAILURE);
    }

    /* set shm size */
    if (-1 == ftruncate(shm_fd, sizeof(uint64_t))) {
      perror("ftruncate");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    /* map shm */
    if (MAP_FAILED == (ft_shared = mmap(NULL, sizeof(struct ft_shared_s), PROT_READ|PROT_WRITE,
				    MAP_SHARED, shm_fd, 0))) {
      perror("mmap");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    if (sem_wait(sem) == -1) {
      perror("sem_wait");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    /* init elapsed time ticks to zero */
    ft_shared->ticks = 0;
    ft_shared->file_idx = 0;
    ft_shared->start_time.real.tv_sec = 0;
    ft_shared->start_time.real.tv_nsec = -1;
    ft_shared->start_time.mon.tv_sec = 0;
    ft_shared->start_time.mon.tv_nsec = -1;
    ft_shared->start_time.mon_raw.tv_sec = 0;
    ft_shared->start_time.mon_raw.tv_nsec = -1;

    if (-1 == munmap(ft_shared, (sizeof(struct ft_shared_s)))) {
      perror("munmap");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    if (sem_post(sem) == -1) {
      perror("semop");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    snprintf(shared_objs, PATH_BUFSIZE, "%s %s", sem_name, shm_name);
    setenv("FAKETIME_SHARED", shared_objs, true);

  }

  {
    char *ftpl_path;  
#ifdef __APPLE__
    ftpl_path = PREFIX "/libfaketime.dylib.1";
    setenv("DYLD_INSERT_LIBRARIES", ftpl_path, true);
    setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", true);
#else
    {
      char *ld_preload_new, *ld_preload = getenv("LD_PRELOAD");
      size_t len;
      if (use_mt) {
	ftpl_path = PREFIX "/lib/faketime/libfaketimeMT.so.1";
      } else {
	ftpl_path = PREFIX "/lib/faketime/libfaketime.so.1";
      }
      len = (ld_preload)?strlen(ld_preload):0 + 2 + strlen(ftpl_path);
      ld_preload_new = malloc(len);
      snprintf(ld_preload_new, len ,"%s%s%s", (ld_preload)?ld_preload:"",
	       (ld_preload)?":":"", ftpl_path);
      setenv("LD_PRELOAD", ld_preload_new, true);
      free(ld_preload_new);
    }
#endif
  }

  /* run command and clean up shared objects */
  if (0 == (child_pid = fork())) {
    if (EXIT_SUCCESS != execvp(argv[curr_opt], &argv[curr_opt])) {
      perror("Running specified command failed");
      exit(EXIT_FAILURE);
    }
  } else {
    int ret;
    waitpid(child_pid, &ret, 0);
    cleanup_shobjs();
    exit(ret);
  }
}
