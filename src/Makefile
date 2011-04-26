#
# Notes:
#
#   * Compilation Defines:
#
#     FAKE_STAT
#         - Enables time faking also for files' timestamps. 
#
#     NO_ATFILE
#         - Disables support for the fstatat() group of functions
#     
#     PTHREAD
#         - Define this to enable multithreading support.
#
#     PTHREAD_SINGLETHREADED_TIME
#         - Define this if you want to single-thread time() ... there ARE
#           possibile caching side-effects in a multithreaded environment
#           without this, but the performance impact may require you to
#           try it unsynchronized.
#
#     FAKE_INTERNAL_CALLS
#         - Also intercept libc internal __functions, e.g. not just time(), 
#           but also __time(). Enhances compatibility with applications
#           that make use of low-level system calls, such as Java Virtual
#           Machines.       
#
#     NO_CACHING
#         - Disables the caching of the fake time offset. Only disable caching
#           if you change the fake time offset during program runtime very 
#           frequently. Disabling the cache may negatively influence the
#           performance. 
#
#
#   * Compilation addition: second libMT target added for building the pthread-
#     enabled library as a separate library
#
#   * Compilation switch change: previous versions compiled using '-nostartfiles'
#     This is no longer the case since there is a 'startup' constructor for the library
#     which is used to activate the start-at times when specified. This also initializes
#     the dynamic disabling of the FAKE_STAT calls.
#

CC = gcc

PREFIX = /usr/local

all: lib libMT testprog test

libs: lib libMT

lib: faketime.c
	${CC} -DFAKE_STAT -DFAKE_INTERNAL_CALLS -shared -fPIC -Wl,-soname,libfaketime.so.1 -o libfaketime.so.1 faketime.c -ldl -lm -lpthread

libMT: faketime.c
	${CC} -DFAKE_STAT -DFAKE_INTERNAL_CALLS -DPTHREAD -DPTHREAD_SINGLETHREADED_TIME -shared -fPIC -Wl,-soname,libfaketimeMT.so.1 -o libfaketimeMT.so.1 faketime.c -ldl -lm -lpthread

testprog: timetest.c
	${CC} -DFAKE_STAT -o timetest timetest.c -lrt

test: lib testprog
	@echo
	@./test.sh

notest: lib libMT testprog
	@echo

clean: 
	@rm -f libfaketime.so.1 libfaketimeMT.so.1 timetest

distclean: clean
	@echo 

install: libs
	@echo 
	@echo "Copying the libraries to /usr/lib/faketime and the wrapper script to /usr/bin ..."
	-mkdir -p "${DESTDIR}${PREFIX}/lib/faketime"
	cp libfaketime.so.1 libfaketimeMT.so.1 "${DESTDIR}${PREFIX}/lib/faketime"
	cp faketime "${DESTDIR}${PREFIX}/bin"
	cp faketime.1 "${DESTDIR}${PREFIX}/share/man/man1/faketime.1"
	gzip "${DESTDIR}${PREFIX}/share/man/man1/faketime.1"
	-mkdir -p "${DESTDIR}${PREFIX}/share/doc/faketime"
	cp README "${DESTDIR}${PREFIX}/share/doc/faketime/README"
	cp Changelog "${DESTDIR}${PREFIX}/share/doc/faketime/Changelog"

