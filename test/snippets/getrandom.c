unsigned int targ;
ssize_t ret = getrandom(&targ, sizeof(targ), 0);
if (ret == sizeof(targ)) {
  printf("[%s] getrandom() yielded 0x%08x\n", where, targ);
} else {
  printf("[%s] getrandom() failed with only %zd\n", where, ret);
}
