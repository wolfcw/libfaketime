#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
  struct rlimit old_limit;
  struct rlimit limited;
  struct timespec ts;
  int fds[256];
  int count = 0;
  int i;

  if (clock_gettime(CLOCK_REALTIME, &ts) == -1 ||
      getrlimit(RLIMIT_NOFILE, &old_limit) == -1)
    return 1;
  limited = old_limit;
  if (limited.rlim_cur > 64)
    limited.rlim_cur = 64;
  if (setrlimit(RLIMIT_NOFILE, &limited) == -1)
    return 77;
  while (count < (int)(sizeof(fds) / sizeof(fds[0])))
  {
    fds[count] = open("/dev/null", O_RDONLY);
    if (fds[count] == -1)
      break;
    count++;
  }
  if (errno != EMFILE || clock_gettime(CLOCK_REALTIME, &ts) == -1)
  {
    fprintf(stderr, "descriptor exhaustion did not preserve clock calls\n");
    for (i = 0; i < count; i++) close(fds[i]);
    setrlimit(RLIMIT_NOFILE, &old_limit);
    return 1;
  }
  for (i = 0; i < count; i++) close(fds[i]);
  if (setrlimit(RLIMIT_NOFILE, &old_limit) == -1) return 1;
  return 0;
}
