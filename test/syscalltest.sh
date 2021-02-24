#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"

set -e

error=0
run0=$(./syscall_test)
run1=$(LD_PRELOAD="$FTPL" ./syscall_test)
run2=$(FAKERANDOM_SEED=0x0000000000000000 LD_PRELOAD="$FTPL" ./syscall_test)
run3=$(FAKERANDOM_SEED=0x0000000000000000 LD_PRELOAD="$FTPL" ./syscall_test)
run4=$(FAKERANDOM_SEED=0xDEADBEEFDEADBEEF LD_PRELOAD="$FTPL" ./syscall_test)

if [ "$run0" = "$run1" ] ; then
    error=1
    printf >&2 'test run without LD_PRELOAD matches run with LD_PRELOAD.  This is very unlikely.\n'
fi
if [ "$run1" = "$run2" ] ; then
    error=2
    printf >&2 'test with LD_PRELOAD but without FAKERANDOM_SEED matches run with LD_PRELOAD and FAKERANDOM_SEED.  This is also very unlikely.\n'
fi
if [ "$run2" != "$run3" ]; then
    error=1
    printf >&2 'test run with same seed produces different outputs.\n'
fi
if [ "$run3" = "$run4" ]; then
    error=1
    printf >&2 'test runs with different seeds produce the same outputs.\n'
fi
