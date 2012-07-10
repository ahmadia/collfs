from mpi4py import MPI
import time

t = time.time()
from clawpack import pyclaw
MPI.COMM_WORLD.Barrier()
elapsed = time.time() - t

if (MPI.COMM_WORLD.rank == 0):
    print "elapsed time: %f" % elapsed
