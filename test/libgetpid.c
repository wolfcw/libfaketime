#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

void getpid_func() {
  fprintf(stderr, "  called getpid_func()\n");
}


static __attribute__((constructor)) void getpid_init() {
  pid_t pid = getpid();
  fprintf(stderr, "  getpid() yielded %d\n", pid);
}
