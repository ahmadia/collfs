#!/bin/bash
prefix=/home/aron/sys
kernel_headers_dir=/home/aron/src/rpm/BUILD/kernel-headers
collfs_dir=`dirname $0`/..
glibc_src_dir=/tmp/aron-rpm-build/glibc-2.4_patch/

cp $collfs_dir/collfs.c $glibc_src_dir/elf/dl-collfs.c 
cp $collfs_dir/collfs.h $glibc_src_dir/elf/collfs.h
cp $collfs_dir/glibc-2.4-sles-10-patches/elf/* $glibc_src_dir/elf

cd $glibc_src_dir
rm -rf build
mkdir build 
cd build

../configure --prefix=$prefix --libexecdir=$prefix/lib64 --infodir=$prefix/share/info --enable-add-ons=nptl,libidn,dceext --srcdir=.. --without-cvs --with-headers=${kernel_headers_dir} --build ppc64-suse-linux --host ppc64-suse-linux --with-tls --with-__thread --enable-kernel=2.6.4  2>&1 CC="gcc -m64" | tee configure.log

make 2>&1 | tee make.log

cp ld.so.conf $prefix/etc/ld.so.conf

make install