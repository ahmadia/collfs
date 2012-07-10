from enchilada_import import mpi4py_finder
from mpiimport import Importer
from mpi4py import MPI
import sys
import time

t = time.time()

# try cached importer first
sys.meta_path.insert(0, mpi4py_finder())

# fall back to our importer
sys.meta_path.insert(1, Importer())

from clawpack import pyclaw

MPI.COMM_WORLD.Barrier()
elapsed = time.time() - t

if (MPI.COMM_WORLD.rank == 0):
    print "elapsed time: %f" % elapsed
