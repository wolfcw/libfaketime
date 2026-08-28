#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
  pid_t result;

  result = getpid();
  if (result != (pid_t)-1 || errno != EINVAL)
  {
    fprintf(stderr, "invalid fake pid returned %ld with errno %d\n",
            (long)result, errno);
    return EXIT_FAILURE;
  }

  puts("invalid fake pid rejected");
  return EXIT_SUCCESS;
}
