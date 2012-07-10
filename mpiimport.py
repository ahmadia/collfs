import os
import imp
import mpiimporter

from mpi4py import MPI

class Importer:
    def __init__(self, path=None):
        if path is not None and not os.path.isdir(path):
            raise ImportError
        self.path = path

    def find_module(self, fullname, path=None):
        rank = MPI.COMM_WORLD.Get_rank()
        #        print "[%d] find_module %s %s" % (rank, fullname, path)

        subname = fullname.split(".")[-1]
        if subname != fullname and path is None:
            return None
        try:
            file, filename, stuff = mpiimporter.find_module(subname, path)
        except ImportError:
            #            print ImportError
            return None

        #        print "[%d] find_module found: %s %s" % (rank, file, filename)

        ignore, ext = os.path.splitext(filename)

        if ext == '.so':
            file, filename, stuff = imp.find_module(subname, path)
            return ImpLoader(file, filename, stuff)
        
        return Loader(file, filename, stuff)


class Loader:
    def __init__(self, file, filename, stuff):
        self.file = file
        self.filename = filename
        self.stuff = stuff

    def load_module(self, fullname):
        rank = MPI.COMM_WORLD.Get_rank()
        #        print "[%d] load_module: %s %s %s %s" % (rank, fullname, self.file, self.filename, self.stuff)
        mod = mpiimporter.load_module(fullname, self.file, self.filename, self.stuff)
        if self.file:
            self.file.close()
        mod.__loader__ = self  # for introspection

                               #        print "[%d] load_module loaded: %s" % (rank, mod)
        return mod

class ImpLoader:

    def __init__(self, file, filename, stuff):
        self.file = file
        self.filename = filename
        self.stuff = stuff

    def load_module(self, fullname):
        mod = imp.load_module(fullname, self.file, self.filename, self.stuff)
        if self.file:
            self.file.close()
        mod.__loader__ = self  # for introspection
        return mod
