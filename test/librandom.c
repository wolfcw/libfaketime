#include <stdio.h>
#include <sys/random.h>

void func() {
  fprintf(stderr, "  called func()\n");
}


static __attribute__((constructor)) void rnd_init() {
  unsigned int targ;
  ssize_t ret = getrandom(&targ, sizeof(targ), 0);
  if (ret == sizeof(targ)) {
    fprintf(stderr, "  getrandom() yielded 0x%08x\n", targ);
  } else {
    fprintf(stderr, "  getrandom() failed with only %zd\n", ret);
  }
}
