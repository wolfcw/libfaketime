# Regression tests for checked numeric configuration parsing.

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
	run_testcase rejects_invalid_cache_duration
	run_testcase rejects_invalid_start_limit
	run_testcase rejects_invalid_offset
	run_testcase rejects_nonfinite_rate
}

assert_faked_command_fails()
{
	typeset description="$1"
	shift
	if "$@" >/dev/null 2>&1; then
		echo "out=0 $description - bad"
		return 1
	fi
	echo "out=1 $description - ok"
	return 0
}

probe_command()
{
	fakecmd "+0" perl -e 'print time'
}

rejects_invalid_cache_duration()
{
	assert_faked_command_fails "invalid cache duration is rejected" invalid_cache
}

rejects_invalid_start_limit()
{
	assert_faked_command_fails "invalid start limit is rejected" invalid_start_limit
}

invalid_cache()
{
	FAKETIME_CACHE_DURATION=invalid probe_command
}

invalid_start_limit()
{
	FAKETIME_START_AFTER_SECONDS=invalid probe_command
}

rejects_invalid_offset()
{
	assert_faked_command_fails "invalid time offset is rejected" \
		fakecmd invalid perl -e 'print time'
}

rejects_nonfinite_rate()
{
	assert_faked_command_fails "non-finite clock rate is rejected" \
		fakecmd xnan perl -e 'print time'
}
