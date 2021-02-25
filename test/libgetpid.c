#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

void getpid_func() {
  printf("  called getpid_func()\n");
}


static __attribute__((constructor)) void getpid_init() {
  pid_t pid = getpid();
  printf("  getpid() yielded %d\n", pid);
}
