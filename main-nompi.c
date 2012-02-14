/* This is a plain non-MPI program that loads a dynamic shared object and calls a function in it. This executable does
 * not link MPI or libcollfs.so. In order to be invoked as normal, it should use the patched dynamic loader.
 *
 *    -Wl,--dynamic-linker=/path/to/ld-collfs.so
 *
 * To intercept dlopen() calls, collfs needs to be loaded. Currently, this can be done with LD_PRELOAD.
 *
 * mpiexec.mpich -env LD_PRELOAD /path/to/libcollfs-easy.so -n NP ./main-nompi
 * mpiexec.ompi -x LD_PRELOAD=/path/to/libcollfs-easy.so -n NP ./main-nompi
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <dlfcn.h>

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

int main(UNUSED int argc, UNUSED char *argv[])
{
  int err;
  char path[MAXPATHLEN];

  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libminimal_thefunc.so");
  err = run_tests(path);CHK(err);
  return 0;
}
