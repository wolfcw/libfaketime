#define _GNU_SOURCE
#include <errno.h>
#include <linux/futex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void)
{
  uint32_t value = 0;
  uint32_t value2 = 0;

  errno = 0;
  if (syscall(SYS_futex, &value, FUTEX_REQUEUE, 0, 1, &value2) < 0)
  {
    perror("FUTEX_REQUEUE");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
