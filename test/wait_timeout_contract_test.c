#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static long long nanoseconds(const struct timespec *value)
{
  return (long long)value->tv_sec * 1000000000LL + value->tv_nsec;
}

int main(void)
{
  struct timespec before;
  struct timespec after;
  int result;
  long long elapsed;

  if (setenv("FAKETIME_DONT_FAKE_MONOTONIC", "1", 1) == -1 ||
      clock_gettime(CLOCK_MONOTONIC_RAW, &before) == -1)
  {
    perror("clock setup");
    return EXIT_FAILURE;
  }
  errno = 0;
  result = poll(NULL, 0, 20);
  if (result != 0 || errno != 0 || clock_gettime(CLOCK_MONOTONIC_RAW, &after) == -1)
  {
    perror("poll");
    return EXIT_FAILURE;
  }
  elapsed = nanoseconds(&after) - nanoseconds(&before);
  if (elapsed < 5000000LL || elapsed > 500000000LL)
  {
    fprintf(stderr, "poll elapsed %lld ns outside expected range\n", elapsed);
    return EXIT_FAILURE;
  }
  if (poll(NULL, 0, 0) != 0 || errno != 0)
  {
    perror("zero poll");
    return EXIT_FAILURE;
  }
  puts("positive poll timeout was preserved");
  return EXIT_SUCCESS;
}
