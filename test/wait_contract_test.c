#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int check_past_deadline(clockid_t clock_id, const char *name)
{
  pthread_condattr_t attr;
  pthread_cond_t condition;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  struct timespec deadline;
  int result;

  if (pthread_condattr_init(&attr) != 0
#ifndef __APPLE__
      || pthread_condattr_setclock(&attr, clock_id) != 0
#endif
      || pthread_cond_init(&condition, &attr) != 0)
  {
    fprintf(stderr, "%s condition initialization failed\n", name);
    return EXIT_FAILURE;
  }
  pthread_condattr_destroy(&attr);

  if (clock_gettime(clock_id, &deadline) != 0)
  {
    fprintf(stderr, "%s clock_gettime failed\n", name);
    pthread_cond_destroy(&condition);
    return EXIT_FAILURE;
  }
  deadline.tv_sec--;

  pthread_mutex_lock(&mutex);
  result = pthread_cond_timedwait(&condition, &mutex, &deadline);
  pthread_mutex_unlock(&mutex);
  pthread_cond_destroy(&condition);
  pthread_mutex_destroy(&mutex);

  if (result != ETIMEDOUT)
  {
    fprintf(stderr, "%s past deadline returned %d, expected ETIMEDOUT\n",
            name, result);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int main(void)
{
  if (check_past_deadline(CLOCK_REALTIME, "realtime") != EXIT_SUCCESS)
  {
    return EXIT_FAILURE;
  }

#ifndef __APPLE__
  if (check_past_deadline(CLOCK_MONOTONIC, "monotonic") != EXIT_SUCCESS)
  {
    return EXIT_FAILURE;
  }
  puts("realtime and monotonic past deadlines returned ETIMEDOUT");
#else
  puts("realtime past deadline returned ETIMEDOUT");
#endif
  return EXIT_SUCCESS;
}
