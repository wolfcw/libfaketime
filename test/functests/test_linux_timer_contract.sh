# Linux timerfd clock-domain contract tests.

init()
{
	typeset testsuite="$1"
	PLATFORM=$(platform)
	if [ -z "$PLATFORM" ]; then
		echo "$testsuite: unknown platform! quitting"
		return 1
	fi
	echo "# PLATFORM=$PLATFORM"
	return 0
}

run()
{
	init
	if [ "$PLATFORM" != "linuxlike" ]; then
		echo "out=skip timerfd is Linux-only - ok"
		return 0
	fi
	run_testcase expired_monotonic_timer_is_readable
}

expired_monotonic_timer_is_readable()
{
	typeset result
	result=$(fakecmd "+0" ./timerfd_contract_test)
	asserteq "$result" "expired monotonic timerfd became readable" \
		"expired monotonic timerfd should become readable"
}
