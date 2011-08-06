#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>

int PrintFrame(int frame,const char *func,const char *file,int line) {fprintf(stderr,"Stack frame %d in %s() at %s:%d\n",frame,func,file,line); return 0;}
#define CHK(err) do { if (err) {PrintFrame(err,__func__,__FILE__,__LINE__); return err+1;} } while (0)
#define ERR(...) do {                                                   \
    char _buf[512];                                                     \
    snprintf(_buf,sizeof _buf,__VA_ARGS__);                             \
    fprintf(stderr,"ERROR: %s\n",_buf);                                 \
    PrintFrame(0,__func__,__FILE__,__LINE__);                           \
    return 1;                                                           \
  } while (0)

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

  MPI_Init(&argc,&argv);
  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libthefunc.so");
  err = symlookup(path,"thefunc",(void(**)(void))&thefunc);CHK(err);
  (*thefunc)();
  MPI_Finalize();
  return 0;
}
