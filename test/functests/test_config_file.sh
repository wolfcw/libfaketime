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
	run_testcase tolerates_missing_config_environment
	run_testcase rejects_oversized_configuration
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

rejects_oversized_configuration()
{
	typeset config_file=".config_oversized_test.$$"
	typeset output
	perl -e 'print "+0" x 10000' > "$config_file"
	output=$(config_file_cmd "$config_file" perl -e 'print time' 2>&1 >/dev/null)
	rm -f "$config_file"
	case "$output" in
		*"configuration file is too long"*)
			echo "out=1 oversized configuration is rejected - ok"
			return 0
			;;
	esac
	echo "out=0 oversized configuration was accepted - bad"
	return 1
}

tolerates_missing_config_environment()
{
	if [ "$PLATFORM" = "mac" ]; then
		env -u HOME -u FAKETIME_TIMESTAMP_FILE -u FAKETIME \
			DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
			DYLD_FORCE_FLAT_NAMESPACE=1 perl -e 'print time' >/dev/null 2>&1
	else
		env -u HOME -u FAKETIME_TIMESTAMP_FILE -u FAKETIME \
			LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
			perl -e 'print time' >/dev/null 2>&1
	fi
	if [ "$?" -ne 0 ]; then
		echo "out=0 missing config environment caused failure - bad"
		return 1
	fi
	echo "out=1 missing config environment is tolerated - ok"
	return 0
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
