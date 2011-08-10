#if COLLFS_IN_LIBC
#else
#define _GNU_SOURCE 1            /* feature test macro so that RTLD_NEXT will be available */
#endif

// start dl-load

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ldsodefs.h>
#include <bits/wordsize.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stackinfo.h>
#include <caller.h>
#include <sysdep.h>


// from dl-load

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mpi.h>

#if DEBUG
#  include <stdio.h>
#endif

#define ECOLLFS EREMOTEIO       /* To set errno when MPI fails */
static void set_errno(int e) { errno = e; }

#if COLLFS_IN_LIBC
#define __read __libc_read  
#define mmap __mmap
#define munmap __munmap
#define stderr_printf(...) _dl_error_printf(__VA_ARGS__)
#else 
#define stderr_printf(...) fprintf(stderr, __VA_ARGS__)
#endif

struct FileLink {
  MPI_Comm comm;
  int fd;
  char fname[MAXPATHLEN];
  void *mem;
  size_t len;
  size_t offset;
  struct FileLink *next;
};
static struct FileLink *DLOpenFiles;
static const int BaseFD = 10000;
static int NextFD = 10001;

struct CommLink {
  MPI_Comm comm;
  struct CommLink *next;
};
static struct CommLink *CommStack;

/* Not collective, but changes the communicator on which future IO is collective */
int __collfs_comm_push(MPI_Comm comm)
{
  struct CommLink *link;
  link = malloc(sizeof *link);
  if (!link) return -1;
  link->comm = comm;
  link->next = CommStack;
  CommStack = link;
  return 0;
}
/* Not collective, but changes the communicator on which future IO is collective */
int __collfs_comm_pop(void)
{
  struct CommLink *link = CommStack;
  if (!link) return -1;
  CommStack = link->next;
  free(link);
  return 0;
}

extern int __fxstat64(int vers, int fd, struct stat64 *buf);
extern int __xstat64 (int vers, const char *file, struct stat64 *buf);
extern int __open(const char *pathname, int flags, int mode);
extern int __close(int fd);

extern void* __mmap(void *addr, size_t len, int prot, int flags, 
                    int fildes, off_t off);

extern int __munmap (__ptr_t addr, size_t len);

extern off_t __lseek(int fildes, off_t offset, int whence);

extern ssize_t __read(int fd,void *buf,size_t count);

// MPI stubs - these function references will be equal to 0 
// if the linker has not brought in MPI yet
extern int MPI_Initialized(int *flag) __attribute__ ((weak));
extern int MPI_Comm_rank (MPI_Comm comm, int *rank) __attribute__ ((weak));
extern int MPI_Bcast (void *buffer, int count, MPI_Datatype datatype, int root,
                      MPI_Comm comm)  __attribute__ ((weak));
extern int MPI_Allreduce (void *sendbuf, void *recvbuf, int count,
                          MPI_Datatype datatype, MPI_Op op,
                          MPI_Comm comm ) __attribute__ ((weak));

/* Logically collective on the communicator used when the fd was created */
int __collfs_fxstat64(int vers, int fd, struct stat64 *buf)
{
  int err;
  struct FileLink *link;
  for (link=DLOpenFiles; link; link=link->next) {
    if (link->fd == fd) {
      int rank,xerr = 0;
      err = MPI_Comm_rank(link->comm, &rank); if (err) return -1;
      if (!rank) xerr = __fxstat64(vers, fd, buf);
#if DEBUG
      err = MPI_Bcast(&xerr, 1, MPI_INT, 0, link->comm);
      if (err < 0) {
        set_errno(ECOLLFS);
        return -1;
      }
#endif
      if (xerr < 0) return -1; /* Only rank 0 will have errno set correctly */

      err = MPI_Bcast(buf, sizeof *buf, MPI_BYTE, 0, link->comm);
      if (err < 0) {
        set_errno(ECOLLFS);
        return -1;
      } else return 0;
    }
  }
  return __fxstat64(vers, fd, buf);
}

