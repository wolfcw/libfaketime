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
	run_testcase rejects_invalid_load_timestamp
	run_testcase rejects_invalid_shared_objects
	run_testcase rejects_overlong_date_output
	if [ -n "${FAKETIME_TESTLIB:-}" ]; then
		echo "out=skip sanitizer-instrumented helper process checks are unavailable - ok"
		echo "out=skip sanitizer-instrumented constructor reentry check is unavailable - ok"
	else
		run_testcase isolates_date_helper_preload
		run_testcase tolerates_constructor_reentry
	fi
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

rejects_invalid_load_timestamp()
{
	perl -e 'print pack("Q>Q>", 0, 1000000000)' > "$LOAD_FILE"
	if run_with_load_file; then
		echo "out=0 invalid load timestamp is rejected - bad"
		return 1
	fi
	echo "out=1 invalid load timestamp is rejected - ok"
	return 0
}

rejects_invalid_shared_objects()
{
	export FAKETIME_SHARED="/tmp/not-a-semaphore /tmp/not-shared-memory"
	export FAKETIME_NO_CACHE=1
	if fakecmd "+0" ../timetest >/dev/null 2>&1; then
		echo "out=0 invalid shared-object names are rejected - bad"
		unset FAKETIME_SHARED FAKETIME_NO_CACHE
		return 1
	fi
	unset FAKETIME_SHARED FAKETIME_NO_CACHE
	echo "out=1 invalid shared-object names are rejected - ok"
	return 0
}

rejects_overlong_date_output()
{
	typeset date_helper=".date_overlong_test.$$"
	printf '#!/bin/sh\nperl -e '\''print "123"; print " " x 300'\''\n' > "$date_helper"
	chmod 755 "$date_helper"
	if ../src/faketime --date-prog "$date_helper" ignored true >/dev/null 2>&1; then
		echo "out=0 overlong date output is accepted - bad"
		rm -f "$date_helper"
		return 1
	fi
	rm -f "$date_helper"
	echo "out=1 overlong date output is rejected - ok"
	return 0
}

isolates_date_helper_preload()
{
	typeset date_helper=".date_preload_test.$$"
	typeset environment_file=".date_preload_environment.$$"
	printf '#!/bin/sh\nprintf "%%s\\n%%s\\n" "${LD_PRELOAD:-}" "${DYLD_INSERT_LIBRARIES:-}" > "$DATE_HELPER_ENV_FILE"\nprintf "0\\n"\n' > "$date_helper"
	chmod 755 "$date_helper"
	if [ "$PLATFORM" = "mac" ]; then
		env DATE_HELPER_ENV_FILE="$environment_file" \
			DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
			DYLD_FORCE_FLAT_NAMESPACE=1 \
			../src/faketime --date-prog "./$date_helper" ignored \
				./date-helper-target-does-not-exist >/dev/null 2>&1
	else
		env DATE_HELPER_ENV_FILE="$environment_file" \
			LD_PRELOAD=../src/libfaketime.so.1 \
			../src/faketime --date-prog "./$date_helper" ignored \
				./date-helper-target-does-not-exist >/dev/null 2>&1
	fi
	typeset status=$?
	typeset helper_environment=""
	if [ -f "$environment_file" ]; then
		helper_environment=$(cat "$environment_file")
	fi
	if [ "$status" -eq 0 ] || [ ! -f "$environment_file" ] || [ -n "$helper_environment" ]; then
		rm -f "$date_helper" "$environment_file"
		echo "out=$status date helper preload environment was not isolated ($helper_environment) - bad"
		return 1
	fi
	rm -f "$date_helper" "$environment_file"
	echo "out=0 date helper preload environment was isolated - ok"
	return 0
}

tolerates_constructor_reentry()
{
	if [ "$PLATFORM" = "mac" ]; then
		typeset reentry_lib=./init_reentry.dylib
		typeset faketime_lib=../src/libfaketime.1.dylib
		typeset preload_variable=DYLD_INSERT_LIBRARIES
	else
		typeset reentry_lib=./init_reentry.so
		typeset faketime_lib=../src/libfaketime.so.1
		typeset preload_variable=LD_PRELOAD
	fi
	if [ ! -f "$reentry_lib" ]; then
		echo "out=skip constructor reentry test helper is unavailable - ok"
		return 0
	fi
	typeset status
	if [ "$PLATFORM" = "mac" ]; then
		env "$preload_variable=$reentry_lib:$faketime_lib" \
			DYLD_FORCE_FLAT_NAMESPACE=1 \
			FAKETIME_NO_CACHE=1 perl -e 'alarm 3; exec @ARGV or exit 127' -- \
			/usr/bin/true >/dev/null 2>&1
	else
		env "$preload_variable=$reentry_lib:$faketime_lib" \
			FAKETIME_NO_CACHE=1 perl -e 'alarm 3; exec @ARGV or exit 127' -- \
			/bin/true >/dev/null 2>&1
	fi
	status=$?
	if [ "$status" -ne 0 ]; then
		echo "out=$status constructor reentry did not complete - bad"
		return 1
	fi
	echo "out=0 constructor reentry completed - ok"
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
