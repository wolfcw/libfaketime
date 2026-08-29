# Cross-check the primary realtime clock APIs under absolute and fractional time.

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
	run_testcase absolute_clock_apis_agree
	run_testcase fractional_clock_apis_agree
}

absolute_clock_apis_agree()
{
	typeset result
	result=$(fakecmd "2020-06-15 12:00:00" ./clock_consistency_test)
	asserteq "$result" "clock APIs agreed" \
		"time, gettimeofday, and clock_gettime should agree"
}

fractional_clock_apis_agree()
{
	typeset result
	result=$(fakecmd "@2020-06-15 12:00:00.123456789" ./clock_consistency_test)
	asserteq "$result" "clock APIs agreed" \
		"realtime APIs should preserve valid fractional timestamps"
}
