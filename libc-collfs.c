#include "libc-collfs.h"
#include "libc-collfs-private.h"
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

/* Declare the unwrapped private libc API */
extern int __fxstat64(int vers, int fd, struct stat64 *buf);
extern int __xstat64 (int vers, const char *file, struct stat64 *buf);
extern int __open (__const char *__file, int __oflag, mode_t __mode);
extern int __close(int fd);
#if COLLFS_IN_LIBC              /* I do not know what is correct here, but __mmap and __munmap is not . */
extern void *__mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
extern int __munmap (__ptr_t addr, size_t len);
#else
static void *__mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off) {return mmap(addr,len,prot,flags,fildes,off);}
static int __munmap(__ptr_t addr, size_t len) {return munmap(addr, len);}
#endif
extern off_t __lseek(int fildes, off_t offset, int whence);
extern ssize_t __read(int fd,void *buf,size_t count);

/* Struct containing the collfs API used by libc_collfs, provided to libc_collfs_initialize() */
static struct libc_collfs_api collfs;

/* This function pointer is provided by libcollfs */
static collfs_debug_vprintf_fp collfs_debug_vprintf;

/* Always use this function for debugging. */
__attribute__((format(printf, 2, 3)))
static int debug_printf(int level, const char *fmt, ...)
{
  va_list ap;
  int ret = 0;
  va_start(ap, fmt);
  if (collfs_debug_vprintf) ret = collfs_debug_vprintf(level, fmt, ap);
  va_end(ap);
  return ret;
}

/***********************************************************************
 * The API used by libcollfs
 ***********************************************************************/

/* Provide callbacks for the filesystem API, optionally get access to the unwrapped API. */
int libc_collfs_initialize(collfs_debug_vprintf_fp vprintf_, const struct libc_collfs_api *api, struct libc_collfs_api *unwrap)
{
  collfs_debug_vprintf = vprintf_;
  memcpy(&collfs, api, sizeof(struct libc_collfs_api));
  debug_printf(1, "%s: registered external API", __func__);
  if (unwrap) {
    unwrap->fxstat64 = __fxstat64;
    unwrap->xstat64  = __xstat64;
    unwrap->open     = __open;
    unwrap->close    = __close;
    unwrap->read     = __read;
    unwrap->lseek    = __lseek;
    unwrap->mmap     = __mmap;
    unwrap->munmap   = __munmap;
    debug_printf(1, "%s: provided unwrapped API", __func__);
  }
  return 0;
}

/* Disable collfs redirect. */
int libc_collfs_finalize(void)
{
  memset(&collfs,0,sizeof(collfs));
  debug_printf(1, "%s: Deregistered external API", __func__);
  collfs_debug_vprintf = 0;
  return 0;
}

/***********************************************************************
 * Wrappers for the file IO functions in libc
 *
 * Compilation flags are needed to wrap these appropriately so that they
 * get called instead of the unwrapped versions.
 ***********************************************************************/

int __collfs_fxstat64(int vers, int fd, struct stat64 *buf)
{
  if (collfs.fxstat64) {
    debug_printf(3,"%s(%d, %d, %p): redirected", __func__, vers, fd, (void*)buf);
    return collfs.fxstat64(vers, fd, buf);
  } else {
    debug_printf(3,"%s(%d, %d, %p): libc", __func__, vers, fd, (void*)buf);
    return __fxstat64(vers, fd, buf);
  }
}

int __collfs_xstat64(int vers, const char *file, struct stat64 *buf)
{
  if (collfs.xstat64) {
    debug_printf(3, "%s(%d, \"%s\", %p): redirected to collfs", __func__, vers, file, (void*)buf);
    return collfs.xstat64(vers, file, buf);
  } else {
    debug_printf(3, "%s(%d, \"%s\", %p): libc", __func__, vers, file, (void*)buf);
    return __xstat64(vers, file, buf);
  }
}

int __collfs_open(const char *pathname, int flags, ...)
{
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
  }
  if (collfs.open) {
    debug_printf(3, "%s(\"%s\", %d, %d): redirected to collfs", __func__, pathname, flags, mode);
    return collfs.open(pathname, flags, mode);
  } else {
    debug_printf(3, "%s(\"%s\", %d, %d): libc", __func__, pathname, flags, mode);
    return __open(pathname, flags, mode);
  }
}

int __collfs_close(int fd)
{
  if (collfs.close) {
    debug_printf(3, "%s(%d): redirected to collfs", __func__, fd);
    return collfs.close(fd);
  } else {
    debug_printf(3, "%s(%d): libc", __func__, fd);
    return __close(fd);
  }
}

ssize_t __collfs_read(int fd, void *buf, size_t count)
{
  if (collfs.read) {
    debug_printf(3, "%s(%d, %p, %zu): redirected to collfs", __func__, fd, buf, count);
    return collfs.read(fd, buf, count);
  } else {
    debug_printf(3, "%s(%d, %p, %zu): libc", __func__, fd, buf, count);
    return __read(fd, buf, count);
  }
}

off_t __collfs_lseek(int fd, off_t offset, int whence)
{
  if (collfs.lseek) {
    debug_printf(3, "%s(%d, %lld, %d): redirected to collfs", __func__, fd, (long long)offset, whence);
    return collfs.lseek(fd, offset, whence);
  } else {
    debug_printf(3, "%s(%d, %lld, %d): libc", __func__, fd, (long long)offset, whence);
    return __lseek(fd, offset, whence);
  }
}

void *__collfs_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
  if (collfs.mmap) {
    debug_printf(3, "%s(%p, %zd, %d, %d, %d, %lld): redirected to collfs", __func__, addr, len, prot, flags, fd, (long long)off);
    return collfs.mmap(addr, len, prot, flags, fd, off);
  } else {
    debug_printf(3, "%s(%p, %zd, %d, %d, %d, %lld): libc", __func__, addr, len, prot, flags, fd, (long long)off);
    return __mmap(addr, len, prot, flags, fd, off);
  }
}

int __collfs_munmap (void *addr, size_t len)
{
  if (collfs.munmap) {
    debug_printf(3, "%s(%p, %zd): redirected to collfs", __func__, addr, len);
    return collfs.munmap(addr, len);
  } else {
    debug_printf(3, "%s(%p, %zd): libc", __func__, addr, len);
    return __munmap(addr, len);
  }
}
