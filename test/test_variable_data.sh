#!/bin/bash

set -e

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"
DELAY="${DELAY:-2}"

declare -A firstunset first second delayed delayedunset

err=0
for func in "$@"; do
    if ! [ -x "./run_$func" ]; then
        printf >&2 '%s does not exist, failing\n' "./run_$func"
        exit 1
    fi
    read varname value < "snippets/$func.variable"
    unset $varname
    firstunset[$func]="$(env LD_PRELOAD="$FTPL" "./run_$func")"
    first[$func]="$(env LD_PRELOAD="$FTPL" "$varname=$value" "./run_$func")"
    second[$func]="$(env LD_PRELOAD="$FTPL" "$varname=$value" "./run_$func")"
    if [ "${first[$func]}" != "${second[$func]}" ]; then
        printf >&2 '[%s] Set %s="%s", but got two different outputs:\n - %s\n - %s\n' "$func" "$varname" "$value" "${first[$func]}" "${second[$func]}"
        err=$(( $err + 1 ))
    fi
    if [ "${first[$func]}" == "${firstunset[$func]}" ]; then
        printf >&2 '[%s] Same answer when %s="%s" and when unset:\n -   set: %s\n - unset: %s\n' "$func" "$varname" "$value" "${first[$func]}" "${firstunset[$func]}"
        err=$(( $err + 1 ))
    fi
done

printf "Sleeping %d seconds..." "$DELAY"
sleep "$DELAY"
printf 'done\n'

for func in "$@"; do
    read varname value < "snippets/$func.variable"
    unset $varname
    delayed[$func]="$(env LD_PRELOAD="$FTPL" "$varname=$value" "./run_$func")"
    delayedunset[$func]="$(LD_PRELOAD="$FTPL" "./run_$func")"
    if [ "${first[$func]}" != "${delayed[$func]}" ]; then
        printf >&2 '[%s] Vary across delay of %d seconds (%s="%s"):\n - before: %s\n -  after: %s\n' "$func" "$DELAY" "$varname" "$value" "${first[$func]}" "${delayed[$func]}"
        err=$(( $err + 1 ))
    fi
    if [ "${firstunset[$func]}" == "${delayedunset[$func]}" ]; then
        printf >&2 '[%s] Same answer when unset across delay of %d seconds:\n - before: %s\n -  after: %s\n' "$func" "$DELAY" "${firstunset[$func]}" "${delayedunset[$func]}"
        err=$(( $err + 1 ))
    fi
done

if [ "$err" -gt 0 ]; then
    printf >&2 'Got %d errors, failing\n' "$err"
    exit 1
fi
exit 0
