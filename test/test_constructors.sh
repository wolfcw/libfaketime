#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"

function="$1"

printf 'Testing library init for %s (no LD_PRELOAD)\n' "$function" 
LD_LIBRARY_PATH=. "./use_lib_$function"
printf 'Testing library init for %s (LD_PRELOAD)\n' "$function" 
LD_LIBRARY_PATH=. LD_PRELOAD="$FTPL" "./use_lib_$function"
