#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"

set -e

error=0
./getrandom_test > run-base
LD_PRELOAD="$FTPL" ./getrandom_test > run0
FAKERANDOM_SEED=0x12345678DEADBEEF LD_PRELOAD="$FTPL" ./getrandom_test > run1
FAKERANDOM_SEED=0x12345678DEADBEEF LD_PRELOAD="$FTPL" ./getrandom_test > run2
FAKERANDOM_SEED=0x0000000000000000 LD_PRELOAD="$FTPL" ./getrandom_test > run3


if diff -u run-base run0 > /dev/null; then
    error=1
    printf >&2 'test run without the LD_PRELOAD matches a run without LD_PRELOAD\n'
fi

if diff -u run0 run1 > /dev/null; then
    error=2
    printf >&2 'test run without a seed produced the same data as a run with a seed!\n'
fi
if ! diff -u run1 run2; then
    error=3
    printf >&2 'test runs with identical seeds differed!\n'
fi
if diff -u run2 run3 >/dev/null; then
    error=4
    printf >&2 'test runs with different seeds produced the same data!\n'
fi

printf 'testing shared object with getrandom() in library constructor\n'
LD_LIBRARY_PATH=. ./use_lib_random
printf 'now with LD_PRELOAD and FAKERANDOM_SEED\n'
FAKERANDOM_SEED=0x0000000000000000 LD_PRELOAD="$FTPL" LD_LIBRARY_PATH=. ./use_lib_random
# this demonstrates the crasher from https://github.com/wolfcw/libfaketime/issues/295
printf 'now with LD_PRELOAD without FAKERANDOM_SEED\n'
LD_PRELOAD="$FTPL" LD_LIBRARY_PATH=. ./use_lib_random


FAKERANDOM_SEED=0xDEADBEEFDEADBEEF LD_PRELOAD="$FTPL" ./repeat_random 3 5 > run4 
FAKERANDOM_SEED=0xDEADBEEFDEADBEEF LD_PRELOAD="$FTPL" ./repeat_random 5 3 > run5

if ! diff -u run4 run5; then
    error=5
    printf >&2 '5 calls of getrandom(3) did not produce the same stream as 3 calls of getrandom(5)\n'
fi

rm -f run-base run0 run1 run2 run3 run4 run5

if [ 0 = $error ]; then
    printf 'getrandom interception test successful.\n'
fi

exit $error
