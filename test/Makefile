CC = gcc

CFLAGS = -std=gnu99 -Wall -DFAKE_STAT -Werror -Wextra $(FAKETIME_COMPILE_CFLAGS)
LDFLAGS = -lrt -lpthread

SRC = timetest.c
OBJ = ${SRC:.c=.o}

TESTFUNCS = $(notdir $(basename $(wildcard snippets/*.c)))

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

randomtest: getrandom_test use_lib_getrandom libgetrandom.so repeat_random getentropy_test
	./randomtest.sh

getpidtest: use_lib_getpid libgetpid.so
	./pidtest.sh

syscalltest: syscall_test
	./syscalltest.sh


## testing when interception points get called in library constructors:

test_library_constructors: test_constructors.sh $(foreach f,${TESTFUNCS},use_lib_${f} lib${f}.so)
	true $(foreach f,${TESTFUNCS},&& ./test_constructors.sh ${f})

lib%.so: _libtest.c snippets/%.c
	sed s/FUNC_NAME/$*/g < _libtest.c | ${CC} -shared -o $@ -fpic ${CFLAGS} -x c -

use_lib_%: _use_lib_test.c snippets/%.c lib%.so
	sed s/FUNC_NAME/$*/g < _use_lib_test.c | ${CC} -L. -o $@ ${CFLAGS} -x c - -l$*


## cleanup and metainformation

clean:
	@rm -f ${OBJ} timetest getrandom_test syscall_test $(foreach f,${TESTFUNCS},use_lib_${f} lib${f}.so)

distclean: clean
	@echo

.PHONY: all test clean distclean randomtest
