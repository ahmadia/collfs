CC = gcc
MPICC = /opt/mpich2/bin/mpicc
CFLAGS = -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG}
CFLAGS_DEBUG = -g3 -DDEBUG=1
LDFLAGS_WRAP = -Wl,-wrap,open -Wl,-wrap,close -Wl,-wrap,read
LIBDL = -ldl #/usr/lib/libdl.a # libdlwrap.so		# Wrapped substitute for -ldl

COLLFS_SRC_C = collfswrap.c
COLLFS_SRC_O = $(COLLFS_SRC_C:.c=.o)

all : libthefunc.so main

libthefunc.so : thefunc.o
	${CC} -shared -g3 -o $@ $^
thefunc.o : thefunc.c
	${CC} ${CFLAGS} -c -fPIC $^

main : main.o libfoo.so
	${MPICC} -g3 -o $@ $^ -ldl
.c.o :
	${MPICC} ${CFLAGS} -fPIC -c $^

libfoo.so : foo.o collfswrap.o
	${MPICC} -g3 -shared -o $@ $^ ${LDFLAGS_WRAP}

libcollfspreload.so : collfswrap.c
	${MPICC} ${CFLAGS} -shared -o $@ $^ -DCOLLFS_PRELOAD=1 -ldl

clean :
	rm -f *.o *.so main

.PHONY: clean all
