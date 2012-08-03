#ifndef _collfs_private_h
#define _collfs_private_h

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct libc_collfs_api {
  void (*fxstat64)(void);
  void (*xstat64)(void);
  void (*open)(void);
  void (*close)(void);
  void (*read)(void);
  void (*lseek)(void);
  void (*mmap)(void);
  void (*munmap)(void);
};

extern struct libc_collfs_api _dl_collfs_api;

typedef int (*collfs_fxstat64_fp)(int vers, int fd, struct stat64 *buf);
typedef int (*collfs_xstat64_fp)(int vers, const char *file, struct stat64 *buf);
typedef int (*collfs_open_fp)(const char *pathname, int flags, mode_t mode);
typedef int (*collfs_close_fp)(int fd);
typedef ssize_t (*collfs_read_fp)(int fd, void *buf, size_t count);
typedef off_t (*collfs_lseek_fp)(int fildes, off_t offset, int whence);
typedef void *(*collfs_mmap_fp)(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
typedef int (*collfs_munmap_fp)(__ptr_t addr, size_t len);

#define __collfs_open(pathname,flags)                                 \
  ((_dl_collfs_api.open != NULL) ?                                      \
   ((collfs_open_fp) _dl_collfs_api.open)(pathname,flags,O_RDONLY) : open(pathname, flags))

#define __collfs_close(fd)                                    \
  ((_dl_collfs_api.close != NULL) ?                           \
   ((collfs_close_fp) _dl_collfs_api.close)(fd) : close(fd))                              

#define __collfs_lseek(fildes, offset, whence)                          \
  ((_dl_collfs_api.lseek != NULL) ?                                     \
   ((collfs_lseek_fp) _dl_collfs_api.lseek)(fildes, offset, whence) : lseek(fildes, offset, whence))

#define __collfs_fxstat64(vers, fd, buf)                                \
  ((_dl_collfs_api.fxstat64 != NULL) ?                                  \
   ((collfs_fxstat64_fp) _dl_collfs_api.fxstat64)(vers, fd, buf) : fxstat64(vers, fd, buf))

#define __collfs_xstat64(vers, file, buf)                               \
  ((_dl_collfs_api.xstat64 != NULL) ?                                   \
   ((collfs_xstat64_fp) _dl_collfs_api.xstat64)(vers, file, buf) : xstat64(vers, file, buf))

#define __collfs_libc_read(fd, buf, count)                              \
  ((_dl_collfs_api.read != NULL) ?                                      \
   ((collfs_read_fp) _dl_collfs_api.read)(fd, buf, count) : read(fd, buf, count))

#define __collfs_mmap(addr, len, prot, flags, fildes, off)              \
  ((_dl_collfs_api.mmap != NULL) ?                                      \
   ((collfs_mmap_fp) _dl_collfs_api.mmap)(addr, len, prot, flags, fildes, off) : mmap(addr, len, prot, flags, fildes, off))   

#define __collfs_munmap(addr, len)                                      \
  ((_dl_collfs_api.munmap != NULL) ?                                    \
   ((collfs_munmap_fp) _dl_collfs_api.munmap)(addr, len) : munmap(addr, len))       

#endif
