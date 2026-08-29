#include <mach/clock.h>
#include <mach/mach.h>
#include <stdio.h>

int main(void)
{
  clock_serv_t clock_service;
  kern_return_t result;
  mach_timespec_t current_time;
  int i;

  result = host_get_clock_service(mach_host_self(), CALENDAR_CLOCK,
                                  &clock_service);
  if (result != KERN_SUCCESS)
  {
    fprintf(stderr, "host_get_clock_service failed: %d\n", result);
    return 1;
  }

  result = clock_get_time(clock_service, NULL);
  mach_port_deallocate(mach_task_self(), clock_service);
  if (result != KERN_INVALID_ARGUMENT)
  {
    fprintf(stderr, "clock_get_time(NULL) returned: %d\n", result);
    return 1;
  }

  for (i = 0; i < 256; i++)
  {
    result = clock_get_time(clock_service, &current_time);
    if (result != KERN_SUCCESS)
    {
      fprintf(stderr, "clock_get_time failed on iteration %d: %d\n", i, result);
      return 1;
    }
  }

  puts("clock_get_time null handling and repeated calls passed");
  return 0;
}
