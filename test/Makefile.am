AM_CPPFLAGS =-DFAKE_SLEEP -DFAKE_INTERNAL_CALLS -I@top_srcdir@/src

AM_CFLAGS =
AM_LDFLAGS =

bin_PROGRAMS = timetest
timetest_SOURCES = timetest.c

if SOLARIS
  AM_CPPFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=600
  AM_LDFLAGS += -Wl,--version-script=libfaketime.map
endif
if !MACOS
  AM_CPPFLAGS += -DFAKE_STAT -DFAKE_PTHREAD -DFAKE_TIMERS -DFAKE_INTERNAL_CALLS
  AM_CFLAGS += -std=gnu99 -Wall -Wextra -Werror -fPIC -Wno-nonnull-compare
  timetest_LDADD = -ldl -lm -lrt -lpthread
endif

test: timetest functest
	@echo
	@./test.sh
#
# run functional tests
functest:
	./testframe.sh functests
