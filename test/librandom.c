#include <stdio.h>
#include <sys/random.h>

void func() {
  printf("  called func()\n");
}


static __attribute__((constructor)) void rnd_init() {
  unsigned int targ;
  ssize_t ret = getrandom(&targ, sizeof(targ), 0);
  if (ret == sizeof(targ)) {
    printf("  getrandom() yielded 0x%08x\n", targ);
  } else {
    printf("  getrandom() failed with only %zd\n", ret);
  }
}
