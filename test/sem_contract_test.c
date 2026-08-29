#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
int main(void)
{
  puts("semaphore timed-wait unavailable on macOS");
  return EXIT_SUCCESS;
}
#else
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

  errno = 0;
  if (sem_timedwait(semaphore, &deadline) != -1 || errno != ETIMEDOUT)
  {
    fprintf(stderr, "past semaphore deadline returned errno %d\n", errno);
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }

  errno = 0;
  if (sem_timedwait_fn(semaphore, NULL) != -1 || errno != EINVAL)
  {
    fprintf(stderr, "null semaphore deadline returned errno %d\n", errno);
    sem_close(semaphore);
    sem_unlink(name);
    return EXIT_FAILURE;
  }

  sem_close(semaphore);
  sem_unlink(name);
  puts("semaphore deadline and null handling passed");
  return EXIT_SUCCESS;
}
#endif
