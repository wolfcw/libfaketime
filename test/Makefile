CC = gcc

CFLAGS = -std=gnu99 -Wall -DFAKE_STAT -Werror -Wextra $(FAKETIME_COMPILE_CFLAGS)
LDFLAGS = -lrt -lpthread

SRC = timetest.c
OBJ = ${SRC:.c=.o}

TEST_SNIPPETS = $(notdir $(basename $(wildcard snippets/*.c)))
EXPECTATIONS= $(notdir $(basename $(wildcard snippets/*.variable)))

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

randomtest: repeat_random
	./randomtest.sh

## test variables

test_variable_data: test_variable_data.sh $(foreach f,${EXPECTATIONS},run_${f})
	./test_variable_data.sh ${EXPECTATIONS}

run_%: _run_test.c snippets/%.c
	sed s/SNIPPET_NAME/$*/g < _run_test.c | ${CC} -o $@ ${CFLAGS} -x c - 

## testing when interception points get called in library constructors:

test_library_constructors: $(foreach f,${TEST_SNIPPETS},test_lib_${f})
test_lib_%: test_constructors.sh use_lib_% lib%.so
	./test_constructors.sh $*

lib%.so: _libtest.c snippets/%.c
	sed s/SNIPPET_NAME/$*/g < _libtest.c | ${CC} -shared -o $@ -fpic ${CFLAGS} -x c -

use_lib_%: _use_lib_test.c snippets/%.c lib%.so
	sed s/SNIPPET_NAME/$*/g < _use_lib_test.c | ${CC} -L. -o $@ ${CFLAGS} -x c - -l$*


## cleanup and metainformation

clean:
	@rm -f ${OBJ} timetest getrandom_test syscall_test $(foreach f,${TEST_SNIPPETS},use_lib_${f} lib${f}.so run_${f})

distclean: clean
	@echo

.PHONY: all test clean distclean randomtest
