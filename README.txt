***********************************************************************************************
# What is collfs?

collfs is a user library and set of patches to glibc that enables file system calls to be handled collectively over an
MPI Communicator.  collfs was written with the specific goal of providing scalable dynamic loading on supercomputers
with faster interconnect than i/o performance, but is a general-purpose tool that can be used either directly as a
library or implicitly by wrapping file system calls with collective versions.

The collfs patches are currently quite minimal.  They insert an externally visible struct object into the run-time
dynamic linker containing void function hooks for collective MPI versions of the standard file system API.  These function pointers
are then used (when non-NULL) to replace file system function calls within the standard dynamic loading routines in glibc.  Additionally,
we provide a small static C library via LD_PRELOAD that initializes MPI (this is pre-main, so it passes NULL to the Init
routines) and activates the function hooks to point to our collective versions of the standard file system API,
shadowing the original file system functions (but utilizing them by default).  

***********************************************************************************************
# Installing collfs on a Fedora Core 5 system

Fedora Core 5 is at this point, quite old, but it features a 2.4 version of glibc, which is very close to the 2.4 glibc
installed on the IBM Blue Gene/P.

This assumes you have an FC5 installation with the standard development RPMs installed.  If you are unable to build
glibc-2.4 from the SRPM, you will probably need to install the missing RPMs from the DVD or a web repository.

Tips for setting up a non-root installation environment for SRPMs can be found here:
http://www.owlriver.com/tips/non-root//

mkdir -p ~/sandbox/glibc
cd ~/sandbox/glibc
wget http://dl.dropbox.com/u/65439/glibc-2.4-11-collfs.src.rpm
rpmbuild --nodeps --rebuild -bc glibc-2.4-11-collfs.src.rpm

This should leave you with a built glibc (good for testing) in the RPM build directory, which
was/var/tmp/glibc-2.4-root on my machine.

***********************************************************************************************
# Building glibc-2.4 for the compute nodes:

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
cd $tmpdir/glibc-2.4-build && PATH=/usr/gnu/bin:/usr/bin:/bin:/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/gnu-linux/bin/ LD_LIBRARY_PATH= \
    make 2>&1 | tee make.log

# make install
cd $tmpdir/glibc-2.4-build && PATH=/usr/gnu/bin:/usr/bin:/bin:/bgsys/drivers/V1R4M2_200_2010-100508P/ppc/gnu-linux/bin/ LD_LIBRARY_PATH= \
    make install 2>&1 | tee install.log


***********************************************************************************************
See shaheen/build_glibc_2.4-sles-10.sh for the basic idea of how to patch the SLES 10
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
