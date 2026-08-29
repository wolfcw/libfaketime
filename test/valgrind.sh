#!/bin/sh

# Run a focused allocator check against the normal Linux shared library.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if [ "$(uname -s)" = "Darwin" ]; then
    echo "valgrind: skipped on macOS (use the sanitizer runner instead)"
    exit 0
fi
if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind: not installed; install valgrind and retry" >&2
    exit 2
fi

make -C "$REPO_DIR" all
cd "$REPO_DIR/test"
FAKETIME="+0" \
LD_PRELOAD="../src/libfaketime.so.1" \
valgrind --quiet --error-exitcode=99 --leak-check=full \
    --show-leak-kinds=definite,possible ./timetest >/dev/null
echo "valgrind: focused libfaketime run passed"
