***********************************************************************************************
Building glibc-2.4 for the compute nodes:

tmpdir=/tmp/aron-glibc-bgp-build
prefix=/home/aron/bgpsys
build=powerpc-linux-gnu
# download and patch glibc sources

cd $tmpdir
curl -O ftp://ftp.gnu.org/gnu/glibc/glibc-2.4.tar.gz
tar -zxvf glibc-2.4.tar.gz

# apply IBM patches to glibc

patch -p2 -E < /bgsys/drivers/ppcfloor/toolchain/glibc-2.4.diff

# temporary linux headers
mkdir -p $tmpdir/templinuxheaders-build/include && \
cp -f -r -L /usr/include/asm \
	 $tmpdir/templinuxheaders-build/include/asm && \
cp -f -r -L /usr/include/asm-ppc \
	 $tmpdir/templinuxheaders-build/include/asm-ppc && \
cp -f -r -L /usr/include/asm-generic \
	$tmpdir/templinuxheaders-build/include/asm-generic && \
cp -f -r -L /usr/include/linux \
	 $tmpdir/templinuxheaders-build/include/linux

# configure using default-shared gcc specs
mkdir -p $tmpdir/glibc-2.4-build
cd $tmpdir/glibc-2.4-build &&  \
    PATH=/usr/gnu/bin:/usr/bin:/bin:/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/gnu-linux/bin/ LD_LIBRARY_PATH=    \
    CC="powerpc-bgp-linux-gcc -specs=/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/gnu-linux/lib/gcc/powerpc-bgp-linux/4.1.2/specs.orig" \
    libc_cv_ppc_machine=yes  \
    dd1_workarounds=  \
    libc_cv_forced_unwind=yes  \
    libc_cv_c_cleanup=yes  \
    libc_cv_slibdir="/lib" \
    $tmpdir/glibc-2.4/configure   \
    --prefix=$prefix \
    --sysconfdir="/etc"   \
    --libdir="/lib" \
    --bindir="/bin"  \
    --datadir="/usr/share" \
    --libexecdir="/libexec"   \
    --build=$build  \
    --host=powerpc-bgp-linux   \
    --enable-shared  \
    --enable-multilib \
    --without-cvs \
    --without-gd \
    --with-elf \
    --with-fp=yes \
    --enable-add-ons=powerpc-cpu,nptl \
    --with-cpu=450fp2   \
    --with-tls \
    --with-__thread   \
    --enable-__cxa_atexit \
    --with-headers=$tmpdir/templinuxheaders-build/include

# patch makeconfig

# make
PATH=/usr/gnu/bin:/usr/bin:/bin:/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/gnu-linux/bin/ LD_LIBRARY_PATH= \
    make 2>&1 | tee make.log

# make install
PATH=/usr/gnu/bin:/usr/bin:/bin:/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/gnu-linux/bin/ LD_LIBRARY_PATH= \
    make install 2>&1 | tee install.log


***********************************************************************************************
See the shaheen/build_glibc_2.4-sles-10.sh for the basic idea of how to patch the SLES 10
glibc with collfs.  I am working from the patched source that you can obtain from the SLES 
10 src.rpm by unpacking the distribution then applying all the patches (just before the 
configure stage).

If you want to run an executable with the updated dynamic loader interpreter, you need to
launch your executable using the installed ld64.so.1 like this:

/home/aron/sys/lib/ld64.so.1 ./main

***********************************************************************************************
some random notes for building glibc Shaheen:

../configure --prefix=/home/aron/sys --libexecdir=/home/aron/sys/lib64 --infodir=/home/aron/sys/share/info --enable-add-ons=nptl,libidn,dceext --srcdir=.. --without-cvs --with-headers=/home/aron/src/rpm/BUILD/kernel-headers --build ppc64-suse-linux --host ppc64-suse-linux --with-tls --with-__thread --enable-kernel=2.6.4  2>&1 CC="gcc -m64" | tee configure.log

make 2>&1 | tee make.log

# make install wants a local ld.so.conf, so copy the one over from /etc or build it by hand
cp ~/sandbox/collfs.git/shaheen/ld.so.conf ~/sys/etc/ld.so.conf
make install

***********************************************************************************************
anticipated changes to:
dl-load.c

793: __close
837: __fxstat64
858: __close
887: __close
912: __close
975: __seek
976: __libc_read
1185: __mmap
1236: __mmap
1350: __munmap
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

dl-close.c

498: DL_UNMAP
***********************************************************************************************
