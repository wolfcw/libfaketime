#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

int main(void)
{
  time_t seconds = time(NULL);
  struct timeval timeval_value;
  struct timespec timespec_value;

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
  puts("clock APIs agreed");
  return EXIT_SUCCESS;
}
