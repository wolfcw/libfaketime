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
	run_testcase rejects_empty_load_file
	run_testcase rejects_partial_load_file
	run_testcase save_file_failure_does_not_hang
	rm -f "$LOAD_FILE"
}

LOAD_FILE=".load_file_failure_test.$$"

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

rejects_empty_load_file()
{
	: > "$LOAD_FILE"
	if run_with_load_file; then
		echo "out=0 empty load file is rejected - bad"
		return 1
	fi
	echo "out=1 empty load file is rejected - ok"
	return 0
}

rejects_partial_load_file()
{
	printf 'invalid' > "$LOAD_FILE"
	if run_with_load_file; then
		echo "out=0 partial load file is rejected - bad"
		return 1
	fi
	echo "out=1 partial load file is rejected - ok"
	return 0
}

run_with_load_file()
{
	export FAKETIME_LOAD_FILE="$LOAD_FILE"
	export FAKETIME_NO_CACHE=1
	fakecmd "+0" ../timetest >/dev/null 2>&1
	typeset status=$?
	unset FAKETIME_LOAD_FILE FAKETIME_NO_CACHE
	return $status
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
