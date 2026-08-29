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
	run_testcase rejects_overflowed_date_output
	run_testcase accepts_partial_date_format
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

rejects_overflowed_date_output()
{
	typeset date_helper=".date_overflow_test.$$"
	printf '#!/bin/sh\nprintf "999999999999999999999\\n"\n' > "$date_helper"
	chmod 755 "$date_helper"
	if ../src/faketime --date-prog "$date_helper" ignored true >/dev/null 2>&1; then
		echo "out=0 overflowed date output is rejected - bad"
		rm -f "$date_helper"
		return 1
	fi
	rm -f "$date_helper"
	echo "out=1 overflowed date output is rejected - ok"
	return 0
}

accepts_partial_date_format()
{
	typeset actual
	actual=$(FAKETIME_FMT=%Y fakecmd 2020 perl -e 'print time')
	if [ -z "$actual" ]; then
		echo "out=0 partial date format failed - bad"
		return 1
	fi
	echo "out=1 partial date format is handled safely - ok"
	return 0
}
