#ifndef __APPLE__
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>

static long long nanoseconds(const struct timespec *value)
{
  return (long long)value->tv_sec * 1000000000LL + value->tv_nsec;
}

static int elapsed_is_reasonable(long long elapsed)
{
  return elapsed >= 5000000LL && elapsed <= 500000000LL;
}

static int check_select(void)
{
  struct timeval timeout = {0, 20000};
  struct timespec before;
  struct timespec after;

  if (clock_gettime(CLOCK_MONOTONIC, &before) == -1 ||
      select(0, NULL, NULL, NULL, &timeout) != 0 ||
      clock_gettime(CLOCK_MONOTONIC, &after) == -1)
  {
    return EXIT_FAILURE;
  }
  return elapsed_is_reasonable(nanoseconds(&after) - nanoseconds(&before))
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}

static int check_pselect(void)
{
  struct timespec timeout = {0, 20000000};
  sigset_t signal_mask;
  struct timespec before;
  struct timespec after;

  if (sigemptyset(&signal_mask) == -1 ||
      clock_gettime(CLOCK_MONOTONIC, &before) == -1 ||
      pselect(0, NULL, NULL, NULL, &timeout, &signal_mask) != 0 ||
      clock_gettime(CLOCK_MONOTONIC, &after) == -1)
  {
    return EXIT_FAILURE;
  }
  return elapsed_is_reasonable(nanoseconds(&after) - nanoseconds(&before))
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}

#ifndef __APPLE__
static int check_ppoll(void)
{
  struct timespec timeout = {0, 20000000};
  sigset_t signal_mask;
  struct timespec before;
  struct timespec after;

  if (sigemptyset(&signal_mask) == -1 ||
      clock_gettime(CLOCK_MONOTONIC, &before) == -1 ||
      ppoll(NULL, 0, &timeout, &signal_mask) != 0 ||
      clock_gettime(CLOCK_MONOTONIC, &after) == -1)
  {
    return EXIT_FAILURE;
  }
  return elapsed_is_reasonable(nanoseconds(&after) - nanoseconds(&before))
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}
#endif

int main(void)
{
  if (setenv("FAKETIME_DONT_FAKE_MONOTONIC", "1", 1) == -1 ||
      check_select() != EXIT_SUCCESS || check_pselect() != EXIT_SUCCESS)
  {
    fprintf(stderr, "select/pselect timeout contract failed\n");
    return EXIT_FAILURE;
  }

#ifndef __APPLE__
  if (check_ppoll() != EXIT_SUCCESS)
  {
    fprintf(stderr, "ppoll timeout contract failed\n");
    return EXIT_FAILURE;
  }
  puts("select, pselect, and ppoll timeout contracts passed");
#else
  puts("select and pselect timeout contracts passed");
#endif
  return EXIT_SUCCESS;
}
