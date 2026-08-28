#!/bin/sh

FTPL="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}"

run_faked()
{
	if [ -n "${LD_PRELOAD:-}" ]; then
		LD_PRELOAD="$FTPL:$LD_PRELOAD" "$@"
	else
		LD_PRELOAD="$FTPL" "$@"
	fi
}

if [ -f /etc/faketimerc ] ; then
	echo "Running the test program with your system-wide default in /etc/faketimerc"
	echo "\$ LD_PRELOAD=$FTPL ./timetest"
	run_faked ./timetest
	echo
else
	echo "Running the test program with no faked time specified"
	echo "\$ LD_PRELOAD=$FTPL ./timetest"
	run_faked ./timetest
	echo
fi

echo "============================================================================="
echo

echo "Running the test program with absolute date 2003-01-01 10:00:05 specified"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"2003-01-01 10:00:05\" ./timetest"
FAKETIME="2003-01-01 10:00:05" run_faked ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with START date @2005-03-29 14:14:14 specified"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"@2005-03-29 14:14:14\" ./timetest"
FAKETIME="@2005-03-29 14:14:14" run_faked ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified"
echo "LD_PRELOAD=$FTPL FAKETIME=\"-10d\" ./timetest"
FAKETIME="-10d" run_faked ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified, and FAKE_STAT disabled"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"-10d\" NO_FAKE_STAT=1 ./timetest"
FAKETIME="-10d" NO_FAKE_STAT=1 run_faked ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days positive offset specified, and speed-up factor"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"+10d x1\" ./timetest"
FAKETIME="+10d x1" NO_FAKE_STAT=1 run_faked ./timetest
echo

echo "============================================================================="
echo

echo "Running the 'date' command with 15 days negative offset specified"
echo "\$ LD_PRELOAD=$FTPL FAKETIME=\"-15d\" date"
FAKETIME="-15d" run_faked date
echo

echo "============================================================================="
echo

echo "@2005-03-29 14:14:14" > .faketimerc-for-test
echo "Running the test program with malloc interception and file faketimerc"
echo "\$ FAKETIME_NO_CACHE=1 FAKETIME_TIMESTAMP_FILE=.faketimerc-for-test LD_PRELOAD=./libmallocintercept.so:$FTPL ./timetest"
if [ -n "${LD_PRELOAD:-}" ]; then
	TEST_PRELOAD="./libmallocintercept.so:$FTPL:$LD_PRELOAD"
else
	TEST_PRELOAD="./libmallocintercept.so:$FTPL"
fi
FAKETIME_NO_CACHE=1 FAKETIME_TIMESTAMP_FILE=.faketimerc-for-test LD_PRELOAD="$TEST_PRELOAD" ./timetest
rm .faketimerc-for-test
echo

echo "============================================================================="
echo "Testing finished."

exit 0
