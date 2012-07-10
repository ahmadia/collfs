from mpiimport import Importer
from mpi4py import MPI
import time

t = time.time()

import sys
sys.meta_path.insert(0, Importer())
sys.path_hooks.append(Importer)

from clawpack import pyclaw

MPI.COMM_WORLD.Barrier()
elapsed = time.time() - t

if (MPI.COMM_WORLD.rank == 0):
    print "elapsed time: %f" % elapsed
