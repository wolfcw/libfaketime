#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

int main(void)
{
  time_t seconds = time(NULL);
  struct timeval timeval_value;
  struct timespec timespec_value;
  struct timespec previous_monotonic;
  struct timespec current_monotonic;
  int sample;

  if (gettimeofday(&timeval_value, NULL) != 0 ||
      clock_gettime(CLOCK_REALTIME, &timespec_value) != 0)
  {
    perror("clock consistency probe");
    return EXIT_FAILURE;
  }
  if (timeval_value.tv_sec != seconds ||
      timespec_value.tv_sec != seconds ||
      timeval_value.tv_usec < 0 || timeval_value.tv_usec >= 1000000 ||
      timespec_value.tv_nsec < 0 || timespec_value.tv_nsec >= 1000000000)
  {
    fprintf(stderr, "clock APIs returned inconsistent values\n");
    return EXIT_FAILURE;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &previous_monotonic) != 0)
  {
    perror("monotonic clock probe");
    return EXIT_FAILURE;
  }
  for (sample = 0; sample < 3; sample++)
  {
    if (clock_gettime(CLOCK_MONOTONIC, &current_monotonic) != 0 ||
        current_monotonic.tv_sec < previous_monotonic.tv_sec ||
        (current_monotonic.tv_sec == previous_monotonic.tv_sec &&
         current_monotonic.tv_nsec < previous_monotonic.tv_nsec))
    {
      fprintf(stderr, "monotonic clock moved backwards\n");
      return EXIT_FAILURE;
    }
    previous_monotonic = current_monotonic;
  }
  puts("clock APIs agreed");
  return EXIT_SUCCESS;
}
