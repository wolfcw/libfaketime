#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  char *end;
  long expected;
  pid_t actual;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s expected-pid\n", argv[0]);
    return EXIT_FAILURE;
  }
  errno = 0;
  expected = strtol(argv[1], &end, 10);
  if (*end != '\0' || errno == ERANGE || expected < 0 || (pid_t)expected != expected)
  {
    fprintf(stderr, "invalid expected PID\n");
    return EXIT_FAILURE;
  }
  actual = getpid();
  if (actual != (pid_t)expected)
  {
    fprintf(stderr, "expected PID %ld, got %ld\n",
            expected, (long)actual);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
