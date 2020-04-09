#!/bin/sh

##################################################################
# NOTICE:                                                        #
# When you edit this script, maintain compatibility with BASH 3. #
# This is the version that Apple distributes.                    #
##################################################################

LIB_BUILD_PATH="../src/$(grep 'LT_OBJDIR' ../config.h | cut -d'"' -f2)"
LIBFAKETIME_LA=$(find "$LIB_BUILD_PATH" | grep 'libfaketime\.\(dylib\|so\)$')

set_libfaketime() {
	the_path="$LIBFAKETIME_LA"
	if [[ "$(uname)" = "Darwin" ]]; then
		export DYLD_INSERT_LIBRARIES="$the_path"
		export DYLD_FORCE_FLAT_NAMESPACE=1
	else
		export LD_PRELOAD="$the_path"
	fi
}

echo $(set_libfaketime)


if [ -f /etc/faketimerc ] ; then
	echo "Running the test program with your system-wide default in /etc/faketimerc"
	echo "\$ $(set_libfaketime) ./timetest"
	set_libfaketime
	./timetest
	echo
else
	echo "Running the test program with no faked time specified"
	echo "\$ $(set_libfaketime) ./timetest"
	set_libfaketime
	./timetest
	echo
fi

echo "============================================================================="
echo

echo "Running the test program with absolute date 2003-01-01 10:00:05 specified"
echo "\$ $(set_libfaketime) FAKETIME=\"2003-01-01 10:00:05\" ./timetest"
set_libfaketime
export FAKETIME="2003-01-01 10:00:05"
./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with START date @2005-03-29 14:14:14 specified"
echo "\$ $(set_libfaketime) FAKETIME=\"@2005-03-29 14:14:14\" ./timetest"
set_libfaketime
export FAKETIME="@2005-03-29 14:14:14"
./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified"
echo "$(set_libfaketime) FAKETIME=\"-10d\" ./timetest"
set_libfaketime
export FAKETIME="-10d"
./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified, and FAKE_STAT disabled"
echo "\$ $(set_libfaketime) FAKETIME=\"-10d\" NO_FAKE_STAT=1 ./timetest"
set_libfaketime
export FAKETIME="-10d"
export NO_FAKE_STAT=1
./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days positive offset specified, and sped up 2 times"
echo "\$ $(set_libfaketime) FAKETIME=\"+10d x2\" ./timetest"
set_libfaketime
export FAKETIME="+10d x2"
export NO_FAKE_STAT=1
./timetest
echo

echo "============================================================================="
echo

echo "Running the 'date' command with 15 days negative offset specified"
echo "\$ $(set_libfaketime) FAKETIME=\"-15d\" date"
set_libfaketime
export FAKETIME="-15d"
date
echo

echo "============================================================================="
echo "Testing finished."

exit 0
