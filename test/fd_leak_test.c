#define _GNU_SOURCE

#include <dirent.h>
#include <stdio.h>
#include <time.h>

static int open_fd_count(void)
{
  DIR *dir = opendir("/proc/self/fd");
  struct dirent *entry;
  int count = 0;

  if (dir == NULL)
    return -1;
  while ((entry = readdir(dir)) != NULL)
  {
    if (entry->d_name[0] != '.')
      count++;
  }
  closedir(dir);
  return count;
}

int main(void)
{
  struct timespec ts;
  int before;
  int after;
  int i;

  if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    return 1;
  before = open_fd_count();
  if (before < 0)
    return 77;
  for (i = 0; i < 1000; i++)
  {
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
      return 1;
  }
  after = open_fd_count();
  if (after < 0)
    return 77;
  if (after != before)
  {
    fprintf(stderr, "file descriptors changed from %d to %d\n", before, after);
    return 1;
  }
  return 0;
}
