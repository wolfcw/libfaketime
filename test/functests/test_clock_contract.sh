# Cross-platform checks for the basic clock and wait contract.

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
	run_testcase realtime_absolute_value
	run_testcase realtime_negative_fraction
	run_testcase monotonic_is_non_decreasing
	run_testcase sleep_returns
	run_testcase invalid_clock_id_fails
	run_testcase null_clock_output_fails
}

realtime_absolute_value()
{
	typeset actual
	actual=$(fakecmd "2020-06-15 12:00:00" perl -MPOSIX -e \
		'print strftime("%Y-%m-%d %H:%M:%S", localtime(time))')
	asserteq "$actual" "2020-06-15 12:00:00" \
		"realtime clock should honor absolute configuration"
}

realtime_negative_fraction()
{
	typeset actual
	actual=$(fakecmd "@2020-01-01 00:00:00.750000000" perl -MPOSIX -e \
		'my @local = localtime(time); print $local[5] + 1900')
	asserteq "$actual" "2020" \
		"fractional realtime values should retain the calendar second"
}

monotonic_is_non_decreasing()
{
	typeset result
	result=$(fakecmd "+0" perl -MTime::HiRes=clock_gettime,CLOCK_MONOTONIC -e \
		'my $a = clock_gettime(CLOCK_MONOTONIC); my $b = clock_gettime(CLOCK_MONOTONIC); print($b >= $a ? "ok" : "bad")')
	asserteq "$result" "ok" "monotonic clock should not move backwards"
}

sleep_returns()
{
	typeset result
	result=$(fakecmd "+0" perl -e 'select undef, undef, undef, 0.01; print "ok"')
	asserteq "$result" "ok" "short relative sleep should return"
}

invalid_clock_id_fails()
{
	typeset result
	result=$(fakecmd "+0" perl -MPOSIX -e \
		'my $ok = !eval { clock_gettime(-1); 1 }; print($ok ? "ok" : "bad")')
	asserteq "$result" "ok" "invalid clock id should fail"
}

null_clock_output_fails()
{
	typeset result
	result=$(fakecmd "+0" ./clock_error_test)
	asserteq "$result" "clock_gettime(NULL) returned EFAULT" \
		"null clock output should fail with EFAULT"
}
