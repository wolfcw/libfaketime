# Verify that a child process receives the faked time on macOS without
# relying on SIP-protected system binaries.

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
	actual=$(fakecmd "2020-06-15 12:00:00" perl -MPOSIX -e \
		'print strftime("%Y", gmtime(time))')
	asserteq "$actual" "$expected" "child process should see faked year via SHM"
}
