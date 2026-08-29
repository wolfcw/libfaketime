# Regression tests for timestamp configuration files.

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
	run_testcase accepts_crlf_and_full_line_comments
}

config_file_cmd()
{
	if [ "$PLATFORM" = "mac" ]; then
		FAKETIME_TIMESTAMP_FILE="$1" FAKETIME_NO_CACHE=1 \
			DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
			DYLD_FORCE_FLAT_NAMESPACE=1 "$2" "${@:3}"
	else
		FAKETIME_TIMESTAMP_FILE="$1" FAKETIME_NO_CACHE=1 \
			LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" "$2" "${@:3}"
	fi
}

accepts_crlf_and_full_line_comments()
{
	typeset config_file=".config_comment_test.$$"
	typeset actual
	printf '# ignored comment\r\n; also ignored\r\n2020-06-15 12:00:00\r\n' > "$config_file"
	actual=$(config_file_cmd "$config_file" perl -MPOSIX -e \
		'print strftime("%Y-%m-%d %H:%M:%S", localtime(time))')
	rm -f "$config_file"
	asserteq "$actual" "2020-06-15 12:00:00" \
		"CRLF config files and full-line comments should be accepted"
}
