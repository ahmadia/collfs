#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mpi.h>
#include "errmacros.h"
#include "foo.h"

int foo(const char *path)
{
  int fd,err,rank;
  unsigned int sum,value;

  err = MPI_Comm_rank(MPI_COMM_WORLD,&rank);CHK(err);
  fd = open(path,O_RDONLY);
  if (fd < 0) ERR("[%d] open(\"%s\",O_RDONLY) failed",rank,path);
  sum = 0;
  while (read(fd,&value,sizeof value) == sizeof value) {
    sum ^= value;
  }
  close(fd);
  printf("[%d] XOR-sum of \"%s\": %x\n",rank,path,sum);
  return 0;
}
