#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

static int check_faked_timestamp(const char *operation, time_t value)
{
  struct tm calendar;

  if (gmtime_r(&value, &calendar) == NULL)
  {
    perror(operation);
    return 1;
  }
  if (calendar.tm_year != 120)
  {
    fprintf(stderr, "%s returned year %d instead of 2020\n", operation,
            calendar.tm_year + 1900);
    return 1;
  }
  return 0;
}

static int exercise_clock_and_stat(void)
{
  struct stat st;
  time_t now;

  now = time(NULL);
  if (now == (time_t)-1)
  {
    perror("time");
    return 1;
  }
  if (check_faked_timestamp("time", now) != 0)
    return 1;
  if (stat("/", &st) == -1)
  {
    perror("stat");
    return 1;
  }
#ifdef __APPLE__
  return check_faked_timestamp("stat", st.st_mtimespec.tv_sec);
#else
  return check_faked_timestamp("stat", st.st_mtim.tv_sec);
#endif
}

static void close_descriptors_from_three(void)
{
  int fd;

  for (fd = 3; fd < 1024; fd++)
    (void)close(fd);
}

static int close_range_from_three(void)
{
#if defined(__linux__) && defined(SYS_close_range)
  if (syscall(SYS_close_range, 3U, UINT_MAX, 0U) == -1 && errno != ENOSYS)
  {
    perror("close_range");
    return 1;
  }
#endif
  return 0;
}

static int wait_for_success(pid_t child)
{
  int status;

  if (waitpid(child, &status, 0) != child)
  {
    perror("waitpid");
    return 1;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
  {
    fprintf(stderr, "child exited unsuccessfully\n");
    return 1;
  }
  return 0;
}

static void *thread_main(void *argument)
{
  int *failed = argument;
  int iteration;

  for (iteration = 0; iteration < 64; iteration++)
  {
    if (exercise_clock_and_stat() != 0)
    {
      *failed = 1;
      break;
    }
  }
  return NULL;
}

static int exercise_threads(void)
{
  pthread_t threads[8];
  int failures[8] = {0};
  int index;
  int failed = 0;

  for (index = 0; index < 8; index++)
  {
    if (pthread_create(&threads[index], NULL, thread_main, &failures[index]) != 0)
    {
      perror("pthread_create");
      return 1;
    }
  }
  for (index = 0; index < 8; index++)
  {
    if (pthread_join(threads[index], NULL) != 0)
    {
      perror("pthread_join");
      return 1;
    }
    failed |= failures[index];
  }
  return failed;
}

static int exercise_children(const char *program)
{
  pid_t child = fork();

  if (child == -1)
  {
    perror("fork");
    return 1;
  }
  if (child == 0)
    _exit(exercise_clock_and_stat());
  if (wait_for_success(child) != 0)
    return 1;

  child = fork();
  if (child == -1)
  {
    perror("fork");
    return 1;
  }
  if (child == 0)
  {
    execl(program, program, "--child", (char *)NULL);
    perror("execl");
    _exit(EXIT_FAILURE);
  }
  return wait_for_success(child);
}

int main(int argc, char **argv)
{
  int sentinel;

  if (argc == 2 && strcmp(argv[1], "--child") == 0)
    return exercise_clock_and_stat();
  if (argc != 1)
  {
    fprintf(stderr, "unexpected argument\n");
    return EXIT_FAILURE;
  }

  if (exercise_clock_and_stat() != 0)
    return EXIT_FAILURE;
  close_descriptors_from_three();
  if (exercise_clock_and_stat() != 0)
    return EXIT_FAILURE;
  sentinel = open("/dev/null", O_RDONLY);
  if (sentinel == -1)
  {
    perror("open");
    return EXIT_FAILURE;
  }
  if (exercise_clock_and_stat() != 0 || fcntl(sentinel, F_GETFD) == -1)
  {
    perror("sentinel");
    close(sentinel);
    return EXIT_FAILURE;
  }
  close(sentinel);

  if (close_range_from_three() != 0)
    return EXIT_FAILURE;
  if (exercise_clock_and_stat() != 0)
    return EXIT_FAILURE;
  sentinel = open("/dev/null", O_RDONLY);
  if (sentinel == -1)
  {
    perror("open");
    return EXIT_FAILURE;
  }
  if (exercise_clock_and_stat() != 0 || exercise_threads() != 0 ||
      fcntl(sentinel, F_GETFD) == -1 ||
      exercise_children(argv[0]) != 0)
  {
    perror("descriptor closure regression");
    close(sentinel);
    return EXIT_FAILURE;
  }
  close(sentinel);
  return EXIT_SUCCESS;
}
