CC = gcc
MPICC = /opt/mpich2/bin/mpicc
CFLAGS = -std=c99 -Wall -Wextra ${CFLAGS_DEBUG}
CFLAGS_DEBUG = -g3 -DDEBUG=1
LDFLAGS_WRAP = -Wl,-wrap,open -Wl,-wrap,close -Wl,-wrap,read

all : libthefunc.so main

libthefunc.so : thefunc.o
	${CC} -shared -g3 -o $@ $^
thefunc.o : thefunc.c
	${CC} ${CFLAGS} -c -fPIC $^

main : main.o wrappers.o
	${MPICC} -g3 -o $@ $^ -ldl -lrt ${LDFLAGS_WRAP}
.c.o :
	${MPICC} ${CFLAGS} -c $^

clean :
	rm -f *.o *.so main

.PHONY: clean all
