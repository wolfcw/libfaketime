#define _GNU_SOURCE
#include <errno.h>
#include <linux/futex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static long long nanoseconds(const struct timespec *value)
{
  return (long long)value->tv_sec * 1000000000LL + value->tv_nsec;
}

static int check_elapsed(const struct timespec *before,
                         const struct timespec *after)
{
  long long elapsed = nanoseconds(after) - nanoseconds(before);
  return elapsed >= 5000000LL && elapsed <= 500000000LL;
}

int main(void)
{
  uint32_t value = 0;
  struct timespec timeout = {0, 20000000};
  struct timespec before;
  struct timespec after;
  struct timespec deadline;

  if (setenv("FAKETIME_DONT_FAKE_MONOTONIC", "1", 1) == -1 ||
      clock_gettime(CLOCK_MONOTONIC, &before) == -1)
  {
    perror("clock setup");
    return EXIT_FAILURE;
  }
  errno = 0;
  if (syscall(SYS_futex, &value, FUTEX_WAIT, 0, &timeout, NULL, 0) != -1 ||
      errno != ETIMEDOUT || clock_gettime(CLOCK_MONOTONIC, &after) == -1 ||
      !check_elapsed(&before, &after))
  {
    perror("relative FUTEX_WAIT");
    return EXIT_FAILURE;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &deadline) == -1)
  {
    perror("monotonic deadline");
    return EXIT_FAILURE;
  }
  deadline.tv_sec--;
  errno = 0;
  if (syscall(SYS_futex, &value, FUTEX_WAIT_BITSET, 0, &deadline, NULL,
              FUTEX_BITSET_MATCH_ANY) != -1 || errno != ETIMEDOUT)
  {
    perror("absolute FUTEX_WAIT_BITSET");
    return EXIT_FAILURE;
  }

  puts("relative and absolute futex timeout contracts passed");
  return EXIT_SUCCESS;
}
