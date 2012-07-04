#include "collfs.h"
#include "libc-collfs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>

static int collfs_initialized;

/***********************************************************************
 * Debugging and error reporting
 ***********************************************************************/

static int collfs_debug_level;
static void (*collfs_error_handler)(void);

#define ECOLLFS EREMOTEIO       /* To set errno when MPI fails */
static void set_errno(int e) { errno = e; }

/* This macro should be used to log errors encountered within this file. */
#define set_error(e, ...) set_error_private(__FILE__, __LINE__, __func__, (e), __VA_ARGS__)

__attribute__((format(printf, 5, 6)))
static void set_error_private(const char *file, int line, const char *func, int e, const char *fmt, ...)
{
  if (collfs_debug_level >= 1) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    fprintf(stderr,
            "ERROR: %s\n, errno=%d \"%s\"\n"
            "ERROR: at %s:%d in %s()\n", buf, e, strerror(e), file, line, func);
    va_end(ap);
  }
  set_errno(e);
  if (collfs_error_handler) collfs_error_handler();
}

#define CHECK_INIT(failcode) do {                               \
    int mpiinitialized;                                         \
    if (!collfs_initialized) {                                  \
      set_error(ECOLLFS, "collfs is not initialized");          \
      return (failcode);                                        \
    }                                                           \
    MPI_Initialized(&mpiinitialized);                           \
    if (!mpiinitialized) {                                      \
      set_error(ECOLLFS, "MPI has not been initialized");       \
    }                                                           \
  } while (0)

/* Do not call directly, use debug_printf() instead */
static int collfs_debug_vprintf(int level, const char *fmt, va_list ap)
{
  char buf[2048], padding[10];
  int i,rank;
  if (level > collfs_debug_level) return 0;
  vsnprintf(buf, sizeof buf, fmt, ap);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  for (i=0; i<level; i++) padding[i] = ' ';
  padding[i++] = '*';
  padding[i] = 0;
  return fprintf(stderr, "[%d] %s %s\n", rank, padding, buf);
}

__attribute__((format(printf, 2, 3)))
static int debug_printf(int level, const char *fmt, ...)
{
  va_list ap;
  int ret = 0;
  va_start(ap, fmt);
  ret = collfs_debug_vprintf(level, fmt, ap);
  va_end(ap);
  return ret;
}

static size_t extend_to_page(size_t len) {
  size_t totallen;
  long pagesize = sysconf(_SC_PAGESIZE);
  totallen = (len + (pagesize-1)) & ~(pagesize-1); /* Includes the whole page */
  debug_printf(2, "%s() len: %zu, pagesize: %ld, totallen: %zu", __func__, len, pagesize, totallen);
  return totallen;
}

/***********************************************************************
 * Data structures and static variables
 ***********************************************************************/

/* Struct containing the collfs API used by libc_collfs, provided to libc_collfs_initialize() */
static struct libc_collfs_api unwrap;

/* One node in the list for each open file */
struct FileLink {
  MPI_Comm comm;
  int fd;
  int refct;
  char fname[MAXPATHLEN];
  void *mem;
  size_t totallen;              /* bytes mapped/malloced */
  size_t len;                   /* bytes in file */
  size_t offset;
  struct FileLink *next;
};
static struct FileLink *DLOpenFiles;
static const int BaseFD = 10000;
static int NextFD = 10001;

/* One node in the list for each mmaped region. */
struct MMapLink {
  void *addr;
  size_t len;
  size_t offset;
  int fd;
  struct FileLink *link;
  struct MMapLink *next;
};
static struct MMapLink *MMapRegions;

struct CommLink {
  MPI_Comm comm;
  struct CommLink *next;
};
static struct CommLink *CommStack;

/***********************************************************************
 * Implementation of the collective file API
 *
 * These symbols need not be exported except for testing purposes because
 * they should only be called by redirect through libc-collfs.
 ***********************************************************************/

