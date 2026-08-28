#include <mach/clock.h>
#include <mach/mach.h>
#include <stdio.h>

int main(void)
{
  clock_serv_t clock_service;
  kern_return_t result;

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

  puts("clock_get_time(NULL) returned KERN_INVALID_ARGUMENT");
  return 0;
}
