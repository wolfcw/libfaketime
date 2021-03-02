struct timespec *ts = malloc(sizeof(struct timespec));
clockid_t ckid = CLOCK_REALTIME;
int ret = clock_gettime(ckid, ts);
if (ret == 0) {
  printf("[%s] clock_gettime_heap(CLOCK_REALTIME[%d], ts) -> {%lld, %ld}\n", where, ckid, (long long)ts->tv_sec, ts->tv_nsec);
 } else {
  printf("[%s] clock_gettime_heap(CLOCK_REALTIME[%d], ts) returned non-zero (%d), errno = %d (%s)\n", where, ckid, ret, errno, strerror(errno));
 }
