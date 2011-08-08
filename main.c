#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <unistd.h>

#include <mpi.h>

#include "errmacros.h"
#include "foo.h"

extern int MPI_Init(int *argc, char ***argv) __attribute__ ((weak));
extern int MPI_Finalize() __attribute__ ((weak));

int symlookup(const char *soname,const char *fname,void (**func)(void))
{
  void *handle;
  handle = dlopen(soname,RTLD_GLOBAL | RTLD_LAZY);
  if (!handle) ERR("failed to dlopen(\"%s\",RTLD_GLOBAL | RTLD_LAZY)",soname);
  *func = dlsym(handle,fname);
  if (!func) ERR("dlsym could not find symbol %s",fname);
  return 0;
}

int main(int argc, char *argv[])
{
  int err,(*thefunc)(void);
  char path[MAXPATHLEN];

  if (MPI_Init) MPI_Init(&argc,&argv);
  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libthefunc.so");
  err = symlookup(path,"thefunc",(void(**)(void))&thefunc);CHK(err);
  (*thefunc)();
  err = foo(path);CHK(err);
  if (MPI_Finalize) MPI_Finalize();
  return 0;
}
