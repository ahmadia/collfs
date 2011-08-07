#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <mpi.h>

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
  int err,(*thefunc)(void),fd,rank;
  char path[MAXPATHLEN];
  unsigned int sum,value;

  MPI_Init(&argc,&argv);
  err = MPI_Comm_rank(MPI_COMM_WORLD,&rank);CHK(err);
  if (!getcwd(path,sizeof path)) ERR("getcwd failed");
  strcat(path,"/libthefunc.so");
  err = symlookup(path,"thefunc",(void(**)(void))&thefunc);CHK(err);
  (*thefunc)();
  fd = open(path,O_RDONLY);
  if (fd < 0) ERR("[%d] open(\"%s\",O_RDONLY) failed",rank,path);
  sum = 0;
  while (read(fd,&value,sizeof value) == sizeof value) {
    sum ^= value;
  }
  close(fd);
  printf("[%d] XOR-sum of \"%s\": %x\n",rank,path,sum);
  MPI_Finalize();
  return 0;
}
