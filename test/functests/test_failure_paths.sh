# Regression tests for wrapper and timestamp-save failure handling.

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
	run_testcase rejects_missing_pid
	run_testcase rejects_missing_date_program
	run_testcase save_file_failure_does_not_hang
}

assert_command_fails()
{
	typeset description="$1"
	shift
	if "$@" >/dev/null 2>&1; then
		echo "out=0 $description - bad"
		return 1
	else
		echo "out=1 $description - ok"
		return 0
	fi
}

rejects_missing_pid()
{
	assert_command_fails "missing -p argument is rejected" ../src/faketime -p
}

rejects_missing_date_program()
{
	assert_command_fails "missing --date-prog argument is rejected" ../src/faketime --date-prog
}

save_file_failure_does_not_hang()
{
	if [ "$PLATFORM" != "linuxlike" ]; then
		echo "out=skip timestamp save failure requires Linux /dev/full - ok"
		return 0
	fi

	typeset output status
	output=$(timeout 2s env FAKETIME="+0" FAKETIME_SAVE_FILE=/dev/full \
		LD_PRELOAD=../src/libfaketime.so.1 date +%s 2>&1)
	status=$?
	if [ "$status" -eq 124 ]; then
		echo "out=124 timestamp save failure timed out - bad"
		return 1
	fi
	echo "out=$status timestamp save failure completed without hanging - ok"
	return 0
}
