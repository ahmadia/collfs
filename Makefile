#CC = gcc -m64 
CC = mpicc
#MPICC = /opt/mpich2/bin/mpicc
#MPICC = gcc -m64 
MPICC = mpicc
#CFLAGS = -I/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/comm/default/include -I/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/comm/sys/include -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG} 
CFLAGS = -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG} -D_LARGEFILE64_SOURCE
CFLAGS_DEBUG = -g3 -DDEBUG=1
#LDFLAGS =  -Wl,-Bdynamic -Wl,-rpath,/home/aron/sys/lib -L/home/aron/sys/lib ${LIBDL} 
#LDFLAGS = -dynamic -Wl,-Bdynamic -Wl,-rpath,/home/aron/bgpsys/lib -L/home/aron/bgpsys/lib ${LIBDL} 
LDFLAGS = -dynamic -Wl,-Bdynamic -Wl,-rpath,${LIBCPATH} -L${LIBCPATH} ${LIBDL} 
LIBDL = -ldl 
COLLFS_SRC_C = collfs.c
COLLFS_SRC_O = $(COLLFS_SRC_C:.c=.o)

all : libthefunc.so main

libc-collfs.so : libc-collfs.o
	${CC} -shared -g3 -o $@ $^
libcollfs.so : collfs.o
	${CC} -shared -g3 -o $@ $^
libminimal_thefunc.so: minimal_thefunc.o
	${CC} -shared -g3 -o $@ $^
libthefunc.so : thefunc.o
	${CC} -shared -g3 -o $@ $^
thefunc.o : thefunc.c
	${CC} ${CFLAGS} -c -fPIC $^

minimal_main : minimal_main.o libminimal_thefunc.so
	${MPICC} -g3 -o $@ minimal_main.o ${LDFLAGS} 

main : main.o libfoo.so libcollfs.so 
#main : main.o libfoo.so libcollfs.so libc-collfs.so
	${MPICC} -g3 -o $@ $^ ${LDFLAGS} 
.c.o :
	${CC} ${CFLAGS} -fPIC -c $^

libfoo.so : foo.o collfs.o
	${MPICC} -g3 -shared -o $@ $^ ${LDFLAGS}

collfs.o : collfs.c collfs.h libc-collfs.h
libc-collfs.o : libc-collfs.c libc-collfs.h libc-collfs-private.h
foo.o : foo.c foo.h errmacros.h collfs.h libc-collfs-private.h
main.o : main.c errmacros.h
thefunc.o : thefunc.h collfs.h

clean :
	rm -f *.o *.so main

.PHONY: clean all
