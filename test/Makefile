CC = gcc

all: testprog test

testprog: timetest.c
	${CC} -DFAKE_STAT -o timetest timetest.c -lrt

test: testprog
	@echo
	@./test.sh

clean: 
	@rm -f timetest

distclean: clean
	@echo 

.PHONY: all testprog test clean distclean
