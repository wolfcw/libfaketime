#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"

set -e

error=0

repeat3x5="$(FAKERANDOM_SEED=0xDEADBEEFDEADBEEF LD_PRELOAD="$FTPL" ./repeat_random 3 5)"
repeat5x3="$(FAKERANDOM_SEED=0xDEADBEEFDEADBEEF LD_PRELOAD="$FTPL" ./repeat_random 5 3)"

if [ "$repeat3x5" != "$repeat5x3" ]; then
    error=1
    printf >&2 '5 calls of getrandom(3) got %s\n3 calls of getrandom(5) got %s\n' "$repeat3x5" "$repeat5x3"
fi

if [ 0 = $error ]; then
    printf 'getrandom interception test successful.\n'
fi

exit $error
