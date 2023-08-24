#
# Notes:
#
#   * Compilation Defines that are set by default:
#
#     FAKE_STAT
#         - Enables time faking when reading files' timestamps.
#
#     FAKE_SLEEP
#         - Also intercept sleep(), nanosleep(), usleep(), alarm(), [p]poll()
#
#     FAKE_TIMERS
#         - Also intercept timer_settime() and timer_gettime()
#
#     FAKE_PTHREAD
#         - Intercept pthread_cond_timedwait
#
#     FAKE_INTERNAL_CALLS
#         - Also intercept libc internal __functions, e.g. not just time(),
#           but also __time(). Enhances compatibility with applications
#           that make use of low-level system calls, such as Java Virtual
#           Machines.
#
#     PTHREAD_SINGLETHREADED_TIME (only set in libfaketimeMT.so)
#         - Define this if you want to single-thread time() ... there ARE
#           possible caching side-effects in a multithreaded environment
#           without this, but the performance impact may require you to
#           try it unsynchronized.
#
#   * Compilation Defines that are unset by default:
#
#     FAKE_FILE_TIMESTAMPS, FAKE_UTIME
#         - Enables time faking for the utime* functions.  If enabled via
#           FAKE_FILE_TIMESTAMPS, the faking is opt-in at runtime using
#           with the FAKE_UTIME environment variable. If enabled via
#           FAKE_UTIME, the faking is opt-out at runtime.
#
#     NO_ATFILE
#         - Disables support for the fstatat() group of functions
#
#     FAKE_SETTIME
#         - Intercept clock_settime(), settimeofday(), and adjtime()
#
#     FAKE_RANDOM
#         - Intercept getrandom()
#
#     FAKE_PID
#         - Intercept getpid()
#
#     INTERCEPT_SYSCALL
#         - (On GNU/Linux only) intercept glibc's syscall() for known relevant syscalls.
#           If enabled, this currently only works to divert the getrandom syscall.
#
#         - note that on unusual architectures, if INTERCEPT_SYSCALL is set, you may
#           need to explicitly define variadic_promotion_t (e.g. by putting
#           -Dvariadic_promotion_t=int into CFLAGS).  See src/faketime_common.h for
#           more info.
#
#     FAKE_STATELESS
#         - Remove support for any functionality that requires sharing state across
#           threads of a process, or different processes. This decreases the risk of
#           interference with a program's normal execution, at the cost of supporting
#           fewer ways of specifying the time.
#           Concretely, this currently:
#           - disables PTHREAD_SINGLETHREADED_TIME, which can cause deadlocks in
#             multithreaded programs that fork due to making clock_gettime not
#             async-signal-safe
#           - disables all shared-memory across processes
#
#     FORCE_MONOTONIC_FIX
#         - If the test program hangs forever on
#                  " pthread_cond_timedwait: CLOCK_MONOTONIC test
#                    (Intentionally sleeping 1 second...)          "
#           then add -DFORCE_MONOTONIC_FIX to CFLAGS and recompile.
#           (This is a platform-specific issue we cannot handle at run-time.)
#
#     MULTI_ARCH
#         - If MULTI_ARCH is set, the faketime wrapper program will put a literal
#           $LIB into the LD_PRELOAD environment variable it creates, which makes
#           ld automatically choose the correct library version to use for the
#           target binary. Use for Linux platforms with Multi-Arch support only!
#
#     SILENT
#         - avoid that the faketime wrapper complains when running within a
#           libfaketime environment
#
#     FAIL_PRE_INIT_CALLS
#         - If the time is queried before the library was initialised, let the
#           call fail instead of trying to initialise on-the-fly. This fixes /
#           works around hangs that were seen with address sanitizer.
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
# Change PREFIX to where you want libfaketime (libraries and wrapper binary) installed.
# LIBDIRNAME is relative to PREFIX. The default is to install into $PREFIX/lib/faketime,
# but you can set LIBDIRNAME to, e.g., /lib64 if you want to install it elsewhere.
# LIBDIRNAME has been introduced to support MultiLib systems. Please do not change the
# default value on MultiArch systems.
#
# For testing in the current directory without installation, try make PREFIX= LIBDIRNAME='.'

CC ?= gcc
INSTALL ?= install

PREFIX ?= /usr/local
LIBDIRNAME ?= /lib/faketime
PLATFORM ?=$(shell uname)

ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang version"), 1)
COMPILER := clang
else
COMPILER := gcc
endif
export COMPILER

CFLAGS += -std=gnu99 -Wall -Wextra -Werror -DFAKE_PTHREAD -DFAKE_STAT -DFAKE_UTIME -DFAKE_SLEEP -DFAKE_TIMERS -DFAKE_INTERNAL_CALLS -fPIC -DPREFIX='"'$(PREFIX)'"' -DLIBDIRNAME='"'$(LIBDIRNAME)'"' $(FAKETIME_COMPILE_CFLAGS)

ifeq ($(COMPILER),clang)
CFLAGS += -Wno-tautological-pointer-compare
endif

ifeq ($(COMPILER),gcc)
CFLAGS += -Wno-nonnull-compare
endif

ifeq ($(PLATFORM),SunOS)
CFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=600
endif

LIB_LDFLAGS += -shared

LDFLAGS += $(FAKETIME_LINK_FLAGS)
ifneq ($(PLATFORM),SunOS)
LDFLAGS += -Wl,--version-script=libfaketime.map
endif

LDADD += -ldl -lm -lrt -lpthread
BIN_LDFLAGS += -lrt -lpthread

SRC = libfaketime.c
LIBS_OBJ = libfaketime.o libfaketimeMT.o
BINS = faketime

SONAME = 1
LIBS = libfaketime.so.${SONAME} libfaketimeMT.so.${SONAME}

all: ${LIBS} ${BINS}

libfaketimeMT.o: EXTRA_FLAGS := -DPTHREAD_SINGLETHREADED_TIME

${LIBS_OBJ}: libfaketime.c
	${CC} -o $@ -c ${CFLAGS} ${CPPFLAGS} ${EXTRA_FLAGS} $<

%.so.${SONAME}: %.o libfaketime.map
	${CC} -o $@ -Wl,-soname,$@ ${LDFLAGS} ${LIB_LDFLAGS} $< ${LDADD}

${BINS}: faketime.c
	${CC} -o $@ ${CFLAGS} ${CPPFLAGS} ${EXTRA_FLAGS} $< ${LDFLAGS} ${BIN_LDFLAGS}

clean:
	@rm -f ${LIBS_OBJ} ${LIBS} ${BINS}

distclean: clean
	@echo

install: ${LIBS} ${BINS}
	@echo
	@echo "Copying the faketime libraries to ${DESTDIR}${PREFIX}${LIBDIRNAME} and the faketime wrapper script to ${DESTDIR}${PREFIX}/bin ..."
	$(INSTALL) -dm0755 "${DESTDIR}${PREFIX}${LIBDIRNAME}/"
	$(INSTALL) -m0644 ${LIBS} "${DESTDIR}${PREFIX}${LIBDIRNAME}/"
	$(INSTALL) -Dm0755 faketime "${DESTDIR}${PREFIX}/bin/faketime"

uninstall:
	for f in ${LIBS}; do rm -f "${DESTDIR}${PREFIX}${LIBDIRNAME}/$$f"; done
	rmdir "${DESTDIR}${PREFIX}${LIBDIRNAME}"
	rm -f "${DESTDIR}${PREFIX}/bin/faketime"

.PHONY: all clean distclean install uninstall
