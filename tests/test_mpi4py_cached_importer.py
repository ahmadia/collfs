from mpi4py import MPI
import time

t = time.time()

import sys
from cached_import import mpi4py_finder
sys.meta_path.append(mpi4py_finder())

#import select
from clawpack import pyclaw
#import _socket

MPI.COMM_WORLD.Barrier()
elapsed = time.time() - t

if (MPI.COMM_WORLD.rank == 0):
    print "elapsed time: %f" % elapsed
