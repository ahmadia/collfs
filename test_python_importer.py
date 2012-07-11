import time
full_start = time.time()

from mpi4py import MPI

MPI.COMM_WORLD.Barrier()
t = time.time()
from clawpack import pyclaw
MPI.COMM_WORLD.Barrier()
elapsed = time.time() - t
full_elapsed = time.time() - full_start

if (MPI.COMM_WORLD.rank == 0):
    print "elapsed time (import): %f" % elapsed
    print "elapsed time (full): %f" % full_elapsed
