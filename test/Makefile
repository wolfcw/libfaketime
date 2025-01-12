CC = gcc

CFLAGS += -std=gnu99 -Wall -DFAKE_STAT -Werror -Wextra $(FAKETIME_COMPILE_CFLAGS) -U_FILE_OFFSET_BITS -U_TIME_BITS
LDFLAGS += -lrt -lpthread

SRC = timetest.c
OBJ = ${SRC:.c=.o}

TEST_SNIPPETS = $(notdir $(basename $(wildcard snippets/*.c)))
EXPECTATIONS = $(notdir $(basename $(wildcard snippets/*.variable)))

ALL_TESTS = timetest test

ifneq ($(filter -DINTERCEPT_SYSCALL,${CFLAGS}),)
ALL_TESTS += confirm_variadic_promotion
else
TEST_SNIPPETS := $(filter-out syscall%,${TEST_SNIPPETS})
EXPECTATIONS := $(filter-out syscall%,${EXPECTATIONS})
endif

all: $(ALL_TESTS)

.c.o:
	${CC} -c ${CFLAGS} $<

timetest: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

test: timetest functest libmallocintercept.so
	@echo
	@./test.sh

# run functional tests
functest:
	./testframe.sh functests

%_test: %_test.c
	${CC} -o $@ ${CFLAGS} $<

randomtest: repeat_random
	./randomtest.sh

libmallocintercept.so: libmallocintercept.c
	${CC} -shared -o $@ -fpic ${CFLAGS} $<

# ensure our variadic argument unpacking/repacking works as expected
confirm_variadic_promotion: variadic_promotion
	./variadic_promotion
variadic_promotion: variadic/main.o variadic/outer.o variadic/inner.o
	${CC} -o $@ ${CFLAGS} $^
variadic/%.o: variadic/%.c
	${CC} -c -o $@ ${CFLAGS} $<

# run snippet tests
snippets: test_variable_data test_library_constructors

## test snippet behavior across env var setting over time:
test_variable_data: test_variable_data.sh $(foreach f,${EXPECTATIONS},run_${f})
	./test_variable_data.sh ${EXPECTATIONS}

run_%: _run_test.c snippets/%.c
	sed s/SNIPPET_NAME/$*/g < _run_test.c | ${CC} -o $@ ${CFLAGS} -x c - 

## test snippets in other library constructors:
test_library_constructors: $(foreach f,${TEST_SNIPPETS},test_lib_${f})
test_lib_%: test_constructors.sh use_lib_% lib%.so
	./test_constructors.sh $*

lib%.so: _libtest.c snippets/%.c
	sed s/SNIPPET_NAME/$*/g < _libtest.c | ${CC} -shared -o $@ -fpic ${CFLAGS} -x c -

use_lib_%: _use_lib_test.c snippets/%.c lib%.so
	sed s/SNIPPET_NAME/$*/g < _use_lib_test.c | ${CC} -L. -o $@ ${CFLAGS} -x c - -l$*


## cleanup and metainformation

clean:
	@rm -f ${OBJ} timetest getrandom_test syscall_test $(foreach f,${TEST_SNIPPETS},use_lib_${f} lib${f}.so run_${f}) variadic_promotion variadic/*.o repeat_random libmallocintercept.so

distclean: clean
	@echo

.PHONY: all test clean distclean randomtest snippets test_variable_data test_library_constructors
