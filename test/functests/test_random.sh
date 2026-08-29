# Tests deterministic random interception when FAKE_RANDOM is enabled.

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
	run_testcase deterministic_random_output
}

deterministic_random_output()
{
	if [ ! -x ./random_test ]; then
		echo "out=skip FAKE_RANDOM is not enabled - ok"
		return 0
	fi
	typeset first second
	if [ "$PLATFORM" = "mac" ]; then
		first=$(FAKERANDOM_SEED=0x12345678 \
			DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
			DYLD_FORCE_FLAT_NAMESPACE=1 ./random_test)
		second=$(FAKERANDOM_SEED=0x12345678 \
			DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
			DYLD_FORCE_FLAT_NAMESPACE=1 ./random_test)
	else
		first=$(FAKERANDOM_SEED=0x12345678 \
			LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" ./random_test)
		second=$(FAKERANDOM_SEED=0x12345678 \
			LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" ./random_test)
	fi
	asserteq "$first" "$second" \
		"same random seed should produce repeatable output"
}
