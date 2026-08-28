#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __APPLE__
int macos_clock_gettime(clockid_t clk_id, struct timespec *tp);
#define test_clock_gettime macos_clock_gettime
#else
#define test_clock_gettime clock_gettime
#endif

int main(void)
{
  int (*clock_gettime_fn)(clockid_t, struct timespec *) = test_clock_gettime;
  struct timespec *output = (struct timespec *)(uintptr_t)0;

  errno = 0;
  if (clock_gettime_fn(CLOCK_REALTIME, output) != -1 || errno != EFAULT)
  {
    fprintf(stderr, "clock_gettime(NULL) returned errno %d\n", errno);
    return EXIT_FAILURE;
  }

  puts("clock_gettime(NULL) returned EFAULT");
  return EXIT_SUCCESS;
}
