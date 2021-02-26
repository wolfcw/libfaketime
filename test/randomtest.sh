#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"

set -e

error=0

FAKERANDOM_SEED=0xDEADBEEFDEADBEEF LD_PRELOAD="$FTPL" ./repeat_random 3 5 > repeat3x5 
FAKERANDOM_SEED=0xDEADBEEFDEADBEEF LD_PRELOAD="$FTPL" ./repeat_random 5 3 > repeat5x3

if ! diff -u repeat3x5 repeat5x3; then
    error=5
    printf >&2 '5 calls of getrandom(3) did not produce the same stream as 3 calls of getrandom(5)\n'
fi

rm -f repeat3x5 repeat5x3

if [ 0 = $error ]; then
    printf 'getrandom interception test successful.\n'
fi

exit $error
