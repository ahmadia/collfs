#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <mpi.h>
#include <dlfcn.h>

#include "collfs.h"
#include "collfs_fopen.h"
#include "errmacros.h"

int run_tests(int verbosity,const char *soname)
{
  void *handle;
  int err, rank, count;
  int (*func)(int);
  FILE* alphabet;
  char abcde[6];

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (verbosity > 0) printf("[%d] & attempting to open %s\n", rank, soname);
  handle = dlopen(soname,RTLD_GLOBAL | RTLD_LAZY);
  if (!handle) ERR("failed to dlopen(\"%s\",RTLD_GLOBAL | RTLD_LAZY) due to: %s", soname, dlerror());

  if (verbosity > 0) printf("[%d] & looking for thefunc in handle %p\n", rank, handle);
  func = dlsym(handle,"thefunc");
  if (!func) ERR("dlsym could not find symbol \"thefunc\" due to: %s", dlerror());

  if (verbosity > 0) printf("[%d] & found thefunc at address %p\n", rank, func);
  err = (*func)(verbosity);CHK(err);

  err = dlclose(handle);
  if (err) ERR("dlclose failed due to: %s", dlerror());

  // // fcollfs_open
  // if (verbosity > 0) printf("[%d] attempting to collfs_fopen alphabet.txt\n", rank);
  // alphabet = fcollfs_open("./alphabet.txt","r");
  
  // // fread
  // if (verbosity > 0) printf("[%d] checking fread\n", rank);
  // count = fread(abcde, sizeof(char), 5, alphabet);
  // if (count < 5) {
  //   ERR("expected to read 5 bytes, read: %d", count);    
  // }
  // abcde[5] = '\0';
  // if (strcmp(abcde, "abcde") != 0) {
  //   ERR("expected to read 'abcde', read: %s", abcde);
  // }

  // // fseek
  // if (verbosity > 0) printf("[%d] checking fseek\n", rank);
  // err = fseek(alphabet, -4, SEEK_CUR);
  // if (err==-1) perror("error in calling fseek:");
  // count = fread(abcde, sizeof(char), 5, alphabet);
  // if (count < 5) {
  //   ERR("expected to read 5 bytes, read: %d", count);    
  // }
  // abcde[5] = '\0';
  // if (strcmp(abcde, "bcdef") != 0) {
  //   ERR("expected to read 'bcdef', read: %s", abcde);
  // }

  // // fclose
  // if (verbosity > 0) printf("[%d] checking fclose\n", rank);
  // err = fclose(alphabet);
  // if (err==-1) perror("error in calling fclose:");
  return 0;
}

int main(int argc, char *argv[])
{
  int err,verbosity,rank,size;
  double tstart,tend,elapsed;
  char path[MAXPATHLEN];

  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  if (argc == 2) {
    verbosity = atoi(argv[1]);
  }
  collfs_initialize(verbosity, NULL);

  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libminimal_thefunc.so");

  tstart = MPI_Wtime();
  collfs_comm_push(MPI_COMM_WORLD);
  err = run_tests(verbosity,path);CHK(err);
  collfs_comm_pop();
  tend = MPI_Wtime();
  elapsed = tend - tstart;
  if (verbosity > 1) printf("[%d] elapsed = %g\n",rank,elapsed);
  {
    struct {double time; int rank; } loc,gmax,gmin;
    double gsum;
    loc.time = elapsed;
    loc.rank = rank;
    MPI_Reduce(&loc,&gmax,1,MPI_DOUBLE_INT,MPI_MAXLOC,0,MPI_COMM_WORLD);
    MPI_Reduce(&loc,&gmin,1,MPI_DOUBLE_INT,MPI_MINLOC,0,MPI_COMM_WORLD);
    MPI_Reduce(&elapsed,&gsum,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    if (!rank) printf("NumProcs %d  Min %g@%d  Max %g@%d  Ratio %g  Ave %g\n",size,gmin.time,gmin.rank,gmax.time,gmax.rank,gmax.time/gmin.time,gsum/size);
  }

  collfs_finalize();
  MPI_Finalize();
  return 0;
}
