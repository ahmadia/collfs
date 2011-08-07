#ifndef _errmacros_h
#define _errmacros_h

#include <stdio.h>

int PrintFrame(int frame,const char *func,const char *file,int line) {fprintf(stderr,"Stack frame %d in %s() at %s:%d\n",frame,func,file,line); return 0;}
#define CHK(err) do { if (err) {PrintFrame(err,__func__,__FILE__,__LINE__); return err+1;} } while (0)
#define ERR(...) do {                                                   \
    char _buf[512];                                                     \
    snprintf(_buf,sizeof _buf,__VA_ARGS__);                             \
    fprintf(stderr,"ERROR: %s\n",_buf);                                 \
    PrintFrame(0,__func__,__FILE__,__LINE__);                           \
    return 1;                                                           \
  } while (0)

#endif
