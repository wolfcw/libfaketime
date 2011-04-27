CC = gcc

CFLAGS = -std=gnu99 -Wall -DFAKE_STAT
LDFLAGS = -lrt

SRC = timetest.c
OBJ = ${SRC:.c=.o}

all: timetest test

.c.o:
	${CC} -c ${CFLAGS} $<

timetest: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

test: timetest
	@echo
	@./test.sh

clean: 
	@rm -f ${OBJ} timetest

distclean: clean
	@echo 

.PHONY: all test clean distclean
