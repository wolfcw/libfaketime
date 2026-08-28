# Shell-free spawn configuration tests.

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
	run_testcase executes_target_without_shell
	run_testcase rejects_mixed_spawn_modes
}

executes_target_without_shell()
{
	typeset marker=".spawn_exec_test.$$"
	rm -f "$marker"
	export FAKETIME_SPAWN_EXEC=/usr/bin/touch
	export FAKETIME_SPAWN_ARG_1="$marker"
	export FAKETIME_SPAWN_SECONDS=0
	export FAKETIME_NO_CACHE=1
	fakecmd "+0" perl -e 'print time' >/dev/null 2>&1
	typeset result=$?
	unset FAKETIME_SPAWN_EXEC FAKETIME_SPAWN_ARG_1 FAKETIME_SPAWN_SECONDS FAKETIME_NO_CACHE
	if [ "$result" -ne 0 ] || [ ! -e "$marker" ]; then
		echo "out=spawn target did not run - bad"
		rm -f "$marker"
		return 1
	fi
	rm -f "$marker"
	echo "out=spawn target ran without a shell - ok"
	return 0
}

rejects_mixed_spawn_modes()
{
	if FAKETIME_SPAWN_TARGET=/bin/true FAKETIME_SPAWN_EXEC=/bin/true \
		FAKETIME_NO_CACHE=1 fakecmd "+0" perl -e 'print time' >/dev/null 2>&1; then
		echo "out=0 mixed spawn modes were accepted - bad"
		return 1
	fi
	echo "out=1 mixed spawn modes were rejected - ok"
	return 0
}