/* Logically collective on the communicator used when the fd was created */
static int collfs_fxstat64(int vers, int fd, struct stat64 *buf)
{
  int err;
  struct FileLink *link;

  CHECK_INIT(-1);
  if (CommStack) {
    debug_printf(2, "%s(%d) collective", __func__, fd);
    for (link=DLOpenFiles; link; link=link->next) {
      if (link->fd == fd) {
        int rank,xerr = 0;
        err = MPI_Comm_rank(link->comm, &rank); if (err) return -1;
        if (!rank) xerr = ((collfs_fxstat64_fp) unwrap.fxstat64)(vers, fd, buf);
        err = MPI_Bcast(&xerr, 1, MPI_INT, 0, link->comm);
        if (err < 0) {
          set_error(ECOLLFS, "MPI_Bcast failed to broadcast success of root fxstat64");
          return -1;
        }
        
        err = MPI_Bcast(buf, sizeof *buf, MPI_BYTE, 0, link->comm);
        if (err < 0) {
          set_error(ECOLLFS, "MPI_Bcast failed to broadcast struct");
          return -1;
        } else return 0;
      }
    }
  }
  return ((collfs_fxstat64_fp) unwrap.fxstat64)(vers, fd, buf);
}

/* Collective on the communicator at the top of the collfs stack */
static int collfs_xstat64(int vers, const char *file, struct stat64 *buf)
{
  CHECK_INIT(-1);
  if (CommStack) {
    debug_printf(2, "%s(\"%s\") collective", __func__, file);
    int err,rank,xerr;
    err = MPI_Comm_rank(CommStack->comm, &rank); if (err) return -1;
    if (!rank) xerr = ((collfs_xstat64_fp) unwrap.xstat64)(vers, file, buf);
    err = MPI_Bcast(&xerr, 1, MPI_INT, 0, CommStack->comm);
    if (err < 0) {
      set_error(ECOLLFS, "MPI_Bcast failed to broadcast success of root xstat64");
      return -1;
    }

    err = MPI_Bcast(buf, sizeof *buf, MPI_BYTE, 0, CommStack->comm);
    if (err < 0) {
      set_error(ECOLLFS, "MPI_Bcast failed to broadcast struct");
      return -1;
    } else return 0;
  }
    return ((collfs_xstat64_fp) unwrap.xstat64)(vers, file, buf);
}

