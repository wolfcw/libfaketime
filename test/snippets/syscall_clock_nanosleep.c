/* Test raw syscall(__NR_clock_nanosleep): relative and TIMER_ABSTIME */
#ifdef __NR_clock_nanosleep
	struct timespec start_fake, end_fake_rel, end_fake_abs;
	struct timespec req_rel = {0, 200 * 1000 * 1000}; /* 200ms fake sleep */
	struct timespec req_abs;
	long ret;

	/* Capture starting time (fake view) */
	clock_gettime(CLOCK_REALTIME, &start_fake);

	/* Relative sleep via syscall */
	ret = syscall(__NR_clock_nanosleep, CLOCK_REALTIME, 0, &req_rel, NULL);
	clock_gettime(CLOCK_REALTIME, &end_fake_rel);

	/* Absolute sleep target: 300ms after start_fake *./test_variable_data.sh */
	req_abs.tv_sec = start_fake.tv_sec;
	req_abs.tv_nsec = start_fake.tv_nsec + 300 * 1000 * 1000;
	if (req_abs.tv_nsec >= 1000000000) { req_abs.tv_sec += 1; req_abs.tv_nsec -= 1000000000; }

	/* Absolute sleep via syscall */
	ret = syscall(__NR_clock_nanosleep, CLOCK_REALTIME, TIMER_ABSTIME, &req_abs, NULL);
	clock_gettime(CLOCK_REALTIME, &end_fake_abs);

	/* Report durations (fake view). */
	long rel_ns = (end_fake_rel.tv_sec - start_fake.tv_sec) * 1000000000L + (end_fake_rel.tv_nsec - start_fake.tv_nsec);
	long abs_ns = (end_fake_abs.tv_sec - start_fake.tv_sec) * 1000000000L + (end_fake_abs.tv_nsec - start_fake.tv_nsec);
	printf("[%s] syscall(clock_nanosleep) relative 200ms -> ~%ld ns, absolute +300ms -> ~%ld ns\n", where, rel_ns, abs_ns);
    return ret;
#else
	printf("[%s] __NR_clock_nanosleep not defined on this platform\n", where);
#endif

