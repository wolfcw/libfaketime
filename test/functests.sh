#!/bin/sh
set -e
[ "${srcdir}" ] || srcdir=.

# run functional tests
# cd "${srcdir}/"
${srcdir}/testframe.sh ${srcdir}/functests
