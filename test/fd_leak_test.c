#define _GNU_SOURCE

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

static int faketime_shm_fd_count(void)
{
  DIR *dir = opendir("/proc/self/fd");
  struct dirent *entry;
  int count = 0;

  if (dir == NULL)
    return -1;
  while ((entry = readdir(dir)) != NULL)
  {
    char path[64];
    char target[512];
    ssize_t length;

    if (entry->d_name[0] == '.')
      continue;
    if (snprintf(path, sizeof(path), "/proc/self/fd/%s", entry->d_name) >=
        (int)sizeof(path))
      continue;
    length = readlink(path, target, sizeof(target) - 1);
    if (length < 0)
      continue;
    target[length] = '\0';
    if (strstr(target, "faketime_shm_") != NULL)
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
  int shm_fds;
  int i;

  if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    return 1;
  shm_fds = faketime_shm_fd_count();
  if (shm_fds < 0)
    return 77;
  if (shm_fds != 0)
  {
    fprintf(stderr, "found %d leaked faketime shared-memory descriptors\n",
            shm_fds);
    return 1;
  }
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
