#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void)
{
  long direct_uid;

  errno = 0;
  direct_uid = syscall(__NR_getuid);
  if (direct_uid != (long)getuid())
  {
    fprintf(stderr, "syscall(getuid) returned %ld, getuid returned %ld\n",
            direct_uid, (long)getuid());
    return EXIT_FAILURE;
  }

  errno = 0;
  if (syscall(-1) != -1 || errno != ENOSYS)
  {
    fprintf(stderr, "invalid syscall did not return ENOSYS (errno=%d)\n", errno);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
