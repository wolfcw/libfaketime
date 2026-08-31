#!/bin/sh

# Compile-only ABI preflight for the headers used by the time interposer.

set -eu

CC=${CC:-cc}
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libfaketime-abi.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

compile_probe()
{
    name=$1
    shift
    "$CC" "$@" -c -o "$tmpdir/$name.o" abi_info_test.c
}

compile_probe native

if "$CC" -dM -E -include features.h - < /dev/null 2>/dev/null |
   grep -q '^#define __GLIBC__ '; then
    compile_probe glibc-time64 -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64
    "$CC" -std=gnu99 -Wall -Wextra -Werror -fPIC \
        -DFAKE_PTHREAD -DFAKE_STAT -DFAKE_UTIME -DFAKE_SLEEP \
        -DFAKE_TIMERS -DFAKE_INTERNAL_CALLS -D_FILE_OFFSET_BITS=64 \
        -D_TIME_BITS=64 -DFAKETIME_TIME64_BUILD -I../src \
        -fsyntax-only ../src/libfaketime.c
fi

echo "ABI compile preflight passed ($CC)"
