Hi!  

***********************************************************************************************
some random notes for building glibc Shaheen:

../configure --prefix=/home/aron/sys --libexecdir=/home/aron/sys/lib64 --infodir=/home/aron/sys/share/info --enable-add-ons=nptl,libidn,dceext --srcdir=.. --without-cvs --with-headers=/home/aron/src/rpm/BUILD/kernel-headers --build ppc64-suse-linux --host ppc64-suse-linux --with-tls --with-__thread --enable-kernel=2.6.4  2>&1 CC="gcc -m64" | tee configure.log

make 2>&1 | tee make.log

# make install wants a local ld.so.conf, so copy the one over from /etc or build it by hand
cp ~/sandbox/collfs.git/shaheen/ld.so.conf ~/sys/etc/ld.so.conf
make install

***********************************************************************************************
anticipated changes to dl-load.c

793: __close
837: __fxstat64
858: __close
887: __close
912: __close
975: __seek
976: __libc_read
1185: __mmap
1236: __mmap
1418: __close
1617: __open
1629: __libc_read
1727: __lseek
1728: __libc_read
1745: __lseek
1746: __libc_read
1761: __close
1872: __fxstat64
1878: __close
1899: __close
2118: __close

***********************************************************************************************
