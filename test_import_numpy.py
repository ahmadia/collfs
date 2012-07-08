#!/usr/bin/python -B


from mpiimport import Importer
from mpi4py import MPI
import sys
sys.meta_path.insert(0, Importer())
sys.path_hooks.append(Importer)

#import numpy
 
mnames = ("colorsys", "urlparse", "distutils.core", "compiler.misc", "numpy")

for mname in mnames:
    m = __import__(mname, globals(), locals(), ["__dummy__"])
    m.__loader__  # to make sure we actually handled the import
