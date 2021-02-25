#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main() {
  int d = 0;
  long r = syscall(__NR_getrandom, &d, sizeof(d), 0);
  printf("getrandom(%d, <ptr>, %zd, 0) returned %ld and yielded 0x%08x\n",
         __NR_getrandom, sizeof(d), r, d);
  return 0;
}
