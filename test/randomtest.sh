#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"

set -e

error=0

for iface in getrandom getentropy; do
    printf "Testing %s() interception...\n" "$iface"

    "./${iface}_test" > "${iface}.alone"
    LD_PRELOAD="$FTPL" "./${iface}_test" > "${iface}.preload"
    FAKERANDOM_SEED=0x12345678DEADBEEF LD_PRELOAD="$FTPL" "./${iface}_test" > "${iface}.preload.seed0"
    FAKERANDOM_SEED=0x12345678DEADBEEF LD_PRELOAD="$FTPL" "./${iface}_test" > "${iface}.preload.seed1"
    FAKERANDOM_SEED=0x0000000000000000 LD_PRELOAD="$FTPL" "./${iface}_test" > "${iface}.preload.seed2"

    if diff -u "${iface}.alone" "${iface}.preload" > /dev/null; then
        error=1
        printf >&2 '%s() without the LD_PRELOAD matches a run without LD_PRELOAD\n' "$iface"
    fi
    if diff -u "${iface}.preload" "${iface}.preload.seed0" > /dev/null; then
        error=2
        printf >&2 '%s() without a seed produced the same data as a run with a seed!\n' "$iface"
    fi
    if ! diff -u "${iface}.preload.seed0" "${iface}.preload.seed1"; then
        error=3
        printf >&2 '%s() with identical seeds differed!\n' "$iface"
    fi
    if diff -u "${iface}.preload.seed1" "${iface}.preload.seed2" >/dev/null; then
        error=4
        printf >&2 '%s() with different seeds produced the same data!\n' "$iface"
    fi
    rm -f "${iface}.alone" "${iface}.preload" "${iface}.preload.seed0" "${iface}.preload.seed1" "${iface}.preload.seed2" 
done

printf 'testing shared object with getrandom() in library constructor\n'
LD_LIBRARY_PATH=. ./use_lib_getrandom
printf 'now with LD_PRELOAD and FAKERANDOM_SEED\n'
FAKERANDOM_SEED=0x0000000000000000 LD_PRELOAD="$FTPL" LD_LIBRARY_PATH=. ./use_lib_getrandom
# this demonstrates the crasher from https://github.com/wolfcw/libfaketime/issues/295
printf 'now with LD_PRELOAD without FAKERANDOM_SEED\n'
LD_PRELOAD="$FTPL" LD_LIBRARY_PATH=. ./use_lib_getrandom


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
