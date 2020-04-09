AM_CPPFLAGS =-DFAKE_SLEEP -DFAKE_INTERNAL_CALLS -I@top_srcdir@/src

AM_CFLAGS =
AM_LDFLAGS =

# functests.sh fail for me, test.sh hangs for me.
# TESTS = test.sh functests.sh
check_PROGRAMS = timetest
timetest_SOURCES = timetest.c
check_SCRIPTS = functests.sh testframe.sh test.sh
EXTRA_DIST = $(check_SCRIPTS) \
    functests/common.inc \
    functests/dont_test_false.sh \
    functests/test_exclude_mono.sh \
    functests/test_null.sh \
    functests/test_true.sh \
    functests/test_walkone.sh

if SOLARIS
  AM_CPPFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=600
  AM_LDFLAGS += -Wl,--version-script=libfaketime.map
endif
if !MACOS
  AM_CPPFLAGS += -DFAKE_STAT -DFAKE_PTHREAD -DFAKE_TIMERS -DFAKE_INTERNAL_CALLS
  AM_CFLAGS += -std=gnu99 -Wall -Wextra -Werror -fPIC -Wno-nonnull-compare
  timetest_LDADD = -ldl -lm -lrt -lpthread
endif


