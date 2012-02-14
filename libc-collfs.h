#ifndef _dlcollfs_h
#define _dlcollfs_h

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

/* The libc printf cannot be called directly from libdl, so call via this function pointer when appropriate. */
typedef int (*collfs_debug_vprintf_fp)(int level,const char *fmt,va_list ap);

/* The wrapped API */
typedef int (*collfs_fxstat64_fp)(int vers, int fd, struct stat64 *buf);
typedef int (*collfs_xstat64_fp)(int vers, const char *file, struct stat64 *buf);
typedef int (*collfs_open_fp)(const char *pathname, int flags, mode_t mode);
typedef int (*collfs_close_fp)(int fd);
typedef ssize_t (*collfs_read_fp)(int fd, void *buf, size_t count);
typedef off_t (*collfs_lseek_fp)(int fildes, off_t offset, int whence);
typedef void *(*collfs_mmap_fp)(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
typedef int (*collfs_munmap_fp)(__ptr_t addr, size_t len);

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


#endif
