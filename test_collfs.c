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

struct libc_collfs_api {
  void (*fxstat64)(void);
  void (*xstat64)(void);
  void (*open)(void);
  void (*close)(void);
  void (*read)(void);
  void (*lseek)(void);
  void (*mmap)(void);
  void (*munmap)(void);
};

struct libc_collfs_api _dl_collfs_api;

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

  err = dlclose(handle);
  if (err) ERR("dlclose failed due to: %s", dlerror());
  return 0;
}

int main(int argc, char *argv[])
{
  int err;
  char path[MAXPATHLEN];

  if (MPI_Init) {
    printf("MPI initialized\n");
    MPI_Init(&argc,&argv);
  }
  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libminimal_thefunc.so");
  err = run_tests(path, path);CHK(err);
  err = foo(path);CHK(err);
  err = foo2("alphabet.txt");CHK(err);
  if (MPI_Finalize) MPI_Finalize();
  return 0;
}
