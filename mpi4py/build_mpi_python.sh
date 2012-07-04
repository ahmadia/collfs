#!/usr/bin/env zsh

LIBCPATH=/home/aron/opt/lib
LIBDL=-ldl
LDSO=ld-2.15.so

LDFLAGS="-L/home/aron/sandbox/collfs -Wl,-rpath,/home/aron/sandbox/collfs -lcollfs -Wl,-rpath,${LIBCPATH} -L${LIBCPATH} -lc ${LIBDL} ${LIBCPATH}/${LDSO} -Wl,-rpath,${PWD} -Wl,--dynamic-linker=${LIBCPATH}/${LDSO}" CFLAGS="-std=c99 -fPIC -Wall -Wextra -g3 -DDEBUG=1 -D_LARGEFILE64_SOURCE -I/home/aron/sandbox/collfs" python setup.py install