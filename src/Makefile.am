ACLOCAL_AMFLAGS=-I m4
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

AM_CFLAGS =-DFAKE_SLEEP -DFAKE_INTERNAL_CALLS

AM_LDFLAGS =

if SOLARIS
  AM_CFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=600
  AM_LDFLAGS += -Wl,--version-script=libfaketime.map
endif
if MACOS
  AM_LDFLAGS += -dynamiclib -current_version 0.9.7 -compatibility_version 0.7
else
  AM_CFLAGS += -std=gnu99 -Wall -Wextra -Werror -DFAKE_STAT -DFAKE_PTHREAD -DFAKE_TIMERS -DFAKE_INTERNAL_CALLS -fPIC
endif

LTCFLAGS=""
lib_LTLIBRARIES = libfaketime.la
libfaketime_la_SOURCES = libfaketime.c
if MACOS
  libfaketime_la_LDFLAGS = -dynamiclib -current_version 0.9.8 -compatibility_version 0.7
else
  libfaketime_la_LIBADD = -ldl -lm -lrt -lpthread
  libfaketime_la_LDFLAGS = -shared
endif

bin_PROGRAMS = faketime
faketime_SOURCES = faketime.c
