struct timespec ts;
clockid_t ckid = CLOCK_REALTIME;
long ret = syscall(__NR_clock_gettime, ckid, &ts);
if (ret == 0)
  printf("[%s] syscall(__NR_gettime, CLOCK_REALTIME[%d], &ts) -> {%lld, %ld}\n", where, ckid, (long long)ts.tv_sec, ts.tv_nsec);
else
  printf("[%s] syscall(__NR_gettime, CLOCK_REALTIME[%d], &ts) returned non-zero (%ld)\n", where, ckid, ret);
   
