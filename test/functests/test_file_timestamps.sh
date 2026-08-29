# Regression tests for optional file timestamp interception.

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
	if [ ! -x ./file_timestamp_test ]; then
		echo "out=skip FAKE_FILE_TIMESTAMPS is not enabled - ok"
		return 0
	fi
	run_testcase fake_utimensat_and_stat
}

fake_utimensat_and_stat()
{
	typeset file=".file-timestamp-test.$$"
	: > "$file"
	if FAKE_UTIME=1 fakecmd "+1d" ./file_timestamp_test "$file" 1000; then
		rm -f "$file"
		echo "out=0 utimensat and stat preserve requested virtual timestamp - ok"
		return 0
	fi
	rm -f "$file"
	echo "out=1 utimensat and stat preserve requested virtual timestamp - bad"
	return 1
}
