# Tests for FAKETIME_FOLLOW_ABSOLUTE feature.
#
# When FAKETIME_FOLLOW_ABSOLUTE=1 is set alongside FAKETIME="%" and
# FAKETIME_FOLLOW_FILE, time freezes at the follow file's mtime
# and only advances when the file's mtime changes.

FOLLOW_FILE=".follow_absolute_test_file"

prepare_follow_file()
{
	: > "$FOLLOW_FILE"
}

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

	run_testcase follow_absolute_basic
	run_testcase follow_absolute_freeze
	run_testcase follow_absolute_tracks_mtime

	rm -f "$FOLLOW_FILE"
}

# Helper to run a command with follow-absolute configuration
follow_absolute_cmd()
{
	FAKETIME_FOLLOW_FILE="$FOLLOW_FILE" \
	FAKETIME_FOLLOW_ABSOLUTE=1 \
	FAKETIME_NO_CACHE=1 \
	fakecmd "%" perl -MPOSIX -e \
		'print strftime("%Y-%m-%d %H:%M:%S", gmtime(time)), "\n"'
}

# Test that time matches the follow file's mtime
follow_absolute_basic()
{
	prepare_follow_file
	./set_mtime "$FOLLOW_FILE" 1584268200 0
	typeset actual
	actual=$(follow_absolute_cmd)
	asserteq "$actual" "2020-03-15 10:30:00" \
		"time should match follow file mtime"
}

# Test that time stays frozen (does not advance with real time)
follow_absolute_freeze()
{
	prepare_follow_file
	./set_mtime "$FOLLOW_FILE" 1584268200 0
	typeset timestamps
	timestamps=$(follow_absolute_cmd \
		perl -e 'print time(), "\n"; sleep(2); print time(), "\n"')
	typeset first second
	first=$(echo "$timestamps" | head -1)
	second=$(echo "$timestamps" | tail -1)
	asserteq "$first" "$second" \
		"time should stay frozen within a single process"
}

# Test that time tracks file mtime changes across portable filesystem precision
follow_absolute_tracks_mtime()
{
	prepare_follow_file
	./set_mtime "$FOLLOW_FILE" 1584268200 0
	typeset first
	first=$(follow_absolute_cmd)

	./set_mtime "$FOLLOW_FILE" 1584268201 0
	typeset second
	second=$(follow_absolute_cmd)

	assertneq "$first" "$second" \
		"time should advance with file mtime"
}
