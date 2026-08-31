#!/bin/sh

# Rebuild the project from a clean tree and run the bounded functional gate.

set -eu

make clean
make TEST_DEMO=0 test
printf '%s\n' 'clean build and bounded test gate passed'
