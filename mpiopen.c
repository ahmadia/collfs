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
   
#include <mpi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

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
    
    /* the file pointer for our memory file*/
    FILE * libmemfile;
    
    /* our MPI rank */
    int rank;
    
    int rc;
    rc=-1;

    /* the size of the library */
    long libsize;
    

    /* file mode to elimate special files and dirs */
    int libtype;

    /* buffer to hold the library */
    char * libfilebuffer;
    
    /* the file descriptor for the buffer */
    int fd;
    
    /* allow an escape if opening a file is NULL */
    
    int nofile;
    nofile=1;
    
    /* obtain rank*/
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    /* debug statement to ensure the MPI is progessing*/
#ifdef MPIDEBUG
    printf("Got rank!\n");
#endif
    
    /* initialize library size to something ridiculous */
    libsize=-1;
    libtype=-1;
   
    /* 
     do a stat from our reading rank only; we need the file size 
     to safely bcast and allocate buffers 
     */    
    if (rank==0)
    {
        struct stat libstat;
        rc=stat(libfilename,&libstat);
        if (rc==0)
        {
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
    
    #ifdef MPIDEBUG
    printf("rank %d |%d| libsize pre-bcast was %lu and stat rc %d for file %s type %d \n",rank,__LINE__,libsize,rc,libfilename,libtype);
    #endif
   /*
     bcast buffer size to all ranks
     */
    rcbc4=MPI_Bcast(&libsize,1,MPI_LONG,0,MPI_COMM_WORLD);
    
    #ifdef MPIDEBUG 
    printf("rank %d |%d| libsize post bcast %lu and stat rc %d for file %s type %d \n",rank,__LINE__,libsize,rcbc4,libfilename,libtype);


    printf("Bcasted!\n");
    #endif
    

if ((rc==0) && libtype){

    /*
     allocate a buffer on all ranks the size of the library
     we use calloc to insure initialization - not a big deal
     on the bluegene; it might be wiser to use posix_memalign
     but we're not worrying about that quite yet
     */
    libfilebuffer=calloc((libsize)+1,sizeof(char));
    libfilebuffer[libsize]='\0';

    #ifdef MPIDEBUG
    printf("rank %d |%d| libfilebuffer size %d asked %lu and preparing to ship %lu \n",rank,__LINE__,sizeof(libfilebuffer),libsize+1,libsize);
    #endif
    
    if (rank==0)
    {
        
        /*
         we create a file handle here as only rank 0 ever needs to know
         about this file pointer
         */
        FILE * f;
        
        /*
         we open read only; this is a bottleneck and may be replaced by
         a call to a "library file server" that uses sockets and does
         preresolution of all dependant symbols
         */
        f=fopen(libfilename,flags);
        
        /*
         if opening worked, read in the full file in a single shot
         then close the file - we'll never use it again if it fails
         if opening fails, game over and return NULL
         
         todo: error handling
         */
        if (f!=NULL){
            read(fileno(f),libfilebuffer,libsize); // Asking upto x amount of data; need a while here.
            
            fclose(f);
            
            nofile=0;
            
        }
        else
        {
            
            
            nofile=1;
        }
        
    }
    
    
    MPI_Bcast(&nofile,1,MPI_INT,0,MPI_COMM_WORLD);
    
    if (nofile==1)
    {
        return NULL;
    }
    
    /*
     take advantage of cheap and fast broadcast networks to get
     around the hazards of i/o and metadata
     */
    rc=MPI_Bcast(libfilebuffer,libsize,MPI_CHAR,0,MPI_COMM_WORLD);
    #ifdef MPIDEBUG
    MPI_Barrier(MPI_COMM_WORLD);
    printf("rank %d |%d| rc %d from bcast of file size %lu \n", rank,__LINE__, rc, libsize);
    #endif
    
    /*
     we're trusting that bcast worked, if it did - awesome if not,
     we'll be segfaulting fairly shortly
     */
    libmemfile=fmemopen(libfilebuffer,libsize,"rb+");
    if (libmemfile)
    {
    fflush(libmemfile);
    fseek(libmemfile, 0, SEEK_SET);
    }
    
    return libmemfile;

}
return NULL;

}

/*
function: __open(const char *pathname, int flags)

in: const char *pathname, int flags

returns: int fd;

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
int mpiopen(const char *libfilename, int flags)
{
    
    /* the file pointer for our memory file*/
    FILE * libmemfile;
    
    /* our MPI rank */
    int rank;
    
    /* the size of the library */
    size_t libsize;
    
    /* buffer to hold the library */
    char * libfilebuffer;
    
    /* the file descriptor for the buffer */
    int fd;

    /* obtain rank*/
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    /* debug statement to ensure the MPI is progessing*/
    #ifdef MPIDEBUG
    printf("Got rank!\n");
    #endif
    
    /* initialize library size to something ridiculous */
    libsize=-1;


    /* 
    do a stat from our reading rank only; we need the file size 
    to safely bcast and allocate buffers 
    */    
    struct stat libstat;
    mpistat(libfilename,&libstat);
    libsize=libstat.st_size;
    
    #ifdef MPIDEBUG
    printf("Bcasting!\n");
    #endif
    
    /*
    bcast buffer size to all ranks
    */
    /* using mpistat should relegate this function to history
    MPI_Bcast(&libsize,1,MPI_INT,0,MPI_COMM_WORLD);
    */ 
    #ifdef MPIDEBUG
    printf("Bcasted!\n");
    #endif
    
    /*
    allocate a buffer on all ranks the size of the library
    we use calloc to insure initialization - not a big deal
    on the bluegene; it might be wiser to use posix_memalign
    but we're not worrying about that quite yet
    */
    libfilebuffer=calloc((libsize)+1, sizeof(char));
    
    /* 
    append a NULL to the end of the buffer as insurance 
    against a whacked glibc 
    */
    libfilebuffer[libsize]='\0'; //added insurance against whacked glibc
    
    #ifdef MPIDEBUG
    printf("Segfault?!\n");
    #endif
        
    if (rank==0)
    {
    
        /*
        we create a file handle here as only rank 0 ever needs to know
        about this file pointer
        */
        FILE * f;
        
        /*
        we open read only; this is a bottleneck and may be replaced by
        a call to a "library file server" that uses sockets and does
        preresolution of all dependant symbols
        */
        f=fopen(libfilename,"rb");

        /*
        if opening worked, read in the full file in a single shot
        then close the file - we'll never use it again if it fails
        if opening fails, game over and return NULL
        
        todo: error handling
        */
        if (f!=NULL){
            read(fileno(f),libfilebuffer,libsize);
            
            #ifdef MPIDEBUG
            printf("Segfault?!\n");
            #endif
            fclose(f);
        }
        else
        {
            return(NULL);
        }
    
     }
    
    
    /*
    take advantage of cheap and fast broadcast networks to get
    around the hazards of i/o and metadata
    */
    MPI_Bcast(libfilebuffer,libsize+1,MPI_CHAR,0,MPI_COMM_WORLD);
    
    
    /*
    we're trusting that bcast worked, if it did - awesome if not,
    we'll be segfaulting fairly shortly
    */
    libmemfile=fmemopen(libfilebuffer,libsize,"rb+");
    fflush(libmemfile);
    fseek(libmemfile, 0, SEEK_SET);
    fd=fileno(libmemfile);

return fd;
}



