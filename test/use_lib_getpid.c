#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "libgetpid.h"

int main() {
  pid_t pid;
  getpid_func();
  pid = getpid();
  fprintf(stderr, " getpid() -> %d\n", pid);
  return 0;
}
