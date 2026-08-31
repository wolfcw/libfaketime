#!/bin/sh

# Lightweight release hygiene check; runtime tests remain in the normal gate.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
RELEASE_VERSION=${RELEASE_VERSION:-0.9.13}

grep -q "version $RELEASE_VERSION" "$REPO_DIR/README" || {
    echo "error: README version does not match $RELEASE_VERSION" >&2
    exit 1
}
grep -q "version\[\] = \"$RELEASE_VERSION\"" "$REPO_DIR/src/faketime.c" || {
    echo "error: faketime wrapper version does not match $RELEASE_VERSION" >&2
    exit 1
}
grep -q "faketime $RELEASE_VERSION" "$REPO_DIR/man/faketime.1" || {
    echo "error: man page version does not match $RELEASE_VERSION" >&2
    exit 1
}
grep -q "current_version $RELEASE_VERSION" "$REPO_DIR/src/Makefile.OSX" || {
    echo "error: macOS library version does not match $RELEASE_VERSION" >&2
    exit 1
}

"$SCRIPT_DIR/ci_validate.sh"
test -s "$SCRIPT_DIR/fuzz/faketime-boundaries"
test -s "$SCRIPT_DIR/fuzz/config-boundaries"
test -f "$SCRIPT_DIR/README-platforms.md"
printf '%s\n' 'release hygiene checks passed'
