#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../src/time_ops.h"

static int check(int condition, const char *description)
{
  if (!condition)
  {
    fprintf(stderr, "time_ops_test: %s\n", description);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int main(void)
{
  const time_t max_time = (time_t)(((uintmax_t)1 << (sizeof(time_t) * CHAR_BIT - 1)) - 1);
  const time_t min_time = (time_t)(-max_time - 1);
  struct timespec a = {1, 900000000};
  struct timespec b = {2, 200000000};
  struct timespec result;

  timespecadd(&a, &b, &result);
  if (check(result.tv_sec == 4 && result.tv_nsec == 100000000,
            "addition should normalize nanoseconds") != EXIT_SUCCESS)
    return EXIT_FAILURE;

  timespecsub(&a, &b, &result);
  if (check(result.tv_sec == -1 && result.tv_nsec == 700000000,
            "subtraction should normalize negative nanoseconds") != EXIT_SUCCESS)
    return EXIT_FAILURE;

  timespecmul(&a, 2.0, &result);
  if (check(result.tv_sec == 3 && result.tv_nsec == 800000000,
            "multiplication should preserve fractional seconds") != EXIT_SUCCESS)
    return EXIT_FAILURE;

  a.tv_sec = max_time;
  a.tv_nsec = 999999999;
  b.tv_sec = 0;
  b.tv_nsec = 1;
  errno = 0;
  timespecadd(&a, &b, &result);
  if (check(errno == EOVERFLOW && result.tv_sec == max_time &&
            result.tv_nsec == 999999999,
            "addition overflow should saturate and report EOVERFLOW") != EXIT_SUCCESS)
    return EXIT_FAILURE;

  a.tv_sec = min_time;
  a.tv_nsec = 0;
  b.tv_sec = 1;
  b.tv_nsec = 0;
  errno = 0;
  timespecsub(&a, &b, &result);
  if (check(errno == EOVERFLOW && result.tv_sec == min_time &&
            result.tv_nsec == 0,
            "subtraction underflow should saturate and report EOVERFLOW") != EXIT_SUCCESS)
    return EXIT_FAILURE;

  a.tv_sec = 1;
  a.tv_nsec = 0;
  errno = 0;
  timespecmul(&a, 1.0e100, &result);
  if (check(errno == EOVERFLOW && result.tv_nsec >= 0 &&
            result.tv_nsec < SEC_TO_nSEC,
            "multiplication overflow should report EOVERFLOW and normalize") != EXIT_SUCCESS)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
