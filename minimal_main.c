#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

struct libc_collfs_api {
  void * fxstat64;
  void *  xstat64;
  void *     open;
  void *    close;
  void *     read;
  void *    lseek;
  void *     mmap;
  void *   munmap;
};

extern struct libc_collfs_api _dl_collfs_api;

typedef FILE* (*open_fp)(const char*, const char*);

int main(int argc, char *argv[])
{
  open_fp fopen_fp;

  MPI_Init(&argc,&argv);
  
  _dl_collfs_api.open = (void *) fopen;
  fopen_fp = (open_fp) _dl_collfs_api.open;
  
  FILE *fp = fopen_fp("test_open.txt","w");
  fclose(fp);
  
  MPI_Finalize();
  return 0;
}
