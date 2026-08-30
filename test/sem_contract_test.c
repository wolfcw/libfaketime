#ifndef __APPLE__
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__GLIBC__) && \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 30))
#define HAVE_SEM_CLOCKWAIT 1
#endif

#ifdef __APPLE__
int main(void)
{
  puts("semaphore timed-wait unavailable on macOS");
  return EXIT_SUCCESS;
}
#else
static void report_case(const char *name)
{
  if (getenv("FAKETIME_SEM_TEST_VERBOSE") != NULL)
    fprintf(stderr, "semaphore-case=%s\\n", name);
}

static int check_future_realtime_wait(sem_t *semaphore)
{
  pid_t child;
  struct timespec deadline;
  int result;
  int status;

  if (clock_gettime(CLOCK_REALTIME, &deadline) == -1)
    return EXIT_FAILURE;
  deadline.tv_sec += 2;
  child = fork();
  if (child == -1)
    return EXIT_FAILURE;
  if (child == 0)
  {
    (void)usleep(100000);
    (void)sem_post(semaphore);
    _exit(EXIT_SUCCESS);
  }
  result = sem_timedwait(semaphore, &deadline);
  if (waitpid(child, &status, 0) != child ||
      !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
    return EXIT_FAILURE;
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef HAVE_SEM_CLOCKWAIT
static int check_future_monotonic_wait(sem_t *semaphore)
{
  pid_t child;
  struct timespec deadline;
  int result;
  int status;

  if (clock_gettime(CLOCK_MONOTONIC, &deadline) == -1)
    return EXIT_FAILURE;
  deadline.tv_sec += 2;
  child = fork();
  if (child == -1)
    return EXIT_FAILURE;
  if (child == 0)
  {
    (void)usleep(100000);
    (void)sem_post(semaphore);
    _exit(EXIT_SUCCESS);
  }
  result = sem_clockwait(semaphore, CLOCK_MONOTONIC, &deadline);
  if (waitpid(child, &status, 0) != child ||
      !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
    return EXIT_FAILURE;
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif

int main(void)
{
  char name[64];
  sem_t *semaphore;
  struct timespec deadline;
  int (*sem_timedwait_fn)(sem_t *, const struct timespec *) = sem_timedwait;

  (void)snprintf(name, sizeof(name), "/libfaketime-sem-%ld", (long)getpid());
  sem_unlink(name);
  semaphore = sem_open(name, O_CREAT | O_EXCL, 0600, 0);
  if (semaphore == SEM_FAILED)
  {
    perror("sem_open");
    return EXIT_FAILURE;
  }

  if (clock_gettime(CLOCK_REALTIME, &deadline) == -1)
  {
    perror("clock_gettime");
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }
  deadline.tv_sec--;

  report_case("realtime-past");
  errno = 0;
  if (sem_timedwait(semaphore, &deadline) != -1 || errno != ETIMEDOUT)
  {
    fprintf(stderr, "past semaphore deadline returned errno %d\n", errno);
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }

  report_case("null-deadline");
  errno = 0;
  if (sem_timedwait_fn(semaphore, NULL) != -1 || errno != EINVAL)
  {
    fprintf(stderr, "null semaphore deadline returned errno %d\n", errno);
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }

  report_case("realtime-future");
  if (check_future_realtime_wait(semaphore) != EXIT_SUCCESS)
  {
    fprintf(stderr, "future realtime semaphore wait failed\n");
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }

#ifdef HAVE_SEM_CLOCKWAIT
  struct timespec monotonic_deadline;
  if (clock_gettime(CLOCK_MONOTONIC, &monotonic_deadline) == -1)
  {
    perror("clock_gettime monotonic");
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }
  monotonic_deadline.tv_sec--;
  report_case("monotonic-past");
  errno = 0;
  if (sem_clockwait(semaphore, CLOCK_MONOTONIC, &monotonic_deadline) != -1 ||
      errno != ETIMEDOUT)
  {
    fprintf(stderr, "past monotonic semaphore deadline returned errno %d\n", errno);
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }

  report_case("monotonic-future");
  if (check_future_monotonic_wait(semaphore) != EXIT_SUCCESS)
  {
    fprintf(stderr, "future monotonic semaphore wait failed\n");
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }
#endif

  if (sem_close(semaphore) == -1)
  {
    perror("sem_close");
    sem_unlink(name);
    return EXIT_FAILURE;
  }
  if (sem_unlink(name) == -1)
  {
    perror("sem_unlink");
    return EXIT_FAILURE;
  }
  puts("semaphore realtime, monotonic, and null handling passed");
  return EXIT_SUCCESS;
}
#endif
