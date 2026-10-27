# Regression tests for faked file timestamps across the stat() family.

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
	if [ ! -x ./stat_entry_points_test ]; then
		echo "out=skip stat entry point test was not built - ok"
		return 0
	fi
	run_testcase stat_entry_points_fake_mtime
}

stat_entry_points_fake_mtime()
{
	typeset file=".stat-entry-points-test.$$"
	: > "$file"
	./set_mtime "$file" 1000000000 0
	if fakecmd "+1d" ./stat_entry_points_test "$file" 1000086400; then
		rm -f "$file"
		echo "out=0 stat entry points report the faked mtime - ok"
		return 0
	fi
	rm -f "$file"
	echo "out=1 stat entry points report the faked mtime - bad"
	return 1
}
