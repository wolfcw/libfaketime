# Exercise shared-state creation, inheritance, and teardown across platforms.

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
	run_testcase repeated_shared_state
	run_testcase fork_exec_inherits_shared_state
}

repeated_shared_state()
{
	typeset iteration actual
	for iteration in $(range 1 10); do
		actual=$(fakecmd "2020-06-15 12:00:00" perl -MPOSIX -e \
			'print strftime("%Y-%m-%d", gmtime(time))')
		if [ "$actual" != "2020-06-15" ]; then
			echo "out=$actual repeated shared-state run $iteration - bad"
			return 1
		fi
	done
	echo "out=10 repeated shared-state runs completed - ok"
	return 0
}

fork_exec_inherits_shared_state()
{
	typeset actual
	actual=$(fakecmd "2020-06-15 12:00:00" perl -MPOSIX -e \
		'my $pid = fork(); die "fork failed\\n" unless defined $pid; if (!$pid) { exec("perl", "-MPOSIX", "-e", q{print strftime("%Y", gmtime(time))}) or die "exec failed\\n"; } waitpid($pid, 0);')
	asserteq "$actual" "2020" \
		"forked and execed child should inherit shared state"
}
