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
  int fd;

  (void)argv;

  fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
  if (fd == -1 || clock_gettime(CLOCK_MONOTONIC, &now) == -1)
  {
    perror("timerfd setup");
    return EXIT_FAILURE;
  }
  if (argc > 1)
    now.tv_sec++;
  else
    now.tv_sec--;
  timer.it_value = now;

  if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL) == -1)
  {
    perror("timerfd_settime");
    close(fd);
    return EXIT_FAILURE;
  }

  descriptor.fd = fd;
  descriptor.events = POLLIN;
  if (poll(&descriptor, 1, 1000) != 1 || !(descriptor.revents & POLLIN) ||
      read(fd, &expirations, sizeof(expirations)) != sizeof(expirations) ||
      expirations == 0)
  {
    fprintf(stderr, "expired timerfd was not readable\n");
    close(fd);
    return EXIT_FAILURE;
  }

  close(fd);
  if (argc > 1)
    puts("absolute monotonic timerfd deadline honored");
  else
    puts("expired monotonic timerfd became readable");
  return EXIT_SUCCESS;
}
