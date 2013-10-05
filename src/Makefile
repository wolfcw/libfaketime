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
# 	  FAKE_SLEEP
# 	      - Also intercept sleep(), nanosleep(), usleep(), alarm(), [p]poll()
#
#	  FAKE_TIMERS
#	      - Also intercept timer_settime() and timer_gettime()
#
#     NO_CACHING
#         - Disables the caching of the fake time offset. Only disable caching
#           if you change the fake time offset during program runtime very
#           frequently. Disabling the cache may negatively influence the
#           performance.
#
#	  MULTI_ARCH
#	  	  - If MULTI_ARCH is set, the faketime wrapper program will put a literal
#	  	    $LIB into the LD_PRELOAD environment variable it creates, which makes
#	  	    ld automatically choose the correct library version to use for the
#	  	    target binary. Use for Linux platforms with Multi-Arch support only!
#
#   * Compilation addition: second libMT target added for building the pthread-
#     enabled library as a separate library
#
#   * Compilation switch change: previous versions compiled using '-nostartfiles'
#     This is no longer the case since there is a 'startup' constructor for the library
#     which is used to activate the start-at times when specified. This also initializes
#     the dynamic disabling of the FAKE_STAT calls.
#
# By default, libfaketime will be compiled for your system's default architecture.
# To build 32-bit libraries and binaries, add -m32 to CFLAGS and LDFLAGS.
#

CC ?= gcc
INSTALL ?= install

PREFIX ?= /usr/local

CFLAGS += -std=gnu99 -Wall -Wextra -Werror -DFAKE_STAT -DFAKE_SLEEP -DFAKE_TIMERS -DFAKE_INTERNAL_CALLS -fPIC -DPREFIX='"'$(PREFIX)'"'
LIB_LDFLAGS += -shared
LDFLAGS += -Wl,--version-script=libfaketime.map -lrt
LDADD += -ldl -lm -lpthread -lrt

SRC = libfaketime.c
LIBS_OBJ = libfaketime.o libfaketimeMT.o
BINS = faketime

SONAME = 1
LIBS = libfaketime.so.${SONAME} libfaketimeMT.so.${SONAME}

all: ${LIBS} ${BINS}

faketimeMT.o: EXTRA_FLAGS := -DPTHREAD -DPTHREAD_SINGLETHREADED_TIME

${LIBS_OBJ}: libfaketime.c
	${CC} -o $@ -c ${CFLAGS} ${EXTRA_FLAGS} $<

%.so.${SONAME}: %.o libfaketime.map
	${CC} -o $@ -Wl,-soname,$@ ${LDFLAGS} ${LIB_LDFLAGS} $< ${LDADD}

${BINS}: faketime.c
	${CC} -o $@ ${CFLAGS} ${EXTRA_FLAGS} $< ${LDFLAGS}

clean:
	@rm -f ${LIBS_OBJ} ${LIBS} ${BINS}

distclean: clean
	@echo

install: ${LIBS} ${BINS}
	@echo
	@echo "Copying the faketime libraries to ${DESTDIR}${PREFIX}/lib/faketime and the faketime wrapper script to ${DESTDIR}${PREFIX}/bin ..."
	$(INSTALL) -dm0755 "${DESTDIR}${PREFIX}/lib/faketime/"
	$(INSTALL) -m0644 ${LIBS} "${DESTDIR}${PREFIX}/lib/faketime/"
	$(INSTALL) -Dm0755 faketime "${DESTDIR}${PREFIX}/bin/faketime"

uninstall:
	for f in ${LIBS}; do rm -f "${DESTDIR}${PREFIX}/lib/faketime/$$f"; done
	rmdir "${DESTDIR}${PREFIX}/lib/faketime"
	rm -f "${DESTDIR}${PREFIX}/bin/faketime"

.PHONY: all clean distclean install uninstall
