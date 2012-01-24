#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>

#include "errmacros.h"
#include "foo.h"


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

typedef int (*collfs_open_fp)(const char *pathname, int flags, mode_t mode);

int minimal_open(const char *pathname, int flags, mode_t mode)
{
  printf("Intercepted open call to: %s\n", pathname);
  return open(pathname, flags, mode);
}

int run_tests(const char *soname, const char *path)
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
  collfs_open_fp _open_fp;
  char path[MAXPATHLEN];

  MPI_Init(&argc,&argv);
  
  _dl_collfs_api.open = (void *) minimal_open;
  _open_fp = (collfs_open_fp) _dl_collfs_api.open;  
  int fid = _open_fp("test_open.txt",O_RDONLY,0);
  close(fid);
  
  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libthefunc.so");
  err = run_tests(path, path);CHK(err);
  
  MPI_Finalize();
  return 0;
}

