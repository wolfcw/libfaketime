#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct test_state {
  atomic_bool stop;
  atomic_bool failed;
  atomic_uint samples;
};

static int compare_timespec(const struct timespec *left, const struct timespec *right)
{
  if (left->tv_sec != right->tv_sec)
    return left->tv_sec < right->tv_sec ? -1 : 1;
  if (left->tv_nsec != right->tv_nsec)
    return left->tv_nsec < right->tv_nsec ? -1 : 1;
  return 0;
}

static void *reader(void *opaque)
{
  struct test_state *state = opaque;
  struct timespec previous = {0, 0};
  bool have_previous = false;

  while (!atomic_load_explicit(&state->stop, memory_order_relaxed))
  {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0 || now.tv_nsec < 0 ||
        now.tv_nsec >= 1000000000L ||
        (have_previous && compare_timespec(&now, &previous) < 0))
    {
      atomic_store_explicit(&state->failed, true, memory_order_relaxed);
      break;
    }
    previous = now;
    have_previous = true;
    atomic_fetch_add_explicit(&state->samples, 1, memory_order_relaxed);
  }
  return NULL;
}

static int replace_config(const char *path, const char *contents)
{
  char temporary[4096];
  int fd;
  size_t length = strlen(contents);

  if (snprintf(temporary, sizeof(temporary), "%s.new", path) >= (int)sizeof(temporary))
    return -1;
  fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    return -1;
  if (write(fd, contents, length) != (ssize_t)length || fsync(fd) != 0 || close(fd) != 0)
  {
    int saved_errno = errno;
    close(fd);
    unlink(temporary);
    errno = saved_errno;
    return -1;
  }
  if (rename(temporary, path) != 0)
  {
    int saved_errno = errno;
    unlink(temporary);
    errno = saved_errno;
    return -1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  static const char *const configurations[] = {"+0 x1\n", "+0 x4\n", "+0 x0.5\n"};
  enum { reader_count = 4, transition_count = 48 };
  struct test_state state = {0};
  pthread_t readers[reader_count];
  struct timespec pause = {0, 1000000L};
  unsigned int i;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s configuration-file\n", argv[0]);
    return EXIT_FAILURE;
  }
  for (i = 0; i < reader_count; ++i)
    if (pthread_create(&readers[i], NULL, reader, &state) != 0)
    {
      fprintf(stderr, "pthread_create failed\n");
      atomic_store(&state.stop, true);
      while (i > 0)
        pthread_join(readers[--i], NULL);
      return EXIT_FAILURE;
    }

  for (i = 0; i < transition_count; ++i)
  {
    if (replace_config(argv[1], configurations[i % 3]) != 0)
    {
      perror("replace configuration");
      atomic_store(&state.failed, true);
      break;
    }
    nanosleep(&pause, NULL);
  }
  atomic_store(&state.stop, true);
  for (i = 0; i < reader_count; ++i)
    pthread_join(readers[i], NULL);

  /* The interposed nanosleep is deliberately rate-scaled, so the amount of
   * reader work varies by platform and configuration.  Every reader must have
   * run, but no fixed scheduling-dependent total is part of the contract. */
  if (atomic_load(&state.failed) || atomic_load(&state.samples) < reader_count)
  {
    fprintf(stderr, "rate-transition contract failed (%u samples)\n",
            atomic_load(&state.samples));
    return EXIT_FAILURE;
  }
  printf("rate-transition contract passed (%u samples)\n", atomic_load(&state.samples));
  return EXIT_SUCCESS;
}
