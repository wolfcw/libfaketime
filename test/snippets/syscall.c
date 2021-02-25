long uid = syscall(__NR_getuid);
printf("[%s] syscall(__NR_getuid) -> %ld\n", where, uid);
