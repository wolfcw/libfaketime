#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <time.h>

int main(void)
{
  struct timespec ts;
  int fd;
  int i;

  fd = open("/dev/null", O_RDONLY);
  if (fd == -1)
  {
    perror("open");
    return 1;
  }

  for (i = 0; i < 1000; i++)
  {
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
      perror("clock_gettime");
      close(fd);
      return 1;
    }
  }

  if (fcntl(fd, F_GETFD) == -1)
  {
    perror("fcntl");
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}