/* Collective on the communicator at the top of the collfs stack */
int __collfs_xstat64(int vers, const char *file, struct stat64 *buf)
{
  if (CommStack) {
    int err,rank,xerr;
    err = MPI_Comm_rank(CommStack->comm, &rank); if (err) return -1;
    if (!rank) xerr = __xstat64(vers, file, buf);
#if DEBUG
    err = MPI_Bcast(&xerr, 1, MPI_INT, 0, CommStack->comm);
    if (err < 0) {
      set_errno(ECOLLFS);
      return -1;
    }
#endif
    if (xerr < 0) return -1; /* Only rank 0 will have errno set correctly */

    err = MPI_Bcast(buf, sizeof *buf, MPI_BYTE, 0, CommStack->comm);
    if (err < 0) {
      set_errno(ECOLLFS);
      return -1;
    } else return 0;
  } else return __xstat64(vers, file, buf);
}

/* Collective on the communicator used when fildes was created */
void *__collfs_mmap(void *addr, size_t len, int prot, int flags, 
                    int fildes, off_t off)
{
  int rank = 0;
  if (MPI_Initialized) {
#if DEBUG
    stderr_printf("[%x] mmap(fd:%x @%x,%x,%x,%x,%x)\n",rank,fildes,(int)addr,(int)len,prot,flags,(int)off);
#endif      
    stderr_printf("__collfs_mmap has not been implemented yet! (passing through)\n");
    return mmap(addr, len, prot, flags, fildes, off);
  }
  else {
#if DEBUG
    stderr_printf("[NO_MPI] mmap(fd:%x @%x,%x,%x,%x,%x)\n",fildes,(int)addr,(int)len,prot,flags,(int)off);
#endif  
    return mmap(addr, len, prot, flags, fildes, off);
  }
}

/* Not collective */
int __collfs_munmap (__ptr_t addr, size_t len) 
{
 int rank = 0;
  if (MPI_Initialized) {
#if DEBUG
    stderr_printf("[%x] munmap(@%x,%x)\n",rank,(int) addr,(int) len);
#endif      
    stderr_printf("__collfs_munmap has not been implemented yet! (passing through)\n");
    return munmap(addr, len);
  }
  else {
#if DEBUG
    stderr_printf("[NO_MPI] munmap(@%x,%x)\n",(int) addr, (int) len);
#endif  
    return munmap(addr, len);
  }
}


off_t __collfs_lseek(int fildes, off_t offset, int whence)
{
  int rank = 0;
  if (MPI_Initialized) {
#if DEBUG
    stderr_printf("[%x] lseek(fd:%x,%x,%x)\n",rank,fildes,(int)offset,whence);
#endif      
    stderr_printf("__collfs_lseek has not been implemented yet! (passing through)\n");
    return __lseek(fildes, offset, whence);
  }
  else {
#if DEBUG
    stderr_printf("[NO_MPI] lseek(fd:%x,%x,%x)\n",fildes,(int)offset,whence);
#endif  
    return __lseek(fildes, offset, whence);
  }
}


int __collfs_open(const char *pathname, int flags,...)
{
  mode_t mode = 0;
  int err,rank = 0,initialized;

  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
  }

  // pass through to libc __open if MPI has not been loaded yet or if no communicator has been pushed
  if (!CommStack) {
#if DEBUG
    stderr_printf("[NO_MPI] open(\"%s\",%x,%x)\n",pathname,flags,mode);
#endif
    return __open(pathname, flags, mode);
  }

  if (!MPI_Initialized) {
#if DEBUG
    stderr_printf("Stack not empty, but no MPI symbols\n");
#endif
    set_errno(ECOLLFS);
    return -1;
  }
    err = MPI_Initialized(&initialized); if (err) return -1;
  if (!initialized) {
#if DEBUG
    stderr_printf("Stack not empty, but MPI is not initialized. Perhaps it was finalized early?\n");
#endif
    set_errno(ECOLLFS);
    return -1;
  }

#if DEBUG
  fprintf(stderr, "[%d] open(\"%s\",%d,%d)\n", rank, pathname, flags, mode);
