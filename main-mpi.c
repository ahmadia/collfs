#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <mpi.h>
#include <dlfcn.h>

#include "collfs.h"
#include "errmacros.h"

int run_tests(const char *soname)
{
  void *handle;
  int err;
  int (*func)(void);

  handle = dlopen(soname,RTLD_GLOBAL | RTLD_LAZY);
  if (!handle) ERR("failed to dlopen(\"%s\",RTLD_GLOBAL | RTLD_LAZY) due to: %s", soname, dlerror());

  func = dlsym(handle,"thefunc");
  if (!func) ERR("dlsym could not find symbol \"thefunc\" due to: %s", dlerror());
  err = (*func)();CHK(err);

  err = dlclose(handle);
  if (err) ERR("dlclose failed due to: %s", dlerror());
  return 0;
}

int main(int argc, char *argv[])
{
  int err;
  char path[MAXPATHLEN];

  MPI_Init(&argc,&argv);
  collfs_initialize(2, NULL);

  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libminimal_thefunc.so");

  collfs_comm_push(MPI_COMM_WORLD);
  err = run_tests(path);CHK(err);
  collfs_comm_pop();

  collfs_finalize();
  MPI_Finalize();
  return 0;
}
