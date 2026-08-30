#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  struct itimerspec timer = {0};
  struct pollfd descriptor;
  struct timespec now;
  uint64_t expirations;
  int flags = TFD_TIMER_ABSTIME;
  int fd;

  (void)argv;

  fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
  if (fd == -1 || clock_gettime(CLOCK_MONOTONIC, &now) == -1)
  {
    perror("timerfd setup");
    return EXIT_FAILURE;
  }
  if (argc > 1)
  {
    if (argv[1][0] == 'r' || argv[1][0] == 'p')
    {
      flags = 0;
      timer.it_value.tv_nsec = 100000000;
      if (argv[1][0] == 'p')
        timer.it_interval.tv_nsec = 100000000;
    }
    else
    {
      now.tv_sec++;
      timer.it_value = now;
    }
  }
  else
  {
    now.tv_sec--;
    timer.it_value = now;
  }

  if (timerfd_settime(fd, flags, &timer, NULL) == -1)
  {
    perror("timerfd_settime");
    close(fd);
    return EXIT_FAILURE;
  }

  if (argc > 1 && argv[1][0] == 'p')
  {
    struct timespec delay = {0, 250000000};
    nanosleep(&delay, NULL);
  }

  descriptor.fd = fd;
  descriptor.events = POLLIN;
  if (poll(&descriptor, 1, 1000) != 1 || !(descriptor.revents & POLLIN) ||
      read(fd, &expirations, sizeof(expirations)) != sizeof(expirations) ||
      expirations == 0 ||
      (argc > 1 && argv[1][0] == 'p' && expirations < 2))
  {
    fprintf(stderr, "expired timerfd was not readable\n");
    close(fd);
    return EXIT_FAILURE;
  }

  close(fd);
  if (argc > 1 && argv[1][0] == 'p')
    puts("periodic monotonic timerfd expiration count honored");
  else if (argc > 1 && argv[1][0] != 'r')
    puts("absolute monotonic timerfd deadline honored");
  else if (argc > 1)
    puts("relative monotonic timerfd deadline honored");
  else
    puts("expired monotonic timerfd became readable");
  return EXIT_SUCCESS;
}
