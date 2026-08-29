# Regression tests for optional deterministic PID interception.

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
	if [ "$PLATFORM" = "mac" ]; then
		echo "out=skip FAKE_PID is unsupported on macOS - ok"
		return 0
	fi
	if [ ! -x ./fake_pid_contract_test ]; then
		echo "out=skip FAKE_PID is not enabled - ok"
		return 0
	fi
	run_testcase valid_fake_pid
	run_testcase invalid_fake_pid
}

valid_fake_pid()
{
	export FAKETIME_FAKEPID=4242
	if fakecmd "+0" ./fake_pid_contract_test 4242; then
		unset FAKETIME_FAKEPID
		echo "out=4242 valid fake PID is returned - ok"
		return 0
	fi
	unset FAKETIME_FAKEPID
	echo "out=invalid valid fake PID was rejected - bad"
	return 1
}

invalid_fake_pid()
{
	export FAKETIME_FAKEPID=not-a-pid
	if fakecmd "+0" ./fake_pid_test >/dev/null 2>&1; then
		unset FAKETIME_FAKEPID
		echo "out=0 invalid fake PID was accepted - bad"
		return 1
	fi
	unset FAKETIME_FAKEPID
	echo "out=1 invalid fake PID is rejected - ok"
	return 0
}
