/**
 * @file   mpiopen.h
 * @author Aron Ahmadia <aron@peregrine.local>
 * @date   Mon Jul 25 19:47:34 2011
 * 
 * @brief  Public interface for mpiopen commands (should we add routines that can use MPI communicators?)
 * 
 * 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

FILE * mpifopen(const char *libfilename, const char * flags);
int mpiopen(const char *libfilename, int flags);
int mpifxstat64(int ver, int filedes, struct stat * stat_buf);
int mpistat(char *path, struct stat *stat_buf);
int mpixstat64(int ver, const char * path, struct stat  * stat_buf);
int mpifclose(FILE *stream);

