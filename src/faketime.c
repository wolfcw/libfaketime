/*
 *  libfaketime wrapper command
 *
 *  This file is part of libfaketime, version 0.9.12
 *
 *  libfaketime is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License v2 as published by the
 *  Free Software Foundation.
 *
 *  libfaketime is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License v2 along
 *  with the libfaketime; if not, write to the Free Software Foundation,
 *  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Converted from shell script by Balint Reczey with the following credits
 * and comments:
 *
 * Thanks to Daniel Kahn Gillmor for improvement suggestions.

 * This wrapper exposes only a small subset of the libfaketime functionality.
 * Please see libfaketime's README file and man page for more details.

 * Acknowledgment: Parts of the functionality of this wrapper have been
 * inspired by Matthias Urlichs' datefudge 1.14.

 * Compile time configuration: Path where the libfaketime libraries can be found
 * on Linux/UNIX
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "ft_sem.h"

#include "android_compat.h"
#include "faketime_common.h"

static const char version[] = "0.9.12";

#if (defined __APPLE__) || (defined __sun)
static const char *date_cmd = "gdate";
#else
static const char *date_cmd = "date";
#endif

#define PATH_BUFSIZE 4096

/* semaphore and shared memory names */
static char sem_name[PATH_BUFSIZE] = {0}, shm_name[PATH_BUFSIZE] = {0};
static ft_sem_t wrapper_sem;

static void set_env_or_exit(const char *name, const char *value)
{
  if (setenv(name, value, true) == -1)
  {
    perror("faketime: setenv");
    exit(EXIT_FAILURE);
  }
}

static void clear_date_helper_preload(void)
{
  if (unsetenv("LD_PRELOAD") == -1 ||
      unsetenv("DYLD_INSERT_LIBRARIES") == -1 ||
      unsetenv("DYLD_FORCE_FLAT_NAMESPACE") == -1)
  {
    perror("faketime: clearing date helper preload environment");
    _exit(EXIT_FAILURE);
  }
}

static pid_t wait_for_child(pid_t pid, int *status)
{
  pid_t waited;

  do
  {
    waited = waitpid(pid, status, 0);
  }
  while (waited == -1 && errno == EINTR);
  return waited;
}

static bool parse_date_seconds(const char *value, long *result)
{
  char *end;

  errno = 0;
  *result = strtol(value, &end, 10);
  if (value == end || errno == ERANGE)
  {
    return false;
  }
  while (isspace((unsigned char)*end))
  {
    end++;
  }
  return *end == '\0';
}

static void usage(const char *name)
{
  printf("\n"
  "Usage: %s [switches] <timestamp> <program with arguments>\n"
  "\n"
  "This will run the specified 'program' with the given 'arguments'.\n"
  "The program will be tricked into seeing the given 'timestamp' as its starting date and time.\n"
  "The clock will continue to run from this timestamp. Please see the manpage (man faketime)\n"
  "for advanced options, such as stopping the wall clock and make it run faster or slower.\n"
  "\n"
  "The optional switches are:\n"
  "  -m                  : Use the multi-threaded version of libfaketime\n"
  "  -f                  : Use the advanced timestamp specification format (see manpage)\n"
  "  --exclude-monotonic : Prevent monotonic clock from drifting (not the raw monotonic one)\n"
#ifdef FAKE_PID
  "  -p PID              : Pretend that the program's process ID is PID\n"
#endif
#ifndef FAKE_STATELESS
  "  --disable-shm       : Disable use of shared memory by libfaketime.\n"
#endif
  "  --date-prog PROG    : Use specified GNU-compatible implementation of 'date' program\n"
  "\n"
  "Examples:\n"
  "%s 'last friday 5 pm' date\n"
  "%s '2008-12-24 08:15:42' date\n"
  "%s -f '+2,5y x10,0' bash -c 'date; while true; do echo $SECONDS ; sleep 1 ; done'\n"
  "%s -f '+2,5y x0,50' bash -c 'date; while true; do echo $SECONDS ; sleep 1 ; done'\n"
  "%s -f '+2,5y i2,0' bash -c 'date; while true; do date; sleep 1 ; done'\n"
  "In this single case all spawned processes will use the same global clock\n"
  "without restarting it at the start of each process.\n\n"
  "(Please note that it depends on your locale settings whether . or , has to be used for fractions)\n"
  "\n", name, name, name, name, name, name);
}

/** Clean up shared objects */
static void cleanup_shobjs(void)
{
  if (-1 == ft_sem_unlink(&wrapper_sem))
  {
    perror("faketime: ft_sem_unlink");
  }
  if (-1 == shm_unlink(shm_name))
  {
    perror("faketime: shm_unlink");
  }
}

