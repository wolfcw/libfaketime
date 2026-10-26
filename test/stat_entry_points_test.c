#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

static void check(const char *name, int result, long long mtime,
                  long long expected)
{
  if (result == -1)
  {
    perror(name);
    failures++;
  }
  else if (mtime != expected)
  {
    fprintf(stderr, "%s: expected mtime %lld, got %lld\n",
            name, expected, mtime);
    failures++;
  }
}

int main(int argc, char **argv)
{
  const char *path;
  struct stat st;
#ifdef __GLIBC__
  struct stat64 st64;
#endif
  char *end;
  long long expected;
  int fd;
  int result;

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

  fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    perror("open");
    return EXIT_FAILURE;
  }

  result = stat(path, &st);
  check("stat", result, st.st_mtime, expected);
  result = lstat(path, &st);
  check("lstat", result, st.st_mtime, expected);
  result = fstat(fd, &st);
  check("fstat", result, st.st_mtime, expected);
#ifdef __GLIBC__
  /* glibc >= 2.33 exports these as regular functions (issue #477) */
  result = fstatat(AT_FDCWD, path, &st, 0);
  check("fstatat", result, st.st_mtime, expected);
  result = stat64(path, &st64);
  check("stat64", result, st64.st_mtime, expected);
  result = lstat64(path, &st64);
  check("lstat64", result, st64.st_mtime, expected);
  result = fstat64(fd, &st64);
  check("fstat64", result, st64.st_mtime, expected);
  result = fstatat64(AT_FDCWD, path, &st64, 0);
  check("fstatat64", result, st64.st_mtime, expected);
#endif

  close(fd);
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