#endif

  if (flags == O_RDONLY) {      /* Read is collectively on comm */
    int len, fd, gotmem;
    void *mem;
    struct FileLink *link;
    if (!rank) {
      len = -1;
      fd = __open(pathname, flags, mode);
      if (fd >= 0) {
        struct stat fdst;
        if (fstat(fd, &fdst) < 0) __close(fd); /* fail cleanly */
        else len = (int)fdst.st_size;              /* Cast prevents using large files, but MPI would need workarounds too */
      }
    }
    err = MPI_Bcast(&len, 1,MPI_INT, 0, CommStack->comm); if (err) return -1;
    if (len < 0) return -1;
    mem = NULL;
    if (!rank) mem = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
    else {
      /* Don't use shm_open() here because the shared memory segment is fixed at boot time. */
      fd = NextFD++;
      mem = malloc(len);
    }
#if DEBUG
    if (fd < 0) stderr_printf("could not shm_open because of \n");
#endif
    if (fd >= 0) 

      /* Make sure everyone found memory */
      gotmem = !!mem;
    err = MPI_Allreduce(MPI_IN_PLACE, &gotmem, 1, MPI_INT, MPI_LAND, CommStack->comm);
    if (!gotmem) {
      if (!rank) {
        if (mem) munmap(mem, len);
      } else free(mem);
      return -1;
    }

    err = MPI_Bcast(mem, len, MPI_BYTE, 0, CommStack->comm); if (err) return -1;

    if (rank) fd = NextFD++;            /* There is no way to make a proper file descriptor, this requires patching read() */

    link = malloc(sizeof *link);
    link->comm = CommStack->comm;
    link->fd = fd;
    strcpy(link->fname, pathname);
    link->mem = mem;
    link->len = len;
    link->offset = 0;

    /* Add new node to the list */
    link->next = DLOpenFiles;
    DLOpenFiles = link;

    return fd;
  }
  /* more than read access needed, fall back to independent access */
  return __open(pathname, flags, mode);
}

/* Collective on the communicator used when the fd was created */
int __collfs_close(int fd)
{
  struct FileLink **linkp;
  int err,initialized;
  int rank = 0;

  // pass through to libc __close if MPI has not been loaded yet
  if (MPI_Initialized) {
    err = MPI_Initialized(&initialized); if (err) return -1;
#if DEBUG
    if (initialized) {err = MPI_Comm_rank(MPI_COMM_WORLD,&rank); if (err) return -1;}
    stderr_printf("[%x] close(fd:%x)\n",rank,fd);
#endif
  }
  else {
#if DEBUG
    stderr_printf("[NO_MPI] close(fd:%x)\n",fd);
#endif
    return __close(fd);
  }

  for (linkp=&DLOpenFiles; linkp && *linkp; linkp=&(*linkp)->next) {
    struct FileLink *link = *linkp;
    if (link->fd == fd) {       /* remove it from the list */
      int rank = 0, xerr = 0;

      if (!initialized) {
#if DEBUG
        stderr_printf("Attempt to close open collective fd, but MPI is not initialized. Perhaps it was finalized early?\n");
#endif
        set_errno(ECOLLFS);
        return -1;
      }
      err = MPI_Comm_rank(CommStack->comm, &rank); if (err) return -1;
      if (!rank) {
        munmap(link->mem, link->len);
        xerr = __close(fd);
      } else {
        free(link->mem);
      }
      *linkp = link->next;
      free(link);
      return xerr;
    }
  }
  return __close(fd);
}

/* Collective on the communicator used when the fd was created */
ssize_t __collfs_read(int fd, void *buf, size_t count)
{
  struct FileLink *link;

  if (!MPI_Initialized) return __read(fd, buf, count);

  for (link=DLOpenFiles; link; link=link->next) { /* Could optimize to not always walk the list */
    int rank = 0, err, initialized;
    err = MPI_Initialized(&initialized); if (err) return -1;
    if (initialized) {err = MPI_Comm_rank(link->comm, &rank); if (err) return -1;}
    if (fd == link->fd) {
      if (!rank) return __read(fd, buf, count);
      else {
        if ((link->len - link->offset) < count) count = link->len - link->offset;
        memcpy(buf, link->mem+link->offset, count);
        link->offset += count;
        return count;
      }
    }
  }
  return __read(fd, buf, count);
}