/*
function: __fxstat(int ver, int fildes, struct stat64 * stat_buf)

in: int ver, int fildes, struct stat64 * stat_buf

returns: int with 0 or -1 to indicate success on error errno is also set

desc: a replacement for the libc fxstat file call that uses mpi to bcast the
      stat struct out to interested clients. 
      
*/
int mpifxstat64(int ver, int filedes, struct stat * stat_buf)
{
    /* used to ensure the safe transmission of struct data */
    int statstructsize;
    int rc;
    
    int bcast_rc;
 
    int rank;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
     
    if(rank==0)
    {
    rc=__fxstat64(ver,filedes,stat_buf);
    statstructsize=sizeof(stat_buf);
    }
    
    bcast_rc=MPI_Bcast(&rc,1,MPI_INT,0,MPI_COMM_WORLD);
    
    if((rc==0) && (bcast_rc==0))
    {
    MPI_Bcast(&statstructsize,1,MPI_INT,0,MPI_COMM_WORLD);
    
    MPI_Bcast(stat_buf,statstructsize,MPI_CHAR,0,MPI_COMM_WORLD);
    }

    return rc;
}


/*
 function: __stat(const char *restrict path, struct stat *restrict buf)
 
 in: int ver, const char * path, struct stat64 * stat_buf
 
 returns: int with 0 or -1 to indicate success on error errno is also set
 
 desc: a replacement for the libc __stat file call that uses mpi to bcast the
 stat struct out to interested clients. 
 
 */
