CC = gcc
MPICC = mpicc

#CC = gcc
#MPICC = /opt/mpich2/bin/mpicc

CFLAGS = -std=c99 -fPIC -Wall -Wextra ${CFLAGS_DEBUG} -D_LARGEFILE64_SOURCE
CFLAGS_DEBUG = -g3 -DDEBUG=1
LIBCPATH = /lib/x86_64-linux-gnu
LIBDL = -ldl
LDSO = ld-2.13.so
# on Arch Linux, mpich is statically linked, so you need to explicitly bring in libdl
# on the other hand, openmpi is dynamically linked, so bringing in libdl before libc will break things
# so don't implicitly bring in libc, explicitly link it before libdl.
LDFLAGS = -dynamic -Wl,-Bdynamic -Wl,-rpath,${LIBCPATH} -L${LIBCPATH} -lc ${LIBDL} ${LIBCPATH}/${LDSO} -Wl,-rpath,${PWD} -Wl,--dynamic-linker=${LIBCPATH}/${LDSO}
