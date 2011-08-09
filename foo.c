#include <fcntl.h>
#include <stdio.h>
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
  pa = __collfs_mmap((void *)0,pagesize,PROT_READ,MAP_SHARED,fd,(off_t)0);
  if (pa == MAP_FAILED) ERR("[%d] mmap(%d,pagesize) failed",rank,fd);

  while (__collfs_read(fd,&value,sizeof value) == sizeof value) {
    sum ^= value;
  }

  err = __collfs_munmap(pa,pagesize); CHK(err);
  
  err = __collfs_close(fd); CHK(err);
  printf("[%d] XOR-sum of \"%s\": %x\n",rank,path,sum);
  return 0;
}
