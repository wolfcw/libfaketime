# Regression tests for Linux syscall interception.

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
		echo "out=skip syscall interception is Linux-only - ok"
		return 0
	fi
	if [ ! -x ./syscall_contract_test ]; then
		echo "out=skip INTERCEPT_SYSCALL is not enabled - ok"
		return 0
	fi
	run_testcase syscall_contract
}

syscall_contract()
{
	if linuxlike_fakecmd "+0" ./syscall_contract_test; then
		echo "out=ok intercepted syscall contract passed - ok"
		return 0
	fi
	echo "out=failed intercepted syscall contract failed - bad"
	return 1
}
