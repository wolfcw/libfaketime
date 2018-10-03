AM_CFLAGS =-DFAKE_SLEEP

AM_LDFLAGS =

if SOLARIS
  AM_CFLAGS += -D__EXTENSIONS__ -D_XOPEN_SOURCE=600
  AM_LDFLAGS += -Wl,--version-script=libfaketime.map
endif
if !MACOS
  AM_CFLAGS += -std=gnu99 -Wall -Wextra -Werror -DFAKE_STAT
endif

bin_PROGRAMS = timetest
timetest_SOURCES = timetest.c

# CC = gcc
#
# CFLAGS = -std=gnu99 -Wall -DFAKE_STAT -Werror -Wextra
# LDFLAGS = -lrt -lpthread
#
# SRC = timetest.c
# OBJ = ${SRC:.c=.o}
#
# all: timetest test
#
# .c.o:
# 	${CC} -c ${CFLAGS} $<
#
# timetest: ${OBJ}
# 	${CC} -o $@ ${OBJ} ${LDFLAGS}
#
test: timetest functest
	@echo
	@./test.sh
#
# run functional tests
functest:
	./testframe.sh functests

# clean-local:
# 	@rm -f ${OBJ} timetest
#
# distclean-local: clean-local
# 	@echo
#
# .PHONY: all test clean-local distclean-local
