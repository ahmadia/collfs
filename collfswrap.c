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

#if COLLFS_PRELOAD
#  define __wrap_open  open
#  define __wrap_close close
#  define __wrap_read  read
#endif

int __wrap_open(const char *pathname,int flags,...);
int __real_open(const char *pathname,int flags,int mode);
int __wrap_close(int fd);
int __real_close(int fd);
int __wrap_read(int fd,void *buf,size_t count);
int __real_read(int fd,void *buf,size_t count);

static int CollFSPathMatchComm(const char *path,MPI_Comm *comm,int *match)
{
  size_t len;
  len = strlen(path);
  if (!strcmp(path+len-3,".so")) {
    *comm = MPI_COMM_WORLD;
    *match = 1;
  } else {
    *match = 0;
  }
  return 0;
}

int __wrap_open(const char *pathname,int flags,...)
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
  err = CollFSPathMatchComm(pathname,&comm,&match); if (err) return -1;
  err = MPI_Initialized(&initialized); if (err) return -1;
  if (initialized) {err = MPI_Comm_rank(MPI_COMM_WORLD,&rank); if (err) return -1;}
#if DEBUG
  fprintf(stderr,"[%d] open(\"%s\",%d,%d)\n",rank,pathname,flags,mode);
#endif

  if (initialized && match && (flags == O_RDONLY)) { /* Read is collectively on comm */
    int len,fd,gotmem;
    void *mem;
    struct FileLink *link;
    if (!rank) {
      len = -1;
      fd = __real_open(pathname,flags,mode);
      if (fd >= 0) {
        struct stat fdst;
        if (fstat(fd,&fdst) < 0) __real_close(fd); /* fail cleanly */
        else len = (int)fdst.st_size;              /* Cast prevents using large files, but MPI would need workarounds too */
      }
    }
    err = MPI_Bcast(&len,1,MPI_INT,0,comm); if (err) return -1;
    if (len < 0) return -1;
    mem = NULL;
    if (!rank) mem = mmap(0,len,PROT_READ,MAP_PRIVATE,fd,0);
    else {
      /* Don't use shm_open() here because the shared memory segment is fixed at boot time. */
      fd = NextFD++;
      mem = malloc(len);
    }
#if DEBUG
    if (fd < 0) fprintf(stderr,"could not shm_open because of \n");
#endif
    if (fd >= 0) 

    /* Make sure everyone found memory */
    gotmem = !!mem;
    err = MPI_Allreduce(MPI_IN_PLACE,&gotmem,1,MPI_INT,MPI_LAND,comm);
    if (!gotmem) {
      if (!rank) {
        if (mem) munmap(mem,len);
      } else free(mem);
      return -1;
    }

    err = MPI_Bcast(mem,len,MPI_BYTE,0,comm); if (err) return -1;

    if (rank) fd = NextFD++;            /* There is no way to make a proper file descriptor, this requires patching read() */

    link = malloc(sizeof *link);
    link->comm = comm;
    link->fd = fd;
    strcpy(link->fname,pathname);
    link->mem = mem;
    link->len = len;
    link->offset = 0;

    /* Add new node to the list */
    link->next = DLOpenFiles;
    DLOpenFiles = link;

    return fd;
  }
  return __real_open(pathname, flags, mode);
}

int __wrap_close(int fd)
{
  struct FileLink **linkp;
  int err,initialized;

  err = MPI_Initialized(&initialized); if (err) return -1;
#if DEBUG
  {
    int rank = 0;
    if (initialized) {err = MPI_Comm_rank(MPI_COMM_WORLD,&rank); if (err) return -1;}
    fprintf(stderr,"[%d] close(%d)\n",rank,fd);
  }
#endif

  for (linkp=&DLOpenFiles; linkp && *linkp; linkp=&(*linkp)->next) {
    struct FileLink *link = *linkp;
    if (link->fd == fd) {       /* remove it from the list */
      int rank = 0;
      if (initialized) {MPI_Comm_rank(MPI_COMM_WORLD,&rank);}
      if (!rank) {
        munmap(link->mem,link->len);
        __real_close(fd);
      } else {
        free(link->mem);
      }
      *linkp = link->next;
      free(link);
      return 0;
    }
  }
  __real_close(fd);
  return 0;
}

int __wrap_read(int fd,void *buf,size_t count)
{
  struct FileLink *link;
  for (link=DLOpenFiles; link; link=link->next) { /* Could optimize to not always walk the list */
    int rank = 0,err,initialized;
    err = MPI_Initialized(&initialized); if (err) return -1;
    if (initialized) {err = MPI_Comm_rank(link->comm,&rank); if (err) return -1;}
    if (fd == link->fd) {
      if (!rank) return __real_read(fd,buf,count);
      else {
        if ((link->len - link->offset) < count) count = link->len - link->offset;
        memcpy(buf,link->mem+link->offset,count);
        link->offset += count;
        return count;
      }
    }
  }
  return __real_read(fd,buf,count);
}

#if COLLFS_PRELOAD
int __real_open(const char *path,int flags,int mode)
{
  int (*f)(const char*,int,int);
  f = dlsym(RTLD_NEXT,"open");
  if (!f) return -1;
  return (*f)(path,flags,mode);
}
int __real_close(int fd)
{
  int (*f)(int);
  f = dlsym(RTLD_NEXT,"close");
  if (!f) return -1;
  return (*f)(fd);
}
int __real_read(int fd,void *buf,size_t count)
{
  int (*f)(int,void*,size_t);
  f = dlsym(RTLD_NEXT,"read");
  if (!f) return -1;
  return (*f)(fd,buf,count);
}
#endif