static int collfs_open(const char *pathname, int flags, mode_t mode)
{
  int err, rank;

  CHECK_INIT(-1);
  // pass through to libc __open if no communicator has been pushed
  if (!CommStack) {
    debug_printf(2, "%s(\"%s\", x%x, o%o) independent", __func__, pathname, flags, mode);
    return ((collfs_open_fp) unwrap.open)(pathname, flags, mode);
  }

  err = MPI_Comm_rank(CommStack->comm, &rank);
  if (err) {
    set_error(ECOLLFS, "Error determining rank. Bad communicator?");
    return -1;
  }

#ifndef O_CLOEXEC
  #define O_CLOEXEC O_RDONLY
#endif

  if (flags == O_RDONLY || flags == (O_RDONLY | O_CLOEXEC)) {      /* Read is collectively on comm */
    int len, fd, gotmem;
    long pagesize;
    struct {int len, errno_save;} buf;
    size_t totallen;
    void *mem;
    struct FileLink *link;

    debug_printf(2, "%s(\"%s\", x%x, o%o) collective", __func__, pathname, flags, mode);
    if (!rank) {
      len = -1;
      fd = ((collfs_open_fp) unwrap.open)(pathname, flags, mode);
      if (fd >= 0) {
        struct stat fdst;   
        if (fstat(fd, &fdst) < 0) ((collfs_close_fp) unwrap.close)(fd); /* fail cleanly */
        else len = (int)fdst.st_size;              /* Cast prevents using large files, but MPI would need workarounds too */
      }
    }
    buf.len = len;
    buf.errno_save = errno;
    err = MPI_Bcast(&buf, 2, MPI_INT, 0, CommStack->comm); if (err) return -1;
    len = buf.len;
    if (len < 0) {
      set_error(buf.errno_save, "Error opening file");
      return -1;
    }
    totallen = extend_to_page(len);

    mem = NULL;
    if (!rank) {
      mem = ((collfs_mmap_fp) unwrap.mmap)(0, totallen, PROT_READ, MAP_PRIVATE, fd, 0);
      ((collfs_lseek_fp) unwrap.lseek)(fd, 0, SEEK_SET);
    } else {
      /* Don't use shm_open() here because the shared memory segment is fixed at boot time. */
      fd = NextFD++;
      pagesize = sysconf(_SC_PAGESIZE);
      mem = ((collfs_mmap_fp) unwrap.mmap)(0, totallen, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);

      ((collfs_lseek_fp) unwrap.lseek)(fd, 0, SEEK_SET);
    }

    if (fd >= 0)
      /* Make sure everyone found memory */
      gotmem = !!mem;
    err = MPI_Allreduce(MPI_IN_PLACE, &gotmem, 1, MPI_INT, MPI_LAND, CommStack->comm);
    if (!gotmem) {
      if (!rank) {
        if (mem) ((collfs_munmap_fp) unwrap.munmap)(mem, totallen);
      } else {
        if (mem) ((collfs_munmap_fp) unwrap.munmap)(mem, totallen);
      }
      set_error(ECOLLFS, "Could not find memory: mmap() on rank 0, malloc otherwise");
      return -1;
    }

    err = MPI_Bcast(mem, len, MPI_BYTE, 0, CommStack->comm); if (err) return -1;

    if (rank) fd = NextFD++;            /* There is no way to make a proper file descriptor, this requires patching read() */

    link = malloc(sizeof *link);
    link->comm = CommStack->comm;
    link->fd = fd;
    link->refct = 1;
    strcpy(link->fname, pathname);
    link->mem = mem;
    link->len = len;
    link->totallen = totallen;
    link->offset = 0;

    /* Add new node to the list */
    link->next = DLOpenFiles;
    DLOpenFiles = link;

    return fd;
  }
  /* more than read access needed, fall back to independent access */
  debug_printf(2, "%s(\"%s\", x%x, o%o)", __func__, pathname, flags, mode);
  return ((collfs_open_fp) unwrap.open)(pathname, flags, mode);
}

/* Collective on the communicator used when the fd was created */
static int collfs_close(int fd)
{
  struct FileLink **linkp;
  int err;

  CHECK_INIT(-1);
  for (linkp=&DLOpenFiles; linkp && *linkp; linkp=&(*linkp)->next) {
    struct FileLink *link = *linkp;
    debug_printf(2, "%s(%d) on file link %p with fd %d", __func__, fd, linkp, link->fd);
    if (link->fd == fd) {       /* remove it from the list */
      int rank = 0, xerr = 0;

      debug_printf(2, "%s(%d) collective", __func__, fd);
      if (--link->refct > 0) {
        debug_printf(2, "%s(%d) nonzero refcount %d", __func__, fd, link->refct);
        return 0;
      }
      err = MPI_Comm_rank(CommStack ? CommStack->comm : MPI_COMM_WORLD, &rank); if (err) return -1;
      if (!rank) {
        ((collfs_munmap_fp) unwrap.munmap)(link->mem, link->totallen);
        xerr = ((collfs_close_fp) unwrap.close)(fd);
      } else {
        ((collfs_munmap_fp) unwrap.munmap)(link->mem, link->totallen);
      }
      *linkp = link->next;
      free(link);
      debug_printf(2, "%s(%d) entering Bcast", __func__, fd);
      err = MPI_Bcast(&xerr, 1, MPI_INT, 0, CommStack->comm);      
      debug_printf(2, "%s(%d) exiting Bcast", __func__, fd);
      return err;
    }
  }
  debug_printf(2, "%s(%d) independent", __func__, fd);
  return ((collfs_close_fp) unwrap.close)(fd);
}

