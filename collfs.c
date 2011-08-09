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

int __collfs_fxstat64(int vers, int fd, struct stat64 *buf);
extern int __fxstat64(int vers, int fd, struct stat64 *buf);

int __collfs_open(const char *pathname,int flags,...);
extern int __open(const char *pathname,int flags,...);

int __collfs_close(int fd);
extern int __close(int fd);

void* __collfs_mmap(void *addr, size_t len, int prot, int flags, 
                    int fildes, off_t off);
extern void* __mmap(void *addr, size_t len, int prot, int flags, 
                    int fildes, off_t off);

int __collfs_munmap (__ptr_t addr, size_t len);
extern int __munmap (__ptr_t addr, size_t len);

off_t __collfs_lseek(int fildes, off_t offset, int whence);
extern off_t __lseek(int fildes, off_t offset, int whence);

ssize_t __collfs_read(int fd,void *buf,size_t count);
extern ssize_t __read(int fd,void *buf,size_t count);

// MPI stubs - these function references will be equal to 0 
// if the linker has not brought in MPI yet
extern int MPI_Initialized( int *flag ) __attribute__ ((weak));
extern int MPI_Comm_rank ( MPI_Comm comm, int *rank ) __attribute__ ((weak));
extern int MPI_Bcast ( void *buffer, int count, MPI_Datatype datatype, int root, 
                       MPI_Comm comm )  __attribute__ ((weak));
extern int MPI_Allreduce ( void *sendbuf, void *recvbuf, int count, 
                           MPI_Datatype datatype, MPI_Op op, 
                           MPI_Comm comm ) __attribute__ ((weak));

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

int __collfs_fxstat64(int vers, int fd, struct stat64 *buf)
{
  int rank = 0;
  if (MPI_Initialized) {
#if DEBUG
    stderr_printf("[%x] fxstat64(\"%x\",%x)\n",rank,vers,fd);
#endif      
    stderr_printf("__collfs_fxstat64 has not been implemented yet! (passing through)\n");
    return __fxstat64(vers, fd, buf);
  }
  else {
#if DEBUG
    stderr_printf("[NO_MPI] fxstat64(\"%x\",%x)\n",vers,fd);
#endif  
    return __fxstat64(vers, fd, buf);
  }
}

void* __collfs_mmap(void *addr, size_t len, int prot, int flags, 
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


int __collfs_open(const char *pathname,int flags,...)
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
  if (MPI_Initialized) {
    err = CollFSPathMatchComm(pathname,&comm,&match); if (err) return -1;
    err = MPI_Initialized(&initialized); if (err) return -1;
    if (initialized) {err = MPI_Comm_rank(MPI_COMM_WORLD,&rank); if (err) return -1;}
#if DEBUG
    stderr_printf("[%x] open(\"%s\",%x,%x)\n",rank,pathname,flags,mode);
#endif
  }
  else {
#if DEBUG
    stderr_printf("[NO_MPI] open(\"%s\",%x,%x)\n",pathname,flags,mode);
#endif
    return __open(pathname, flags, mode);
  }

  if (initialized && match && (flags == O_RDONLY)) { /* Read is collectively on comm */
    int len,fd,gotmem;
    void *mem;
    struct FileLink *link;
    if (!rank) {
      len = -1;
      fd = __open(pathname,flags,mode);
      if (fd >= 0) {
        struct stat fdst;
        if (fstat(fd,&fdst) < 0) __close(fd); /* fail cleanly */
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
    if (fd < 0) stderr_printf("could not shm_open because of \n");
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
  return __open(pathname, flags, mode);
}

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
      int rank = 0;
      if (initialized) {MPI_Comm_rank(MPI_COMM_WORLD,&rank);}
      if (!rank) {
        munmap(link->mem,link->len);
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

ssize_t __collfs_read(int fd,void *buf,size_t count)
{
  struct FileLink *link;

  if (!MPI_Initialized) return __read(fd, buf, count);

  for (link=DLOpenFiles; link; link=link->next) { /* Could optimize to not always walk the list */
    int rank = 0,err,initialized;
    err = MPI_Initialized(&initialized); if (err) return -1;
    if (initialized) {err = MPI_Comm_rank(link->comm,&rank); if (err) return -1;}
    if (fd == link->fd) {
      if (!rank) return __read(fd,buf,count);
      else {
        if ((link->len - link->offset) < count) count = link->len - link->offset;
        memcpy(buf,link->mem+link->offset,count);
        link->offset += count;
        return count;
      }
    }
  }
  return __read(fd,buf,count);
}

