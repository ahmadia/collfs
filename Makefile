CC = gcc -m64 
#MPICC = /opt/mpich2/bin/mpicc
MPICC = gcc -m64 
CFLAGS = -I/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/comm/default/include -I/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/comm/sys/include -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG} 
#CFLAGS = -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG} 
CFLAGS_DEBUG = -g3 -DDEBUG=1
LDFLAGS =  -Wl,-Bdynamic -Wl,-rpath,/home/aron/sys/lib -L/home/aron/sys/lib ${LIBDL} 
LIBDL = -ldl 
COLLFS_SRC_C = collfs.c
COLLFS_SRC_O = $(COLLFS_SRC_C:.c=.o)

all : libthefunc.so main

libthefunc.so : thefunc.o
	${CC} -shared -g3 -o $@ $^
thefunc.o : thefunc.c
	${CC} ${CFLAGS} -c -fPIC $^

main : main.o libfoo.so
	${MPICC} -g3 -o $@ $^ ${LDFLAGS} 
.c.o :
	${MPICC} ${CFLAGS} -fPIC -c $^

libfoo.so : foo.o collfs.o
	${MPICC} -g3 -shared -o $@ $^ ${LDFLAGS}

clean :
	rm -f *.o *.so main

.PHONY: clean all
