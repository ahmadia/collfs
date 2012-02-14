#CC = gcc -m64 
CC = gcc
#MPICC = /opt/mpich2/bin/mpicc
#MPICC = gcc -m64 
MPICC = mpicc
#CFLAGS = -I/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/comm/default/include -I/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/comm/sys/include -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG} 
CFLAGS = -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG} -D_LARGEFILE64_SOURCE
CFLAGS_DEBUG = -g3 -DDEBUG=1
LIBCPATH ?= /lib
LIBDL ?= -ldl
#LDFLAGS =  -Wl,-Bdynamic -Wl,-rpath,/home/aron/sys/lib -L/home/aron/sys/lib ${LIBDL} 
#LDFLAGS = -dynamic -Wl,-Bdynamic -Wl,-rpath,/home/aron/bgpsys/lib -L/home/aron/bgpsys/lib ${LIBDL} 
LDFLAGS = -dynamic -Wl,-Bdynamic -Wl,-rpath,${LIBCPATH} -L${LIBCPATH} ${LIBDL}
LIBDL = -ldl 
COLLFS_SRC_C = collfs.c
COLLFS_SRC_O = $(COLLFS_SRC_C:.c=.o)

# This option should be set to use the patched ld.so
#LDCOLLFSFLAGS = -Wl,--dynamic-loader=/path/to/ld-collfs.so

all : libthefunc.so main

# Depends on MPI, must be called from an executable using patched ld.so because _dl_collfs_api is referenced.
libcollfs.so : collfs.o
	${MPICC} -shared -g3 -o $@ $^

# Depends on libcollfs.so and MPI, can be preloaded to make an ignorant program use collective IO
libcollfs-easy.so : collfs-easy.o libcollfs.so
	${MPICC} -shared -g3 -o $@ $^

# Depends only on libc, just prints "called thefunc"
libminimal_thefunc.so: minimal_thefunc.o
	${CC} -shared -g3 -o $@ $^
libthefunc.so : thefunc.o
	${CC} -shared -g3 -o $@ $^

minimal_main : minimal_main.o libminimal_thefunc.so
	${MPICC} -g3 -o $@ minimal_main.o ${LDFLAGS}

# Explicitly uses libcollfs to push communicators. Loads libminimal_thefunc.so (so only a run-time dependency)
main-mpi : main-mpi.o libcollfs.so libminimal_thefunc.so
	${MPICC} -g3 -o $@ $< libcollfs.so ${LDFLAGS} ${LDCOLLFSFLAGS}

main : main.o libfoo.so libcollfs.so 
#main : main.o libfoo.so libcollfs.so libc-collfs.so
	${MPICC} -g3 -o $@ $^ ${LDFLAGS} 
.c.o :
	${CC} ${CFLAGS} -fPIC -c $<

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
