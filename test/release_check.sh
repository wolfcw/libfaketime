#!/bin/sh

# Lightweight release hygiene check; runtime tests remain in the normal gate.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$SCRIPT_DIR/ci_validate.sh"
test -s "$SCRIPT_DIR/fuzz/faketime-boundaries"
test -s "$SCRIPT_DIR/fuzz/config-boundaries"
test -f "$SCRIPT_DIR/README-platforms.md"
printf '%s\n' 'release hygiene checks passed'
