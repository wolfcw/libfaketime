#!/bin/sh

error=0
./getrandom_test > run0
FAKERANDOM_SEED=0x12345678DEADBEEF LD_PRELOAD=../src/libfaketime.so.1 ./getrandom_test > run1
FAKERANDOM_SEED=0x12345678DEADBEEF LD_PRELOAD=../src/libfaketime.so.1 ./getrandom_test > run2
FAKERANDOM_SEED=0x0000000000000000 LD_PRELOAD=../src/libfaketime.so.1 ./getrandom_test > run3


if diff -u run0 run1 > /dev/null; then
    error=1
    printf >&2 'test run without a seed produced the same data as a run with a seed!\n'
fi
if ! diff -u run1 run2; then
    error=2
    printf >&2 'test runs with identical seeds differed!\n'
fi
if diff -u run2 run3 >/dev/null; then
    error=3
    printf >&2 'test runs with different seeds produced the same data!\n'
fi

rm -f run0 run1 run2 run3

if [ 0 = $error ]; then
    printf 'getrandom interception test successful.\n'
fi

exit $error
