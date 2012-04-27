#!/bin/bash
collfs_dir_rel=`dirname $0`/..
collfs_dir=`readlink -f $collfs_dir_rel`
tmpdir=`pwd`
prefix=/home/$USER/bgqsys
build=powerpc64-linux-gnu

glibc_src_dir=$tmpdir/glibc-2.12.2

# download_and_patch_source
#rm -rf $tmpdir
# mkdir $tmpdir

cd $tmpdir
#if !skip_download_and_patch
#curl -O ftp://ftp.gnu.org/gnu/glibc/glibc-2.12.2.tar.gz
#tar -zxvf glibc-2.12.2.tar.gz
#or
rm -rf glibc-2.12.2
tar -zxvf /bgsys/downloads/glibc-2.12.2.tar.gz
#endif 

# apply IBM patches to glibc
# if you do not have the below patch file
# you may need the latest driver RPM from http://wiki.bg.anl-external.org/index.php/Main_Page
# specifically http://bg-toolchain.anl-external.org/wiki/images/7/78/BgptoolchainSA_V1R4M2-2010.1.4-2.ppc64.rpm

patch -d glibc-2.12.2 -p0 -E < /bgsys/drivers/ppcfloor/toolchain/glibc-2.12.2.diff

# temporary linux headers
#mkdir -p $tmpdir/templinuxheaders-build/include && \
#cp -f -r -L /usr/include/asm \
#        $tmpdir/templinuxheaders-build/include/asm && \
#cp -f -r -L /usr/include/asm-ppc \
#        $tmpdir/templinuxheaders-build/include/asm-ppc && \
#cp -f -r -L /usr/include/asm-generic \
#       $tmpdir/templinuxheaders-build/include/asm-generic && \
#cp -f -r -L /usr/include/linux \
#        $tmpdir/templinuxheaders-build/include/linux

# patch makeconfig
cp $collfs_dir/glibc-2.12.2-bgq-patches/Makeconfig $glibc_src_dir

# apply collfs patches
cp $collfs_dir/glibc-2.12.2-bgq-patches/elf/* $glibc_src_dir/elf

# configure using default-shared gcc specs
rm -rf $tmpdir/glibc-2.12.2-build
mkdir -p $tmpdir/glibc-2.12.2-build
cd $tmpdir/glibc-2.12.2-build &&  \
    PATH=/usr/gnu/bin:/usr/bin:/bin:/bgsys/drivers/toolchain/V1R1M0-/gnu-linux/powerpc64-bgq-linux/bin:/bgsys/drivers/V1R1M0/ppc64/gnu-linux/bin LD_LIBRARY_PATH=    \
    CC="powerpc64-bgq-linux-gcc -g -lgcc_eh -lgcc -specs=/bgsys/drivers/toolchain/V1R1M0-/gnu-linux/lib/gcc/powerpc64-bgq-linux/4.4.6/specs.orig" \
    libc_cv_ppc_machine=yes  \
    libc_cv_forced_unwind=yes  \
    libc_cv_c_cleanup=yes  \
    libc_cv_powerpc64_tls=yes \
    libc_cv_slibdir="/lib" \
    $tmpdir/glibc-2.12.2/configure   \
    --prefix=$prefix \
    --sysconfdir="/etc"   \
    --libdir="/lib" \
    --bindir="/bin"  \
    --datadir="/usr/share" \
    --libexecdir="/libexec"   \
    --build=$build  \
    --host=powerpc64-bgq-linux   \
    --enable-shared  \
    --enable-static-nss \
    --disable-multilib \
    --without-cvs \
    --without-gd \
    --with-elf \
    --with-fp=yes \
    --enable-add-ons=nptl \
    --with-tls \
    --with-__thread   \
    --enable-__cxa_atexit \
    --with-headers=/bgsys/drivers/V1R1M0/ppc64/toolchain/gnu/build-powerpc64-bgq-linux/templinuxheaders-build/include \
    --with-binutils=/bgsys/drivers/toolchain/V1R1M0-/gnu-linux/powerpc64-bgq-linux/bin

# make
cd $tmpdir/glibc-2.12.2-build && PATH=/bin:/usr/bin:/bgsys/drivers/V1R1M0/ppc64/gnu-linux/bin:/bgsys/drivers/toolchain/V1R1M0-/gnu-linux/powerpc64-bgq-linux/bin LD_LIBRARY_PATH= \
   make -j4 2>&1 | tee make.log

# make install
cd $tmpdir/glibc-2.12.2-build && PATH=/bgsys/drivers/toolchain/V1R1M0-/gnu-linux/bin:/bgsys/drivers/toolchain/V1R1M0-/gnu-linux/powerpc64-bgq-linux/bin:/bin:/usr/bin LD_LIBRARY_PATH= \
   make install 2>&1 | tee install.log
