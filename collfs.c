#define _GNU_SOURCE             /* feature test macro so that RTLD_NEXT will be available */
#include <dlfcn.h>
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




extern int __fxstat64(int vers, int fd, struct stat64 *buf);
extern int __xstat64 (int vers, const char *file, struct stat64 *buf);
extern int __open(const char *pathname, int flags, int mode);
extern int __close(int fd);

#if COLLFS_IN_LIBC
#define __read __libc_read  
#endif

extern int __read(int fd, void *buf, size_t count);

// MPI stubs - these function references will be equal to 0 
// if the linker has not brought in MPI yet
extern int MPI_Initialized(int *flag) __attribute__ ((weak));
extern int MPI_Comm_rank (MPI_Comm comm, int *rank) __attribute__ ((weak));
extern int MPI_Bcast (void *buffer, int count, MPI_Datatype datatype, int root,
                      MPI_Comm comm)  __attribute__ ((weak));
extern int MPI_Allreduce (void *sendbuf, void *recvbuf, int count,
                          MPI_Datatype datatype, MPI_Op op,
                          MPI_Comm comm ) __attribute__ ((weak));

static int CollFSPathMatchComm(const char *path, MPI_Comm *comm, int *match)
{
  size_t len;
  len = strlen(path);
  if (!strcmp(path+len-3, ".so")) {
    *comm = MPI_COMM_WORLD;
    *match = 1;
  } else {

    *match = 0;
  }
  return 0;
}

int __collfs_open(const char *pathname, int flags,...)
{
  mode_t mode = 0;
  int err,match,rank = 0,initialized;
  MPI_Comm comm;

  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
  }

  // pass through to libc __open if MPI has not been loaded yet
  if (!MPI_Initialized) return __open(pathname, flags, mode);

  err = CollFSPathMatchComm(pathname, &comm, &match); if (err) return -1;
  err = MPI_Initialized(&initialized); if (err) return -1;
  if (initialized) {err = MPI_Comm_rank(MPI_COMM_WORLD, &rank); if (err) return -1;}
#if DEBUG
  fprintf(stderr, "[%d] open(\"%s\",%d,%d)\n", rank, pathname, flags, mode);
#endif

  if (initialized && match && (flags == O_RDONLY)) { /* Read is collectively on comm */
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
    err = MPI_Bcast(&len, 1,MPI_INT, 0, comm); if (err) return -1;
    if (len < 0) return -1;
    mem = NULL;
    if (!rank) mem = mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
    else {
      /* Don't use shm_open() here because the shared memory segment is fixed at boot time. */
      fd = NextFD++;
      mem = malloc(len);
    }
#if DEBUG
    if (fd < 0) fprintf(stderr, "could not shm_open because of \n");
#endif
    if (fd >= 0) 

    /* Make sure everyone found memory */
    gotmem = !!mem;
    err = MPI_Allreduce(MPI_IN_PLACE, &gotmem, 1, MPI_INT, MPI_LAND, comm);
    if (!gotmem) {
      if (!rank) {
        if (mem) munmap(mem, len);
      } else free(mem);
      return -1;
    }

    err = MPI_Bcast(mem, len, MPI_BYTE, 0, comm); if (err) return -1;

    if (rank) fd = NextFD++;            /* There is no way to make a proper file descriptor, this requires patching read() */

    link = malloc(sizeof *link);
    link->comm = comm;
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
  return __open(pathname, flags, mode);
}

int __collfs_close(int fd)
{
  struct FileLink **linkp;
  int err,initialized;

  // pass through to libc __close if MPI has not been loaded yet
  if (!MPI_Initialized) return __close(fd);

  err = MPI_Initialized(&initialized); if (err) return -1;
#if DEBUG
  {
    int rank = 0;
    if (initialized) {err = MPI_Comm_rank(MPI_COMM_WORLD, &rank); if (err) return -1;}
    fprintf(stderr, "[%d] close(%d)\n", rank, fd);
  }
#endif

  for (linkp=&DLOpenFiles; linkp && *linkp; linkp=&(*linkp)->next) {
    struct FileLink *link = *linkp;
    if (link->fd == fd) {       /* remove it from the list */
      int rank = 0;
      if (initialized) {MPI_Comm_rank(MPI_COMM_WORLD, &rank);}
      if (!rank) {
        munmap(link->mem, link->len);
        return __close(fd);
      } else {
        free(link->mem);
      }
      *linkp = link->next;
      free(link);
      return 0;
    }
  }
  return __close(fd);
}

int __collfs_read(int fd, void *buf, size_t count)
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