/* Collective on the communicator used when the fd was created */
static ssize_t collfs_read(int fd, void *buf, size_t count)
{
  struct FileLink *link;

  CHECK_INIT(-1);
  for (link=DLOpenFiles; link; link=link->next) { /* Could optimize to not always walk the list */
    int rank = 0, err, initialized;
    err = MPI_Initialized(&initialized); if (err) return -1;
    if (initialized) {err = MPI_Comm_rank(link->comm, &rank); if (err) return -1;}
    if (fd == link->fd) {
      debug_printf(2, "%s(%d, %p, %zu) collective", __func__, fd, buf, count);
      if (!rank) return ((collfs_read_fp) unwrap.read)(fd, buf, count);
      else {
        if ((link->len - link->offset) < count) count = link->len - link->offset;
        memcpy(buf, link->mem+link->offset, count);
        link->offset += count;
        return count;
      }
    }
  }
  debug_printf(2, "%s(%d, %p, %zu) independent", __func__, fd, buf, count);
  return ((collfs_read_fp) unwrap.read)(fd, buf, count);
}

static off_t collfs_lseek(int fildes, off_t offset, int whence)
{
  struct FileLink *link;

  for (link=DLOpenFiles; link; link=link->next) {
    if (link->fd == fildes) {
      int rank = 0;
      MPI_Comm_rank(link->comm,&rank);
      debug_printf(2, "%s(%d, %lld, %d) collective", __func__, fildes, (long long)offset, whence);
      if (!rank) return ((collfs_lseek_fp) unwrap.lseek)(fildes, offset, whence); /* Rank 0 has a normal fd */
      switch (whence) {
      case SEEK_SET:
        link->offset = offset;
        break;
      case SEEK_CUR:
        link->offset += offset;
        break;
      case SEEK_END:
        link->offset = link->len + offset;
        break;
      }
      return link->offset;
    }
  }
  debug_printf(2, "%s(%d, %lld, %d) independent", __func__, fildes, (long long)offset, whence);
  return ((collfs_lseek_fp) unwrap.lseek)(fildes, offset, whence);
}

