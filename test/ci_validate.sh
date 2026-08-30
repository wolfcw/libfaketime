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

printf '%s\n' 'CI shell syntax validation passed'
