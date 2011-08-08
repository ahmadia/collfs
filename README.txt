Hi!  

***********************************************************************************************
some random notes for Shaheen:

../configure --prefix=/home/aron/sys --libexecdir=/home/aron/sys/lib64 --infodir=/home/aron/sys/share/info --enable-add-ons=nptl,libidn,dceext --srcdir=.. --without-cvs --with-headers=/home/aron/src/rpm/BUILD/kernel-headers --build ppc64-suse-linux --host ppc64-suse-linux --with-tls --with-__thread --enable-kernel=2.6.4  2>&1 CC="gcc -m64" | tee configure.log

make 2>&1 | tee make.log

# make install wants a local ld.so.conf, so copy the one over from /etc or build it by hand
cp ~/sandbox/collfs.git/shaheen/ld.so.conf ~/sys/etc/ld.so.conf
make install

***********************************************************************************************
