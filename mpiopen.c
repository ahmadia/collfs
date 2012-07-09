/*  Map in a shared object's file and return a fd... in ram
    Copyright (C) 2010, 2011 Argonne National Laboratory
    Written by William Scullin <wscullin_the at sign_alcf.anl.gov>
    
    This research used resources of the Argonne Leadership Computing 
    Facility at Argonne National Laboratory, which is supported by the 
    Office of Science of the U.S. Department of Energy 
    under contract DE-AC02-06CH11357.
    
    support from the KAUST Supercomputing Laboratory and 
    Prof. David Ketcheson, MCSE, KAUST
    
    special thanks to Dr. Aron Ahmadia, Dr. Nichols Romero, and Dr. Jeff Hammond
    
    
    This file is not fit for human consumption
   
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
   
*/
   
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <mpi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

FILE * myfmemopen (void *buf, size_t len, const char *mode);
int mpistat(const char *path, struct stat *stat_buf);


/*
 function: mpifopen(const char *pathname, int flags)
 
 in: const char *pathname, int flags
 
 returns: FILE;
 
 desc: a replacement for the libc open file call that uses mpi to bcast a file
 the sort of thing that is useful on architectures like Blue Gene with
 poor i/o performance and a really rocking interconnect.
 
 rank 0 does all the reading and does a bcast to MPI_COMM_WORLD this is
 somewhat janky in applications with subcommunicators that don't load a
 library as it will block. In SPI-land we'd set a time out and toss the
 buffer.
 
 we ignore important things like open flags, though they are on the todo
 list. we also provide no error handling atm, that's a big todo.
 
 just like the libc open, when done, you get a numerical file descriptor
 though instead of pointing at a real file it points at a memory buffer
 that has no physical representation
 
 */

FILE * mpifopen(const char *libfilename, const char * flags)
{   
  FILE * libmemfile;
  int rank;  
  int rc = -1;
  long libsize;
  int libtype;
  char * libfilebuffer;
  int nofile = -1;
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  libsize=-1;
  libtype=-1;
  
  if (rank==0) {
    struct stat libstat;
    rc=stat(libfilename,&libstat);
    if (rc==0) {
      libsize=libstat.st_size;
      libtype=S_ISREG(libstat.st_mode);
    }
    else {
      libsize=0;
      libtype=0;
    }
    
#ifdef MPIDEBUG
    printf("rank %d |%d| libsize on  was determined to be %lu rc %d for file %s \n",rank,__LINE__,libsize,rc,libfilename);
#endif
  }
    
  int rcbc2, rcbc3, rcbc4;
 
  rcbc2=0;
  rcbc3=0;
  rcbc4=0;

  rcbc2=MPI_Bcast(&rc,1,MPI_INT,0,MPI_COMM_WORLD);
  rcbc3=MPI_Bcast(&libtype,1,MPI_INT,0,MPI_COMM_WORLD);
    
  rcbc4=MPI_Bcast(&libsize,1,MPI_LONG,0,MPI_COMM_WORLD);
    
#ifdef MPIDEBUG 
  MPI_Barrier(MPI_COMM_WORLD);  
  printf("rank %d |%d| libsize %lu and stat rc %d for file %s type %d \n",rank,__LINE__,libsize,rcbc4,libfilename,libtype);
#endif
    

  if ((rc==0) && libtype) {
    libfilebuffer=calloc((libsize)+1,sizeof(char));
    libfilebuffer[libsize]='\0';
    
#ifdef MPIDEBUG
    MPI_Barrier(MPI_COMM_WORLD);
    printf("rank %d |%d| libfilebuffer asked %lu and preparing to ship %lu \n",rank,__LINE__,libsize+1,libsize);
#endif
    
    if (rank==0) {
      FILE * f;
      f=fopen(libfilename,flags);
      if (f!=NULL){
        read(fileno(f),libfilebuffer,libsize); // Asking upto x amount of data; need a while here.
        fclose(f);
        nofile=0;
      }
      else {
        nofile=1;
      } 
    }
    
    MPI_Bcast(&nofile,1,MPI_INT,0,MPI_COMM_WORLD);
#ifdef MPIDEBUG
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    
    if (nofile==1) {
        return NULL;
    }

    rc=MPI_Bcast(libfilebuffer,libsize,MPI_CHAR,0,MPI_COMM_WORLD);
    #ifdef MPIDEBUG
    MPI_Barrier(MPI_COMM_WORLD);
    printf("rank %d |%d| rc %d from bcast of file size %lu \n", rank,__LINE__, rc, libsize);
    #endif
    
    libmemfile=myfmemopen(libfilebuffer,libsize,"rb+");
    if (libmemfile) {
      fflush(libmemfile);
      fseek(libmemfile, 0, SEEK_SET);
#ifdef MPIDEBUG
      MPI_Barrier(MPI_COMM_WORLD);
#endif
    }
    else {
#ifdef MPIDEBUG
      MPI_Barrier(MPI_COMM_WORLD);
      printf("rank %d |%d| unable to open file %s with flags %s \n", rank,__LINE__, libfilename, flags);
#endif
      return NULL;
    }

    return libmemfile;
  }
#ifdef MPIDEBUG
  MPI_Barrier(MPI_COMM_WORLD);
  printf("rank %d |%d| unable to open file %s with flags %s (rc != 0)\n", rank,__LINE__, libfilename, flags);
#endif  
  return NULL;
}


/*
 function: mpistat(const char *restrict path, struct stat *restrict buf)
  
 desc: a replacement for the libc stat file call that uses mpi to bcast the
 stat struct out to interested clients. 
 
 */
int mpistat(const char *path, struct stat *stat_buf)
{
  int rank;
  int statstructsize;
  int rc;
  int bcast_rc;
  int st_mode;
    
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
#ifdef MPIDEBUG
  MPI_Barrier(MPI_COMM_WORLD);
  printf("rank %d |%d| is in mpistat of %s\n",rank,__LINE__,path);
#endif
  
  if(rank==0) {
    rc=stat(path,stat_buf);
    st_mode=stat_buf->st_mode;
    statstructsize=sizeof(stat);
  }
    
  bcast_rc=MPI_Bcast(&rc,1,MPI_INT,0,MPI_COMM_WORLD);
  bcast_rc=MPI_Bcast(&st_mode,1,MPI_INT,0,MPI_COMM_WORLD);
  
#ifdef MPIDEBUG
  MPI_Barrier(MPI_COMM_WORLD);
  printf("rank %d |%d| post-bc sees the stat rc as %d for %s\n",rank,__LINE__,rc, path);
#endif
    
  if(rc==0) {
    MPI_Barrier(MPI_COMM_WORLD);
    printf("rank %d |%d| about to broadcast %zu bytes of stat buf %s\n",rank,__LINE__,sizeof(struct stat),path);
    MPI_Bcast(stat_buf,sizeof(struct stat),MPI_BYTE,0,MPI_COMM_WORLD);
#ifdef MPIDEBUG
    MPI_Barrier(MPI_COMM_WORLD);
    printf("rank %d |%d| has broadcast %zu bytes of stat buf %s\n",rank,__LINE__,sizeof(struct stat),path);
#endif
  }    
  
  return rc;
}

int mpifclose(FILE *stream)
{
    return fclose(stream);
}
