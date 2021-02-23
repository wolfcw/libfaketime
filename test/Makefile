CC = gcc

CFLAGS = -std=gnu99 -Wall -DFAKE_STAT -Werror -Wextra $(FAKETIME_COMPILE_CFLAGS)
LDFLAGS = -lrt -lpthread

SRC = timetest.c
OBJ = ${SRC:.c=.o}

all: timetest test

.c.o:
	${CC} -c ${CFLAGS} $<

timetest: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

test: timetest functest
	@echo
	@./test.sh

# run functional tests
functest:
	./testframe.sh functests

getrandom_test: getrandom_test.c
	${CC} -o $@ ${CFLAGS} $<

randomtest: getrandom_test use_lib_random
	./randomtest.sh

librandom.o: librandom.c
	${CC} -c -o $@ -fpic ${CFLAGS} $<

librandom.so: librandom.o
	${CC} -o $@ -shared ${CFLAGS} $<

use_lib_random: use_lib_random.c librandom.so
	${CC} -L. -o $@ ${CFLAGS}  $< -lrandom

clean:
	@rm -f ${OBJ} timetest getrandom_test librandom.o librandom.so use_lib_random

distclean: clean
	@echo

.PHONY: all test clean distclean randomtest
