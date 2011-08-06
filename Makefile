CC = gcc
MPICC = mpicc
CFLAGS = -std=c99 -Wall -Wextra

all : libthefunc.so main

libthefunc.so : thefunc.o
	${CC} -shared -o $@ $^
thefunc.o : thefunc.c
	${CC} ${CFLAGS} -c -fPIC $^

main : main.o
	${MPICC} -o $@ $^ -ldl
.c.o :
	${MPICC} ${CFLAGS} -c $^

clean :
	rm -f *.o *.so main

.PHONY: clean all