int mpistat(char *path, struct stat *stat_buf)
{
    int rank;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    
    /* used to ensure the safe transmission of struct data */
    int statstructsize;
    
    /* carries the return code - the important part */
    int rc;
    int bcast_rc;
    int st_mode;
    
    /* 
     as with other functions, rank 0 is the only one interacting with the file
     */
    
    #ifdef MPIDEBUG
    printf("rank %d |%d| is in mpistat of %s\n",rank,__LINE__,path);
    #endif
    
    if(rank==0)
    {
        rc=stat(path,stat_buf);
        st_mode=stat_buf->st_mode;
        statstructsize=sizeof(stat_buf);
    }
    
    #ifdef MPIDEBUG
    printf("rank %d |%d| pre-bc sees the stat rc as %d for %s\n",rank,__LINE__,rc,path);
    printf("rank %d |%d| pre-bc sees the statstructsize as %d for %s\n",rank,__LINE__,statstructsize,path);
    #endif
    bcast_rc=MPI_Bcast(&rc,1,MPI_INT,0,MPI_COMM_WORLD);
    bcast_rc=MPI_Bcast(&st_mode,1,MPI_INT,0,MPI_COMM_WORLD);
    
    #ifdef MPIDEBUG
    printf("rank %d |%d| post-bc sees the stat rc as %d for %s\n",rank,__LINE__,rc, path);
    #endif
    
    if((rc==0) && (bcast_rc))
    {
        MPI_Bcast(&statstructsize,1,MPI_INT,0,MPI_COMM_WORLD);
        
       /* MPI_Bcast(stat_buf,statstructsize,MPI_CHAR,0,MPI_COMM_WORLD); */
    }
    
    #ifdef MPIDEBUG
    printf("rank %d |%d| post-bc sees the statstructsize as %d for %s\n",rank,__LINE__,statstructsize,path);
    #endif
    
    
    stat_buf->st_mode=st_mode;
    
    return rc;
}


/*
function: __xstat(int ver, const char * path, struct stat64 * stat_buf)

in: int ver, const char * path, struct stat64 * stat_buf

returns: int with 0 or -1 to indicate success on error errno is also set

desc: a replacement for the libc __xstat file call that uses mpi to bcast the
      stat struct out to interested clients. 
      
*/
int mpixstat64(int ver, const char * path, struct stat  * stat_buf)
{
    /* our mpi rank */
    int rank;

    /* obtain rank*/
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);


    /* used to ensure the safe transmission of struct data */
    int statstructsize;
    
    /* carries the return code - the important part */
    int rc;
    
    /* 
    as with other functions, rank 0 is the only one interacting with the file
    */
    if(rank==0)
    {
    rc=__xstat64(ver,path,stat_buf);
    statstructsize=sizeof(stat_buf);
    }
    
    rc=MPI_Bcast(&rc,1,MPI_INT,0,MPI_COMM_WORLD);
    
    if(rc==0)
    {
    MPI_Bcast(&statstructsize,1,MPI_INT,0,MPI_COMM_WORLD);
    
    MPI_Bcast(stat_buf,statstructsize,MPI_CHAR,0,MPI_COMM_WORLD);
    }

    return rc;
}

/*
 function: __fclose(FILE *stream)
 
 in: FILE *stream
 
 returns: int on successful completion, fclose() shall return 0; otherwise, it shall return EOF and set errno to indicate the error.
 
 desc: a replacement for the libc __fclose file call; while this is a wonderfully serial (read pointless) function, it's here for completeness
 
 */
int mpifclose(FILE *stream)
{
    
    //simple right now
    
    fclose(stream);
}

/* walling off our earlier test function
int main(int argc, char *argv[])
{

int fd;
char * libfilename;
libfilename="/lib/libm.so.6";
    
MPI_Init (&argc, &argv);

fd = __mpiopen(libfilename,O_RDONLY);

MPI_Finalize();

return 0;
}
*/
