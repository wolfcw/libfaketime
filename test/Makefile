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

%_test: %_test.c
	${CC} -o $@ ${CFLAGS} $<

randomtest: getrandom_test use_lib_random librandom.so
	./randomtest.sh

getpidtest: use_lib_getpid libgetpid.so
	./pidtest.sh

syscalltest: syscall_test
	./syscalltest.sh

lib%.o: lib%.c
	${CC} -c -o $@ -fpic ${CFLAGS} $<

lib%.so: lib%.o
	${CC} -o $@ -shared ${CFLAGS} $<

use_lib_%: use_lib_%.c lib%.so
	${CC} -L. -o $@ ${CFLAGS}  $< -l$*

clean:
	@rm -f ${OBJ} timetest getrandom_test lib*.o lib*.so use_lib_random use_lib_getpid

distclean: clean
	@echo

.PHONY: all test clean distclean randomtest
