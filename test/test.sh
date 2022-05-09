#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"
MALLOC_INTERCEPT=./libmallocintercept.so

if [ -f /etc/faketimerc ] ; then
	echo "Running the test program with your system-wide default in /etc/faketimerc"
	echo "\$ LD_PRELOAD=$FTPL ./timetest"
	LD_PRELOAD="$FTPL" ./timetest
	echo
else
	echo "Running the test program with no faked time specified"
	echo "\$ LD_PRELOAD=$FTPL ./timetest"
	LD_PRELOAD="$FTPL" ./timetest
	echo
fi

echo "============================================================================="
echo

echo "Running the test program with absolute date 2003-01-01 10:00:05 specified"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"2003-01-01 10:00:05\" ./timetest"
LD_PRELOAD="$FTPL" FAKETIME="2003-01-01 10:00:05" ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with START date @2005-03-29 14:14:14 specified"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"@2005-03-29 14:14:14\" ./timetest"
LD_PRELOAD="$FTPL" FAKETIME="@2005-03-29 14:14:14" ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified"
echo "LD_PRELOAD=$FTPL FAKETIME=\"-10d\" ./timetest"
LD_PRELOAD="$FTPL" FAKETIME="-10d" ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified, and FAKE_STAT disabled"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"-10d\" NO_FAKE_STAT=1 ./timetest"
LD_PRELOAD="$FTPL" FAKETIME="-10d" NO_FAKE_STAT=1 ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days positive offset specified, and speed-up factor"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"+10d x1\" ./timetest"
LD_PRELOAD="$FTPL" FAKETIME="+10d x1" NO_FAKE_STAT=1 ./timetest
echo

echo "============================================================================="
echo

echo "Running the 'date' command with 15 days negative offset specified"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"-15d\" date"
LD_PRELOAD="$FTPL" FAKETIME="-15d" date
echo

echo "============================================================================="
echo

echo "Running the test program with malloc interception"
echo "\$ LD_PRELOAD=./libmallocintercept.so:$FTPL ./timetest"
LD_PRELOAD="./libmallocintercept.so:$FTPL" ./timetest
echo

echo "============================================================================="
echo

echo "@2005-03-29 14:14:14" > .faketimerc-for-test
echo "Running the test program with malloc interception and file faketimerc"
echo "\$ FAKETIME_NO_CACHE=1 FAKETIME_TIMESTAMP_FILE=.faketimerc-for-test LD_PRELOAD=./libmallocintercept.so:$FTPL ./timetest"
FAKETIME_NO_CACHE=1 FAKETIME_TIMESTAMP_FILE=.faketimerc-for-test LD_PRELOAD="./libmallocintercept.so:$FTPL" ./timetest
rm .faketimerc-for-test
echo

echo "============================================================================="
echo "Testing finished."

exit 0
