#ifndef _collfs_h
#define _collfs_h

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int __collfs_open(const char *pathname,int flags,...);
int __collfs_close(int fd);
int __collfs_read(int fd,void *buf,size_t count);
int __collfs_fxstat64(int vers, int fd, struct stat64 *buf);
void* __collfs_mmap(void *addr, size_t len, int prot, int flags, 
                    int fildes, off_t off);
int __collfs_munmap (__ptr_t addr, size_t len);
off_t __collfs_lseek(int fildes, off_t offset, int whence);

#endif
