#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

int main(int argc, char **argv)
{
  const char *path;
  struct timespec times[2] = {{1000, 0}, {1000, 0}};
  struct stat st;
  char *end;
  long long expected;

  if (argc != 3)
  {
    fprintf(stderr, "usage: %s path expected-seconds\n", argv[0]);
    return EXIT_FAILURE;
  }

  path = argv[1];
  expected = strtoll(argv[2], &end, 10);
  if (*end != '\0')
  {
    fprintf(stderr, "invalid expected timestamp\n");
    return EXIT_FAILURE;
  }

  if (utimensat(AT_FDCWD, path, times, 0) == -1)
  {
    perror("utimensat");
    return EXIT_FAILURE;
  }
  if (stat(path, &st) == -1)
  {
    perror("stat");
    return EXIT_FAILURE;
  }
  if ((long long)st.st_mtime != expected)
  {
    fprintf(stderr, "expected mtime %lld, got %lld\n",
            expected, (long long)st.st_mtime);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
