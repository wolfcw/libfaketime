# Verify that shared memory works end-to-end across processes.
# The faketime wrapper creates SHM with a known FAKETIME value,
# and the child process (via LD_PRELOAD) reads it and reports
# the faked time.

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

	run_testcase shm_year_check
}

shm_year_check()
{
	typeset expected="2020"
	typeset actual
	actual=$(fakecmd "2020-06-15 12:00:00" date -u +%Y)
	asserteq "$actual" "$expected" "child process should see faked year via SHM"
}
