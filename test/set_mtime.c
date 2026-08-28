#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
  struct timespec times[2];
  char *end;

  if (argc != 4)
  {
    fprintf(stderr, "usage: %s path seconds nanoseconds\n", argv[0]);
    return EXIT_FAILURE;
  }

  times[0].tv_sec = strtoll(argv[2], &end, 10);
  if (*end != '\0') return EXIT_FAILURE;
  times[0].tv_nsec = strtol(argv[3], &end, 10);
  if (*end != '\0' || times[0].tv_nsec < 0 || times[0].tv_nsec >= 1000000000L)
    return EXIT_FAILURE;
  times[1] = times[0];

  if (utimensat(AT_FDCWD, argv[1], times, 0) == -1)
  {
    perror("utimensat");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
