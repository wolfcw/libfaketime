#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

static const char *walk_target;
static int walk_seen;

static int walk_callback(const char *path, const struct stat *st,
                         int type, struct FTW *state)
{
  (void)st;
  (void)type;
  (void)state;
  if (strcmp(path, walk_target) == 0)
    walk_seen = 1;
  return 0;
}

int main(int argc, char **argv)
{
  const char *path;
  struct timespec times[2] = {{1000, 0}, {1000, 0}};
  struct stat st;
  char link_path[4096];
  char *end;
  long long expected;
  int fd;

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
  if (lstat(path, &st) == -1)
  {
    perror("lstat");
    return EXIT_FAILURE;
  }
  if ((long long)st.st_mtime != expected)
  {
    fprintf(stderr, "expected lstat mtime %lld, got %lld\n",
            expected, (long long)st.st_mtime);
    return EXIT_FAILURE;
  }

  /* fstatat() must remain usable, but is not part of this opt-in timestamp
   * contract because its libc symbol path differs between platforms. */
  if (fstatat(AT_FDCWD, path, &st, 0) == -1)
  {
    perror("fstatat");
    return EXIT_FAILURE;
  }

  if (snprintf(link_path, sizeof(link_path), "%s.link", path) >=
      (int)sizeof(link_path) || symlink(path, link_path) == -1)
  {
    perror("symlink");
    return EXIT_FAILURE;
  }
  if (fstatat(AT_FDCWD, link_path, &st, AT_SYMLINK_NOFOLLOW) == -1 ||
      !S_ISLNK(st.st_mode))
  {
    perror("fstatat(AT_SYMLINK_NOFOLLOW)");
    unlink(link_path);
    return EXIT_FAILURE;
  }
  unlink(link_path);

  walk_target = ".";
  walk_seen = 0;
  if (nftw(".", walk_callback, 16, FTW_PHYS) == -1 || !walk_seen)
  {
    perror("nftw");
    return EXIT_FAILURE;
  }

  if (utimensat(AT_FDCWD, path, NULL, 0) == -1)
  {
    perror("utimensat(NULL)");
    return EXIT_FAILURE;
  }
  fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    perror("open");
    return EXIT_FAILURE;
  }
  if (futimens(fd, NULL) == -1)
  {
    perror("futimens(NULL)");
    close(fd);
    return EXIT_FAILURE;
  }
  close(fd);
  return EXIT_SUCCESS;
}
