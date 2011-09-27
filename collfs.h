#ifndef _collfs_h
#define _collfs_h

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <mpi.h>

int collfs_initialize(int debuglevel, void(*fp)(void));
int collfs_set_debug_level(int debuglevel);
int collfs_set_error_handler(void (*fp)(void));
int collfs_finalize(void);
int collfs_comm_push(MPI_Comm comm);
int collfs_comm_pop(void);

#endif
