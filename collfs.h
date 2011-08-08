#ifndef _collfs_h
#define _collfs_h

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int __collfs_open(const char *pathname,int flags,...);
int __collfs_close(int fd);
int __collfs_read(int fd,void *buf,size_t count);

#endif
