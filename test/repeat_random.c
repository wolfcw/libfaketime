#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>

void usage(const char* name) {
  fprintf(stderr,
          "Usage: %s REPS SIZE\n\n"
          "Gather and print REPS blocks of SIZE bytes from getrandom()\n",
          name);
}

int main(int argc, const char **argv) {
  int reps, size;
  unsigned char *buf;
  if (argc != 3) {
    usage(argv[0]);
    return 1;
  }
  reps = atoi(argv[1]);
  size = atoi(argv[2]);
  buf = malloc(size);
  if (!buf) {
    fprintf(stderr, "failure to allocate buffer of size %d\n", size);
    return 1;
  }
  for (int i = 0; i < reps; i++) {
    ssize_t resp = getrandom(buf, size, 0);
    if (resp != size) {
      fprintf(stderr, "tried to get %d bytes, got %zd\n", size, resp);
      free(buf);
      return 2;
    }
    for (int j = 0; j < size; j++) {
      printf("%02x", buf[j]);
    }
  }
  free(buf);
  printf("\n");
  return 0;
};
