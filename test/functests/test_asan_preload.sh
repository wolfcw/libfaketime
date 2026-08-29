# Regression coverage for sanitizer-first preload ordering.

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
	if [ "$PLATFORM" != "linuxlike" ] || [ -z "${FAKETIME_TESTLIB:-}" ]; then
		echo "out=skip sanitizer preload configuration is unavailable - ok"
		return 0
	fi

	typeset sanitizer_lib="${FAKETIME_TESTLIB%%:*}"
	if [ ! -f "$sanitizer_lib" ]; then
		echo "out=skip sanitizer runtime is unavailable - ok"
		return 0
	fi
	run_testcase sanitizer_preload_preserves_timestamp_parsing
}

sanitizer_preload_preserves_timestamp_parsing()
{
	typeset output status
	output=$(timeout 5s env \
		LD_PRELOAD="$FAKETIME_TESTLIB" \
		FAKETIME="2021-08-19 12:00:00" \
		date +"%Y-%m-%d %H:%M:%S" 2>&1)
	status=$?
	if [ "$status" -eq 0 ] && [ "$output" = "2021-08-19 12:00:00" ]; then
		echo "out=$output sanitizer-first preload preserves timestamp parsing - ok"
		return 0
	fi
	echo "out=$output sanitizer-first preload preserves timestamp parsing - bad (status=$status)"
	return 1
}
