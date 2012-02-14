#include <stdio.h>
#include <mpi.h>

#include "collfs.h"

__attribute__((constructor))
void collfs_easy_initialize()
{
  int rank,size;

  MPI_Init(NULL,NULL);
  collfs_initialize(1, NULL);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  fprintf(stderr,"[%d/%d] Pushing MPI_COMM_WORLD\n",rank,size);
  collfs_comm_push(MPI_COMM_WORLD);
}

__attribute__((destructor))
void collfs_easy_finalize()
{
  collfs_comm_pop();
  collfs_finalize();
  MPI_Finalize();
}
