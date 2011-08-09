#ifndef _collfs_h
#define _collfs_h

#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mpi.h>

int __collfs_fxstat64(int vers, int fd, struct stat64 *buf);
int __collfs_xstat64(int vers, const char *file, struct stat64 *buf);
int __collfs_open(const char *pathname, int flags,...);
int __collfs_close(int fd);
int __collfs_read(int fd, void *buf, size_t count);
int __collfs_comm_push(MPI_Comm comm);
int __collfs_comm_pop(void);

#endif