/* Collective on the communicator used when fildes was created 

Note that we allow len > mlink-> len.

*/
static void *collfs_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
  struct FileLink *link;
  long pagesize;
  int gotmem, rank, err;
  void *mem;

  if (off < 0) {
    debug_printf(2, "%s() ERROR: off < 0", __func__);
    set_error(ENXIO, "off < 0");
    return MAP_FAILED;
  }

  CHECK_INIT(MAP_FAILED);
  for (link=DLOpenFiles; link; link=link->next) {
    if (link->fd == fildes) {
      struct MMapLink *mlink;
      debug_printf(2, "%s(%p, %zu, %d, %d, %d, %lld) collective", __func__, addr, len, prot, flags, fildes, (long long)off);
      if (prot & PROT_WRITE && !(flags & MAP_FIXED)) {
        set_error(EACCES, "prot & PROT_WRITE && !(flags & MAP_FIXED)");
        return MAP_FAILED;
      }
      if (flags & MAP_FIXED) {
        if (off + len > link->totallen || off < 0) {
          debug_printf(2, "%p requested of length %zd, but off + len = %zd and link->totallen = %zd", 
                       addr, len, off+len, link->totallen);
          set_error(EACCES, "addr < (void*) (char*) link->mem+link->totallen");          
          return MAP_FAILED;
        }

        MPI_Comm_rank(CommStack->comm, &rank);
        if (!rank) {
          mem =  ((collfs_mmap_fp) unwrap.mmap)(addr, len, prot, flags, link->fd, off);
        } else {
          mem = ((collfs_mmap_fp) unwrap.mmap)(addr, len, PROT_READ | PROT_WRITE, flags | MAP_ANONYMOUS, link->fd, off);
        }

        gotmem = (mem != MAP_FAILED);
        debug_printf(2, "%s() memory: %p, requested addr: %p", __func__, mem, addr);
        MPI_Allreduce(MPI_IN_PLACE, &gotmem, 1, MPI_INT, MPI_LAND, CommStack->comm);
        debug_printf(2, "%s() Exiting Allreduce", __func__);
        if (!gotmem) {
          debug_printf(2, "%s() ERROR: Could not find memory to reallocate", __func__);
          set_error(ECOLLFS, "Could not find memory to reallocate");
          return MAP_FAILED;
        }

        if (rank) {
          debug_printf(2, "Moving %zd bytes from %p to %p [%p]",len,(void*)(char*)link->mem+off,addr,mem);
          memmove(addr,(void*)(char*)link->mem+off,len); 
          mprotect(mem, len, prot);          
        }
        mlink = malloc(sizeof *mlink);
        mlink->addr = mem;
        mlink->len = len;
        mlink->offset = off;
        mlink->fd = fildes;
        mlink->next = MMapRegions;
        MMapRegions = mlink;
        debug_printf(2, "%s() Returning mlink->addr at %p (fixed)", __func__, mlink->addr);
        return mlink->addr;
      }
      if (flags & MAP_SHARED ) {
        debug_printf(2, "%s() ERROR: Cannot do MAP_SHARED for a collective fd", __func__);
        set_error(ENOTSUP, "flags & MAP_SHARED: cannot do MAP_SHARED for a collective fd");
        return MAP_FAILED;
      }
      if (link->refct==1) { // no clients have mmaped this file, safe to create mmap
        size_t totallen;
        
        totallen = extend_to_page(len);
        debug_printf(2, "mmaping new size %zd (file size %zd) on offset %lld", totallen, link->totallen, (long long) off);
        MPI_Comm_rank(CommStack->comm, &rank);
        if (!rank) {
          ((collfs_munmap_fp) unwrap.munmap)(link->mem,link->totallen);
          mem = ((collfs_mmap_fp) unwrap.mmap)(addr, totallen, prot, flags, link->fd, off);
        } else {
          mem = ((collfs_mmap_fp) unwrap.mmap)(0, totallen, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, link->fd, 0);
        }
        
        gotmem = (mem != MAP_FAILED);
        debug_printf(2, "%s() memory: %p, requested addr: %p", __func__, mem, addr);
        MPI_Allreduce(MPI_IN_PLACE, &gotmem, 1, MPI_INT, MPI_LAND, CommStack->comm);
        if (!gotmem) {
          debug_printf(2, "%s() ERROR: Could not find memory to reallocate", __func__);
          set_error(ECOLLFS, "Could not find memory to reallocate");
          return MAP_FAILED;
        }

        if (rank) {
          memmove(mem, (void*)(char*)link->mem+off, link->totallen);
          mprotect(mem, totallen, prot);
          /* ((collfs_munmap_fp) unwrap.munmap)(link->mem,link->totallen); */
          debug_printf(2, "Allocated %zd bytes range [%p %p]", totallen, mem, (void*)(char*)mem+totallen);
          debug_printf(2, "Copied %zd bytes - range [%p %p]", link->totallen, mem, (void*)(char*)mem+link->totallen);
        }
      }
      else {
        debug_printf(2, "%s() ERROR: off + len > link->totallen", __func__);
        set_error(EOVERFLOW, "off + len > link->totallen");
        return MAP_FAILED;
      }
      mlink = malloc(sizeof *mlink);
      mlink->addr = mem;
      mlink->len = len;
      mlink->offset = off;
      mlink->fd = fildes;
      mlink->next = MMapRegions;
      mlink->link = link;
      MMapRegions = mlink;
      link->refct++;
      debug_printf(2, "%s() Returning mlink->addr at %p (extended)", __func__, mlink->addr);
      return mlink->addr;
    }
  }
  debug_printf(2, "%s(%p, %zu, %d, %d, %d, %lld) independent", __func__, addr, len, prot, flags, fildes, (long long)off);
  return ((collfs_mmap_fp) unwrap.mmap)(addr, len, prot, flags, fildes, off);
}

