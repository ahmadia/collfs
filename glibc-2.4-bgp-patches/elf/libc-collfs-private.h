#ifndef _collfs_private_h
#define _collfs_private_h

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int __collfs_fxstat64(int vers, int fd, struct stat64 *buf);
int __collfs_xstat64(int vers, const char *file, struct stat64 *buf);
void *__collfs_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
int __collfs_munmap(__ptr_t addr, size_t len);
off_t __collfs_lseek(int fildes, off_t offset, int whence);
int __collfs_open(const char *pathname, int flags, ...);
int __collfs_close(int fd);
ssize_t __collfs_read(int fd, void *buf, size_t count);

#ifdef COLLFS_INTERCEPT
#  define __fxstat64(vers, fd, buf)                   __collfs_fxstat64(vers, fd, buf)
#  define __xstat64(vers, file, buf)                  __collfs_xstat64(vers, file, buf)
#  define __mmap(addr, len, prot, flags, fildes, off) __collfs_mmap(addr, len, prot, flags, fildes, off)
#  define __unmap(addr, len)                          __collfs_munmap(addr,len)
#  define __lseek(fildes, offset, whence)             __collfs_lseek(fildes, offset, whence)
#  define __open(pathname,flags)                      __collfs_open(pathname,flags)
#  define __close(fd)                                 __collfs_close(fd)
#  define __libc_read(fd, buf, count)                 __collfs_read(fd, buf, count)
#endif

#endif
