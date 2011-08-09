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

int run_tests(const char *soname, const char *path)
{
  void *handle;
  int err;
  int (*func)(void);
  int (*test_fxstat64)(const char *);

  handle = dlopen(soname,RTLD_GLOBAL | RTLD_LAZY);
  if (!handle) ERR("failed to dlopen(\"%s\",RTLD_GLOBAL | RTLD_LAZY) due to: %s", soname, dlerror());

  func = dlsym(handle,"thefunc");
  if (!func) ERR("dlsym could not find symbol \"thefunc\" due to: %s", dlerror());
  err = (*func)();CHK(err);

  test_fxstat64 = dlsym(handle,"thetest_fxstat64");
  if (!func) ERR("dlsym could not find symbol \"thetest_fxstat64\" due to: %s", dlerror());
  err = (*test_fxstat64)(path);CHK(err);

  err = dlclose(handle);
  if (err) ERR("dlclose failed due to: %s", dlerror());
  return 0;
}

int main(int argc, char *argv[])
{
  int err;
  char path[MAXPATHLEN];

  if (MPI_Init) MPI_Init(&argc,&argv);
  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libthefunc.so");
  err = run_tests(path, path);CHK(err);
  err = foo(path);CHK(err);
  if (MPI_Finalize) MPI_Finalize();
  return 0;
}
