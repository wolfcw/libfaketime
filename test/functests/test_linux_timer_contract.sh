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
	run_testcase absolute_monotonic_timer_uses_clock_domain
	run_testcase relative_monotonic_timer_uses_duration
	if [ -x ./futex_contract_test ]; then
		run_testcase futex_deadline_contract
	else
		echo "out=skip futex interception is not enabled - ok"
	fi
}

absolute_monotonic_timer_uses_clock_domain()
{
	typeset result
	result=$(fakecmd "+1d" ./timerfd_contract_test absolute)
	asserteq "$result" "absolute monotonic timerfd deadline honored" \
		"absolute monotonic timerfd deadline should use its clock domain"
}

expired_monotonic_timer_is_readable()
{
	typeset result
	result=$(fakecmd "+0" ./timerfd_contract_test)
	asserteq "$result" "expired monotonic timerfd became readable" \
		"expired monotonic timerfd should become readable"
}

relative_monotonic_timer_uses_duration()
{
	typeset result
	result=$(fakecmd "+0 x2" ./timerfd_contract_test relative)
	asserteq "$result" "relative monotonic timerfd deadline honored" \
		"relative monotonic timerfd deadline should use its duration"
}

futex_deadline_contract()
{
	typeset result
	result=$(timeout 5s env \
		LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
		FAKETIME_DONT_FAKE_MONOTONIC=1 \
		FAKETIME="+1d x2" ./futex_contract_test)
	asserteq "$result" "relative and absolute futex timeout contracts passed" \
		"relative and absolute futex deadlines should use their clock contracts"
}
