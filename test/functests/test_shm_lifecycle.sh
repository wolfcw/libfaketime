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
	run_testcase recreates_stale_shared_state
	run_testcase save_and_load_resources
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

recreates_stale_shared_state()
{
	typeset actual
	actual=$(FAKETIME_SHARED="/faketime_sem_999999999 /faketime_shm_999999999" \
		FAKETIME_NO_CACHE=1 fakecmd "2020-06-15 12:00:00" perl -MPOSIX -e \
		'print strftime("%Y-%m-%d", gmtime(time))')
	if [ "$actual" != "2020-06-15" ]; then
		echo "out=$actual stale shared state was not recreated - bad"
		return 1
	fi
	echo "out=$actual stale shared state was recreated - ok"
	return 0
}

save_and_load_resources()
{
	if [ "$PLATFORM" != "linuxlike" ]; then
		echo "out=skip timestamp save/load resources are Linux-only - ok"
		return 0
	fi

	typeset save_file load_file actual
	save_file=".save_resource_test.$$"
	load_file=".load_resource_test.$$"
	actual=$(FAKETIME_SAVE_FILE="$save_file" fakecmd "+0" perl -e 'print time for 1..3')
	if [ "${#actual}" -ne 30 ] || [ ! -s "$save_file" ]; then
		echo "out=save failed to create timestamp resource - bad"
		rm -f "$save_file" "$load_file"
		return 1
	fi
	if [ $(( $(wc -c < "$save_file") % 16 )) -ne 0 ]; then
		echo "out=save produced a partial timestamp record - bad"
		rm -f "$save_file" "$load_file"
		return 1
	fi
	cp "$save_file" "$load_file"
	actual=$(FAKETIME_LOAD_FILE="$load_file" fakecmd "+0" perl -e 'print time')
	rm -f "$save_file" "$load_file"
	if [ -z "$actual" ]; then
		echo "out=load failed to consume timestamp resource - bad"
		return 1
	fi
	echo "out=save/load timestamp resources completed - ok"
	return 0
}
