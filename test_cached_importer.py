#import os
#print os.environ
from mpi4py import MPI

import sys
from collective_zipimport import collective_zipimporter

#sys.meta_path.insert(0, collective_zipimporter('./cache/bytecode.zip'))
#sys.path_hooks.insert(0, collective_zipimporter)

import import_cache
import_cache.disable_debug()

from import_cache import test_pickled_cache_dict

test_pickled_cache_dict()

