#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mpi.h>
#include "errmacros.h"
#include "foo.h"
#include "collfs.h"

extern int MPI_Comm_rank ( MPI_Comm comm, int *rank ) __attribute__ ((weak));

int foo(const char *path)
{
  int fd,err,rank;
  unsigned int sum,value;
  struct stat64 st;

  if (MPI_Comm_rank) { 
    err = MPI_Comm_rank(MPI_COMM_WORLD,&rank);CHK(err); 
  } 
  else rank = -1;
    
  fd = __collfs_open(path,O_RDONLY);
  if (fd < 0) ERR("[%d] open(\"%s\",O_RDONLY) failed",rank,path);
  sum = 0;

  err = __collfs_lseek(fd,0,SEEK_SET); CHK(err);

  long pagesize;
  char* pa;
  pagesize = sysconf(_SC_PAGESIZE);
  pa = __collfs_mmap((void *)0,pagesize,PROT_READ,MAP_PRIVATE,fd,(off_t)0);
  if (pa == MAP_FAILED) ERR("[%d] mmap(%d,pagesize) failed",rank,fd);

  while (__collfs_read(fd,&value,sizeof value) == sizeof value) {
    sum ^= value;
  }

  err = __collfs_munmap(pa,pagesize); CHK(err);

  err = __collfs_close(fd); CHK(err);
  printf("[%d] XOR-sum of \"%s\": %x\n",rank,path,sum);

  err = __collfs_xstat64(_STAT_VER, path, &st);CHK(err);
  printf("[%d] %s (via __collfs_xstat64): inode %ld size %ld\n", rank, path, (long)st.st_ino, (long)st.st_size);
  return 0;
}

int foo2_inner(const char *path)
{
  int err, fd, off, nc;
  char buf[4], *ptr, *ptr2;
  fd = __collfs_open(path, O_RDONLY);
  if (!fd) return -1;

  off = __collfs_lseek(fd, 0, SEEK_SET);
  if (off != 0) ERR("lseek SET");

  nc = __collfs_read(fd, buf, 2);
  if (nc != 2) ERR("read [%d] but expected 2\nbuf: %4.4s", nc, buf);
  if (strncmp(buf, "ab", 2)) ERR("wrong content");

  if (__collfs_read(fd, buf, 3) != 3) ERR("read");
  if (strncmp(buf, "cde", 3)) ERR("wrong repeat read");

  off = __collfs_lseek(fd, 8, SEEK_CUR);
  if (off != 13) ERR("lseek CUR");
  if (__collfs_read(fd, buf, 3) != 3) ERR("read");
  if (strncmp(buf, "nop", 3)) ERR("wrong content");

  off = __collfs_lseek(fd, 5, SEEK_SET);
  if (off != 5) ERR("lseek SET");
  if (__collfs_read(fd, buf, 3) != 3) ERR("read");
  if (strncmp(buf, "fgh", 3)) ERR("wrong content");

  err = __collfs_comm_pop();CHK(err);

  ptr = __collfs_mmap(0, 12, PROT_READ, MAP_PRIVATE, fd, 0);
  if (ptr == MAP_FAILED) ERR("mmap failed: %s", strerror(errno));
  if (strncmp(ptr+6, "ghijkl", 6)) ERR("wrong content");

  ptr2 = __collfs_mmap(0, 20, PROT_READ, MAP_PRIVATE, fd, 0);
  if (ptr2 == MAP_FAILED) ERR("mmap failed");
  if (strncmp(ptr2+7, "hij", 3)) ERR("wrong content");

  err = __collfs_close(fd); if (err) ERR("close");

  err = __collfs_comm_push(MPI_COMM_WORLD);CHK(err);
  err = __collfs_munmap(ptr2, 20); if (err) ERR("munmap");

  if (strncmp(ptr+6, "ghijkl", 6)) ERR("wrong content");

  err = __collfs_munmap(ptr, 12); if (err) ERR("munmap");
  return 0;
}

int foo2(const char *path)
{
  int err;
  err = __collfs_comm_push(MPI_COMM_WORLD);CHK(err);
  foo2_inner(path);
  err = __collfs_comm_pop();CHK(err);
  return 0;
}
