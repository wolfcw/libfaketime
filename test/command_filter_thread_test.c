#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

static int expect_fake;
static int check_stat;

static int check_timestamp(const char *api, time_t value)
{
  struct tm calendar;

  if (gmtime_r(&value, &calendar) == NULL)
  {
    perror(api);
    return 1;
  }
  if ((expect_fake && calendar.tm_year != 100) ||
      (!expect_fake && calendar.tm_year == 100))
  {
    fprintf(stderr, "%s returned unexpected year %d\n", api,
            calendar.tm_year + 1900);
    return 1;
  }
  return 0;
}

static int check_apis(void)
{
  struct timespec timespec_value;
  struct timeval timeval_value;
  struct stat stat_value;
  time_t time_value;
  int failed = 0;

  if (clock_gettime(CLOCK_REALTIME, &timespec_value) == -1)
  {
    perror("clock_gettime");
    failed = 1;
  }
  else
  {
    failed |= check_timestamp("clock_gettime", timespec_value.tv_sec);
  }
  if (gettimeofday(&timeval_value, NULL) == -1)
  {
    perror("gettimeofday");
    failed = 1;
  }
  else
  {
    failed |= check_timestamp("gettimeofday", timeval_value.tv_sec);
  }
  time_value = time(NULL);
  if (time_value == (time_t)-1)
  {
    perror("time");
    failed = 1;
  }
  else
  {
    failed |= check_timestamp("time", time_value);
  }
  if (check_stat && stat(".", &stat_value) == -1)
  {
    perror("stat");
    failed = 1;
  }
  else if (check_stat)
  {
    failed |= check_timestamp("stat", stat_value.st_mtime);
  }
  return failed;
}

static void *thread_main(void *ignored)
{
  (void)ignored;
  return (void *)(long)check_apis();
}

int main(void)
{
  pthread_t thread;
  void *thread_status;
  const char *setting = getenv("THREAD_EXPECT_FAKE");
  int failed;

  expect_fake = setting != NULL && setting[0] == '1';
  setting = getenv("THREAD_CHECK_STAT");
  check_stat = setting != NULL && setting[0] == '1';
  failed = check_apis();
  if (pthread_create(&thread, NULL, thread_main, NULL) != 0)
  {
    perror("pthread_create");
    return EXIT_FAILURE;
  }
  if (pthread_join(thread, &thread_status) != 0)
  {
    perror("pthread_join");
    return EXIT_FAILURE;
  }
  return (failed || (long)thread_status != 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
