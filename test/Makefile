CC = gcc

CFLAGS += -std=gnu99 -Wall -DFAKE_STAT -Werror -Wextra $(FAKETIME_COMPILE_CFLAGS) -U_FILE_OFFSET_BITS -U_TIME_BITS
LDFLAGS += -lrt -lpthread

SRC = timetest.c
OBJ = ${SRC:.c=.o}

TEST_SNIPPETS = $(notdir $(basename $(wildcard snippets/*.c)))
EXPECTATIONS = $(notdir $(basename $(wildcard snippets/*.variable)))

ALL_TESTS = timetest test shm_layout_test set_mtime time_ops_test clock_consistency_test clock_error_test wait_contract_test wait_api_contract_test sem_contract_test wait_timeout_contract_test timerfd_contract_test

ifneq ($(filter -DINTERCEPT_SYSCALL,${CFLAGS}),)
ALL_TESTS += confirm_variadic_promotion syscall_contract_test
SYSCALL_TEST_DEP = syscall_contract_test
else
TEST_SNIPPETS := $(filter-out syscall%,${TEST_SNIPPETS})
EXPECTATIONS := $(filter-out syscall%,${EXPECTATIONS})
endif

ifneq ($(filter -DINTERCEPT_FUTEX,${CFLAGS}),)
ALL_TESTS += futex_contract_test
FUTEX_TEST_DEP = futex_contract_test
endif

ifneq ($(filter -DFAKE_PID,${CFLAGS}),)
ALL_TESTS += fake_pid_test fake_pid_contract_test
FAKE_PID_TEST_DEP = fake_pid_contract_test
endif

ifneq ($(filter -DFAKE_RANDOM,${CFLAGS}),)
ALL_TESTS += random_test
RANDOM_TEST_DEP = random_test
endif

ifneq ($(filter -DFAKE_FILE_TIMESTAMPS -DFAKE_UTIME,${CFLAGS}),)
ALL_TESTS += file_timestamp_test
FILE_TIMESTAMP_TEST_DEP = file_timestamp_test
endif

all: $(ALL_TESTS) init_reentry.so

.c.o:
	${CC} -c ${CFLAGS} $<

timetest: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

shm_layout_test: shm_layout_test.c ../src/faketime_common.h
	${CC} -o $@ ${CFLAGS} $<

set_mtime: set_mtime.c
	${CC} -o $@ ${CFLAGS} $<

time_ops_test: time_ops_test.c ../src/time_ops.h
	${CC} -o $@ ${CFLAGS} $<

clock_consistency_test: clock_consistency_test.c
	${CC} -o $@ ${CFLAGS} $<

clock_error_test: clock_error_test.c
	${CC} -o $@ ${CFLAGS} $<

wait_contract_test: wait_contract_test.c
	${CC} -o $@ ${CFLAGS} $<

wait_api_contract_test: wait_api_contract_test.c
	${CC} -o $@ ${CFLAGS} $<

timerfd_contract_test: timerfd_contract_test.c
	${CC} -o $@ ${CFLAGS} $<

sem_contract_test: sem_contract_test.c
	${CC} -o $@ ${CFLAGS} $< ${LDFLAGS}

wait_timeout_contract_test: wait_timeout_contract_test.c
	${CC} -o $@ ${CFLAGS} $< ${LDFLAGS}

init_reentry.so: init_reentry.c
	${CC} -shared -fPIC -o $@ ${CFLAGS} $<

fake_pid_test: fake_pid_test.c
	${CC} -o $@ ${CFLAGS} $<

fake_pid_contract_test: fake_pid_contract_test.c
	${CC} -o $@ ${CFLAGS} $<

random_test: random_test.c
	${CC} -o $@ ${CFLAGS} $<

file_timestamp_test: file_timestamp_test.c
	${CC} -o $@ ${CFLAGS} $<

syscall_contract_test: syscall_contract_test.c
	${CC} -o $@ ${CFLAGS} $<

futex_contract_test: futex_contract_test.c
	${CC} -o $@ ${CFLAGS} $<

test: timetest time_ops_test clock_consistency_test set_mtime clock_error_test wait_contract_test wait_api_contract_test sem_contract_test wait_timeout_contract_test timerfd_contract_test init_reentry.so ${RANDOM_TEST_DEP} ${FILE_TIMESTAMP_TEST_DEP} ${FAKE_PID_TEST_DEP} ${SYSCALL_TEST_DEP} ${FUTEX_TEST_DEP} functest
	@echo
	@./time_ops_test
	@./test.sh

# run functional tests
functest:
	./testframe.sh functests

%_test: %_test.c
	${CC} -o $@ ${CFLAGS} $<

randomtest: repeat_random
	./randomtest.sh

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
	@rm -f ${OBJ} timetest shm_layout_test set_mtime time_ops_test clock_consistency_test clock_error_test wait_contract_test wait_api_contract_test sem_contract_test wait_timeout_contract_test timerfd_contract_test futex_contract_test init_reentry.so fake_pid_test fake_pid_contract_test random_test file_timestamp_test syscall_contract_test getrandom_test syscall_test $(foreach f,${TEST_SNIPPETS},use_lib_${f} lib${f}.so run_${f}) variadic_promotion variadic/*.o repeat_random libmallocintercept.so

distclean: clean
	@echo

.PHONY: all test clean distclean randomtest snippets test_variable_data test_library_constructors
