#ifndef _errmacros_h
#define _errmacros_h

#include <stdio.h>

int PrintFrame(int frame,const char *func,const char *file,int line) {fprintf(stderr,"Stack frame %d in %s() at %s:%d\n",frame,func,file,line); return 0;}
#define CHK(err) do { if (err) {PrintFrame(err,__func__,__FILE__,__LINE__); return err+1;} } while (0)
int HandleError(const char *func,const char *file,int line,const char *msg)
{
  fprintf(stderr,"ERROR: %s\n",msg);
  PrintFrame(0,func,file,line);
  return 1;
}
#define ERR(...) do {                                                   \
    char _buf[512];                                                     \
    snprintf(_buf,sizeof _buf,__VA_ARGS__);                             \
    return HandleError(__func__,__FILE__,__LINE__,_buf);                \
  } while (0)

#endif
