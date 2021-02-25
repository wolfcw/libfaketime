unsigned int targ;
if (getentropy(&targ, sizeof(targ)) == 0) {
  printf("[%s] getentropy() yielded 0x%08x\n", where, targ);
} else {
  printf("[%s] getentropy() failed with %d (%s)\n", where, errno, strerror(errno));
}