int main (int argc, char **argv)
{
  pid_t child_pid;
  int curr_opt = 1;
  bool use_mt = false, use_direct = false;
  long offset;
  bool fake_pid = false;
  const char *pid_val;

#ifndef SILENT
  if (getenv("FAKETIME") || getenv("FAKETIME_SHARED") || getenv("FAKETIME_FAKEPID") || getenv("FAKERANDOM_SEED")) {
    fprintf(stderr, "faketime: You appear to be running faketime within a libfaketime environment. Proceeding, but check for unexpected results...\n");
  }
#endif

  while (curr_opt < argc)
  {
    if (0 == strcmp(argv[curr_opt], "-m"))
    {
      use_mt = true;
      curr_opt++;
      continue;
    }
    if (0 == strcmp(argv[curr_opt], "-p"))
    {
      if (curr_opt + 1 >= argc)
      {
        fprintf(stderr, "faketime: -p requires a further argument\n");
        exit(EXIT_FAILURE);
      }
      fake_pid = true;
      pid_val = argv[curr_opt + 1];
      curr_opt += 2;
#ifndef FAKE_PID
      fprintf(stderr, "faketime: -p argument probably won't work (try rebuilding with -DFAKE_PID)\n");
#endif
      continue;
    }
    else if (0 == strcmp(argv[curr_opt], "-f"))
    {
      use_direct = true;
      curr_opt++;
      continue;
    }
    else if (0 == strcmp(argv[curr_opt], "--exclude-monotonic"))
    {
      set_env_or_exit("FAKETIME_DONT_FAKE_MONOTONIC", "1");
      curr_opt++;
      continue;
    }
#ifndef FAKE_STATELESS
    else if (0 == strcmp(argv[curr_opt], "--disable-shm"))
    {
      set_env_or_exit("FAKETIME_DISABLE_SHM", "1");
      curr_opt++;
      continue;
    }
#endif
    else if (0 == strcmp(argv[curr_opt], "--date-prog"))
    {
      curr_opt++;
      if (curr_opt >= argc) {
        fprintf(stderr, "faketime: --date-prog requires a further argument\n");
        exit(EXIT_FAILURE);
      } else {
        date_cmd = argv[curr_opt];
        curr_opt++;
        //fprintf(stderr, "faketime: --date-prog assigned: %s\n", date_cmd);
      }
      continue;
    }
    else if ((0 == strcmp(argv[curr_opt], "-v")) ||
             (0 == strcmp(argv[curr_opt], "--version")))
    {
      printf("\n%s: Version %s\n"
         "For usage information please use '%s --help'.\n",
         argv[0], version, argv[0]);
      exit(EXIT_SUCCESS);
    }
    else if ((0 == strcmp(argv[curr_opt], "-h")) ||
             (0 == strcmp(argv[curr_opt], "-?")) ||
             (0 == strcmp(argv[curr_opt], "--help")))
    {
      usage(argv[0]);
      exit(EXIT_SUCCESS);
    }
    else
    {
      /* we parsed all options */
      break;
    }
  }

  /* we need at least a timestamp string and a command to run */
  if (argc - curr_opt < 2)
  {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if (!use_direct)
  {
    // TODO get seconds
    int pfds[2];
    if (pipe(pfds) == -1)
    {
      perror("faketime: pipe");
      exit(EXIT_FAILURE);
    }
    int ret = EXIT_SUCCESS;

    child_pid = fork();
    if (child_pid == 0)
    {
      close(1);       /* close normal stdout */
      if (dup2(pfds[1], STDOUT_FILENO) == -1)
      {
        perror("faketime: dup2");
        _exit(EXIT_FAILURE);
      }
      close(pfds[1]);
      close(pfds[0]); /* we don't need this */
      clear_date_helper_preload();
      // fprintf(stderr, "faketime: using --date-prog: %s\n", date_cmd);
      if (EXIT_SUCCESS != execlp(date_cmd, date_cmd, "-d", argv[curr_opt], "+%s",(char *) NULL))
      {
        perror("faketime: Running (g)date failed");
        _exit(EXIT_FAILURE);
      }
    }
    else if (child_pid > 0)
    {
      char buf[256] = {0}; /* e will have way less than 256 digits */
      int child_status;
      pid_t waited;
      close(pfds[1]);   /* we won't write to this */
      size_t bytes_read = 0;
      bool output_overflow = false;
      for (;;)
      {
        char byte;
        ssize_t result = read(pfds[0], &byte, sizeof(byte));
        if (result == 0)
        {
          break;
        }
        if (result == -1)
        {
          if (errno == EINTR)
          {
            continue;
          }
          perror("faketime: read");
          close(pfds[0]);
          exit(EXIT_FAILURE);
        }
        if (bytes_read < sizeof(buf) - 1)
        {
          buf[bytes_read++] = byte;
        }
        else
        {
          output_overflow = true;
        }
      }
      buf[bytes_read] = '\0';
      waited = wait_for_child(child_pid, &child_status);
      close(pfds[0]);
      if (output_overflow || waited == -1 || !WIFEXITED(child_status) ||
          WEXITSTATUS(child_status) != EXIT_SUCCESS)
      {
        printf("Error: Timestamp to fake not recognized, please re-try with a "
               "different timestamp.\n");
        exit(EXIT_FAILURE);
      }
      long date_seconds;
      __int128 offset_wide;
      if (!parse_date_seconds(buf, &date_seconds))
      {
        fprintf(stderr, "faketime: date program returned an invalid timestamp\n");
        exit(EXIT_FAILURE);
      }
      offset_wide = (__int128)date_seconds - (__int128)time(NULL);
      if (offset_wide < LONG_MIN || offset_wide > LONG_MAX)
      {
        fprintf(stderr, "faketime: date timestamp is outside the supported range\n");
        exit(EXIT_FAILURE);
      }
      offset = (long)offset_wide;
      ret = snprintf(buf, sizeof(buf), "%s%ld", (offset >= 0)?"+":"", offset);
      if (ret < 0 || (size_t)ret >= sizeof(buf))
      {
        fprintf(stderr, "faketime: generated FAKETIME value is too long\n");
        exit(EXIT_FAILURE);
      }
      set_env_or_exit("FAKETIME", buf);
    }
    else
    {
      perror("faketime: fork");
      close(pfds[0]);
      close(pfds[1]);
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    /* simply pass format string along */
    set_env_or_exit("FAKETIME", argv[curr_opt]);
  }
  if (fake_pid)
  {
    set_env_or_exit("FAKETIME_FAKEPID", pid_val);
  }
  int keepalive_fds[2];
  if (pipe(keepalive_fds) == -1)
  {
    perror("faketime: pipe");
    exit(EXIT_FAILURE);
  }

  /* we just consumed the timestamp option */
  curr_opt++;

  {
    /* create lock and shared memory */
    int shm_fd;
    int name_length;
    struct ft_shared_s *ft_shared;
    char shared_objs[PATH_BUFSIZE * 2 + 1];

    /*
     * Casting of getpid() return value to long needed to make GCC on SmartOS
     * happy, since getpid's return value's type on SmartOS is long. Since
     * getpid's return value's type is int on most other systems, and that
     * sizeof(long) always >= sizeof(int), this works on all platforms without
     * the need for crazy #ifdefs.
     */
    name_length = snprintf(sem_name, sizeof(sem_name), "/faketime_sem_%ld", (long)getpid());
    if (name_length < 0 || (size_t)name_length >= sizeof(sem_name))
    {
      fprintf(stderr, "faketime: failed to format semaphore name\n");
      exit(EXIT_FAILURE);
    }
    name_length = snprintf(shm_name, sizeof(shm_name), "/faketime_shm_%ld", (long)getpid());
    if (name_length < 0 || (size_t)name_length >= sizeof(shm_name))
    {
      fprintf(stderr, "faketime: failed to format shared-state names\n");
      exit(EXIT_FAILURE);
    }

    if (-1 == ft_sem_create(sem_name, &wrapper_sem))
    {
      perror("faketime: ft_sem_create");
      fprintf(stderr, "The faketime wrapper failed to create its lock.\nHowever, you may LD_PRELOAD libfaketime without using this wrapper.\n");
      exit(EXIT_FAILURE);
    }

    /* create shm */
    if (-1 == (shm_fd = shm_open(shm_name, O_CREAT|O_EXCL|O_RDWR, S_IWUSR|S_IRUSR)))
    {
      perror("faketime: shm_open");
      ft_sem_unlink(&wrapper_sem);
      exit(EXIT_FAILURE);
    }

    /* set shm size */
    if (-1 == ftruncate(shm_fd, sizeof(struct ft_shared_s)))
    {
      perror("faketime: ftruncate");
      close(shm_fd);
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    /* map shm */
    if (MAP_FAILED == (ft_shared = mmap(NULL, sizeof(struct ft_shared_s), PROT_READ|PROT_WRITE,
                        MAP_SHARED, shm_fd, 0)))
    {
      perror("faketime: mmap");
      close(shm_fd);
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }
    if (close(shm_fd) == -1)
    {
      perror("faketime: close");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    if (ft_sem_lock(&wrapper_sem) == -1)
    {
      perror("faketime: ft_sem_lock");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    /* init elapsed time ticks to zero */
    ft_shared->magic = FT_SHARED_MAGIC;
    ft_shared->version = FT_SHARED_VERSION;
    ft_shared->size = sizeof(struct ft_shared_s);
    ft_shared->ticks = 0;
    ft_shared->file_idx = 0;
    ft_shared->start_time_real.sec = 0;
    ft_shared->start_time_real.nsec = -1;
    ft_shared->start_time_mon.sec = 0;
    ft_shared->start_time_mon.nsec = -1;
    ft_shared->start_time_mon_raw.sec = 0;
    ft_shared->start_time_mon_raw.nsec = -1;
#ifdef CLOCK_BOOTTIME
    ft_shared->start_time_boot.sec = 0;
    ft_shared->start_time_boot.nsec = -1;
#endif

    if (-1 == munmap(ft_shared, (sizeof(struct ft_shared_s))))
    {
      perror("faketime: munmap");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    if (ft_sem_unlock(&wrapper_sem) == -1)
    {
      perror("faketime: ft_sem_unlock");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }

    if (snprintf(shared_objs, sizeof(shared_objs), "%s %s", sem_name, shm_name) < 0 ||
        strlen(sem_name) + strlen(shm_name) + 1 >= sizeof(shared_objs))
    {
      fprintf(stderr, "faketime: failed to format shared-state configuration\n");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }
    set_env_or_exit("FAKETIME_SHARED", shared_objs);
    ft_sem_close(&wrapper_sem);
  }

  {
    char *ftpl_path;
#ifdef __APPLE__
    ftpl_path = PREFIX "/libfaketime.1.dylib";
    FILE *check;
    check = fopen(ftpl_path, "ro");
    if (check == NULL)
    {
      ftpl_path = PREFIX "/lib/faketime/libfaketime.1.dylib";
    }
    else
    {
      fclose(check);
    }
    set_env_or_exit("DYLD_INSERT_LIBRARIES", ftpl_path);
    set_env_or_exit("DYLD_FORCE_FLAT_NAMESPACE", "1");
#else
    {
      char *ld_preload_new, *ld_preload = getenv("LD_PRELOAD");
      size_t len;
      if (use_mt)
      {
        /*
         * on MultiArch platforms, such as Debian, we put a literal $LIB into LD_PRELOAD.
         */
#ifndef MULTI_ARCH
        ftpl_path = PREFIX LIBDIRNAME "/libfaketimeMT.so.1";
#else
        ftpl_path = PREFIX "/$LIB/faketime/libfaketimeMT.so.1";
#endif
      }
      else
      {
#ifndef MULTI_ARCH
        ftpl_path = PREFIX LIBDIRNAME "/libfaketime.so.1";
#else
        ftpl_path = PREFIX "/$LIB/faketime/libfaketime.so.1";
#endif
      }
      len = ((ld_preload)?strlen(ld_preload) + 1: 0) + 1 + strlen(ftpl_path);
      ld_preload_new = malloc(len);
      if (ld_preload_new == NULL)
      {
        perror("faketime: malloc");
        exit(EXIT_FAILURE);
      }
      snprintf(ld_preload_new, len ,"%s%s%s", (ld_preload)?ld_preload:"",
              (ld_preload)?":":"", ftpl_path);
      set_env_or_exit("LD_PRELOAD", ld_preload_new);
      free(ld_preload_new);
    }
#endif
  }

  /* run command and clean up shared objects */
  child_pid = fork();
  if (child_pid == 0)
  {
    close(keepalive_fds[0]); /* only parent needs to read this */
    // fprintf(stderr, "faketime: Executing: %s\n", argv[curr_opt]);
    if (EXIT_SUCCESS != execvp(argv[curr_opt], &argv[curr_opt]))
    {
      perror("faketime: Running specified command failed");
      _exit(EXIT_FAILURE);
    }
  }
  else if (child_pid > 0)
  {
    int status;
    pid_t waited;
    char buf;
    close(keepalive_fds[1]); /* only children need keep this open */
    waited = wait_for_child(child_pid, &status);
    if (waited == -1)
    {
      perror("faketime: waitpid");
      close(keepalive_fds[0]);
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }
    ssize_t bytes_read;
    do
    {
      bytes_read = read(keepalive_fds[0], &buf, 1);
    }
    while (bytes_read == -1 && errno == EINTR);
    close(keepalive_fds[0]);
    if (bytes_read == -1)
    {
      perror("faketime: read");
      cleanup_shobjs();
      exit(EXIT_FAILURE);
    }
    cleanup_shobjs();
    if (WIFSIGNALED(status))
    {
      fprintf(stderr, "Caught %s\n", strsignal(WTERMSIG(status)));
      exit(EXIT_FAILURE);
    }
    if (!WIFEXITED(status))
      exit(EXIT_FAILURE);
    exit(WEXITSTATUS(status));
  }
  else
  {
    perror("faketime: fork");
    close(keepalive_fds[0]);
    close(keepalive_fds[1]);
    cleanup_shobjs();
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}

/*
 * Editor modelines
 *
 * Local variables:
 * c-basic-offset: 2
 * tab-width: 2
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */

/* eof */
