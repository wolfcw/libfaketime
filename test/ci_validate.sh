#!/bin/sh

# Static validation for the shell-based CI and functional-test harness.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

for script in "$SCRIPT_DIR"/*.sh "$SCRIPT_DIR"/functests/*.sh; do
    [ -f "$script" ] || continue
    case "$script" in
        */testframe.sh|*/testframe.inc) bash -n "$script" ;;
        *) sh -n "$script" ;;
    esac
done

sh -n "$SCRIPT_DIR/compile_abi_variants.sh"

REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
grep -q 'TEST_DEMO=0 test' "$SCRIPT_DIR/docker_baseline.sh" || {
    echo "error: Docker baseline must use the bounded functional gate" >&2
    exit 1
}
grep -q '__GLIBC__ ' "$REPO_DIR/src/Makefile" || {
    echo "error: glibc detection must match the exact macro" >&2
    exit 1
}
grep -q -- '-Wall' "$SCRIPT_DIR/Makefile" || {
    echo "error: test builds must enable compiler warnings" >&2
    exit 1
}
grep -q -- '-Werror' "$SCRIPT_DIR/Makefile" || {
    echo "error: Linux test builds must treat warnings as errors" >&2
    exit 1
}

printf '%s\n' 'CI shell syntax validation passed'