/* This implementation is not actually collective, but it relies on the fd being opened collectively */
static int collfs_munmap(__ptr_t addr, size_t len)
{
  if (CommStack) {
    debug_printf(2, "%s(%p, %zu) collective", __func__, addr, len);
    struct MMapLink *mlink;
    for (mlink=MMapRegions; mlink; mlink=mlink->next) {
      if (mlink->addr == addr) {
        int fd = mlink->fd;
        if (mlink->len != len) {
          set_error(EINVAL, "Attempt to unmap region of length %zu when %zu was mapped", len, mlink->len);
          return -1;
        }
        mlink->link->refct--;
        free(mlink);
        return ((collfs_munmap_fp) unwrap.munmap)(addr, len);
      }
    }
    set_error(EINVAL, "Address not mapped: %p", addr);
  }
  debug_printf(2, "%s(%p, %zu) independent", __func__, addr, len);
  return ((collfs_munmap_fp) unwrap.munmap)(addr, len);
}


/***********************************************************************
 * User-callable interface to collfs
 ***********************************************************************/

/* Symbol is exposed by patched ld.so */
extern struct libc_collfs_api _dl_collfs_api __attribute((weak));

int collfs_initialize(int level, void (*errhandler)(void))
{
  collfs_debug_level = level;
  collfs_error_handler = errhandler;
  if (collfs_initialized) {
    set_error(EALREADY, "collfs already initialized");
    return -1;
  }

  typedef void (*void_fp)(void);

  unwrap.fxstat64 = (void_fp) __fxstat64;
  unwrap.xstat64  = (void_fp) __xstat64;
  unwrap.open     = (void_fp) open;
  unwrap.close    = (void_fp) close;
  unwrap.read     = (void_fp) read;
  unwrap.lseek    = (void_fp) lseek;
  unwrap.mmap     = (void_fp) mmap;
  unwrap.munmap   = (void_fp) munmap;

  /* Make API visible to libc-rtld (ld.so) */

  _dl_collfs_api.fxstat64 = (void_fp) collfs_fxstat64;
  _dl_collfs_api.xstat64  = (void_fp) collfs_xstat64;
  _dl_collfs_api.open     = (void_fp) collfs_open;
  _dl_collfs_api.close    = (void_fp) collfs_close;
  _dl_collfs_api.read     = (void_fp) collfs_read;
  _dl_collfs_api.lseek    = (void_fp) collfs_lseek;
  _dl_collfs_api.mmap     = (void_fp) collfs_mmap;
  _dl_collfs_api.munmap   = (void_fp) collfs_munmap;

  collfs_initialized = 1;
  debug_printf(2, "collfs initialized");
  return 0;
}

int collfs_finalize()
{
  CHECK_INIT(-1);
  memset(&unwrap, 0, sizeof unwrap);
  memset(&_dl_collfs_api, 0, sizeof(_dl_collfs_api));
  collfs_initialized = 0;
  collfs_error_handler = 0;
  return 0;
}

int collfs_set_debug_level(int level)
{
  CHECK_INIT(-1);
  collfs_debug_level = level;
  return 0;
}

int collfs_set_error_handler(void (*fp)(void))
{
  CHECK_INIT(-1);
  collfs_error_handler = fp;
  return 0;
}

/* Logically collective, changes the communicator on which future IO is collective */
int collfs_comm_push(MPI_Comm comm)
{
  struct CommLink *link;

  CHECK_INIT(-1);
  link = malloc(sizeof *link);
  if (!link) {
    debug_printf(2, "%s failed -- unable to malloc link", __func__);
    return -1;
  }
  link->comm = comm;
  link->next = CommStack;
  CommStack = link;
#if DEBUG
  MPI_Barrier(link->comm);
#endif
  debug_printf(2, "%s comm %p", __func__, (void *) comm);
  return 0;
}

/* Logically collective, changes the communicator on which future IO is collective */
int collfs_comm_pop(void)
{
  struct CommLink *link = CommStack;
  CHECK_INIT(-1);
  if (!link) return -1;
  CommStack = link->next;
#if DEBUG
  MPI_Barrier(link->comm);
#endif
  free(link);
  return 0;
}
