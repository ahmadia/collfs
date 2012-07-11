import time

full_start = time.time()

from enchilada_import import mpi4py_finder
from mpiimport import Importer
from mpi4py import MPI
import sys

# all processes need to synch up 
MPI.COMM_WORLD.Barrier()
enchilada_start = time.time()

# try cached importer first
sys.meta_path.insert(0, mpi4py_finder())

# fall back to our importer
sys.meta_path.insert(1, Importer())

from clawpack import pyclaw

MPI.COMM_WORLD.Barrier() 
enchilada_elapsed = time.time() - enchilada_start
full_elapsed = time.time() - full_start


if (MPI.COMM_WORLD.rank == 0):
    print "elapsed time (enchilada): %f" % enchilada_elapsed
    print "elapsed time (full): %f" % full_elapsed
