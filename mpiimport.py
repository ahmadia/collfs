import os
import imp
import mpiimporter

class Importer:
    def __init__(self, path=None):
        if path is not None and not os.path.isdir(path):
            raise ImportError
        self.path = path

    def find_module(self, fullname, path=None):
        #        print "find_module", fullname, path

        subname = fullname.split(".")[-1]
        if subname != fullname and self.path is None:
            return None
        if self.path is None:
            path = None
        else:
            path = [self.path]
        try:
            file, filename, stuff = mpiimporter.find_module(subname, path)
        except ImportError:
            #            print ImportError
            return None

        #        print "find_module found: ", file, filename

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
        #        print "load_module: ", fullname, self.file, self.filename, self.stuff
        mod = mpiimporter.load_module(fullname, self.file, self.filename, self.stuff)
        if self.file:
            self.file.close()
        mod.__loader__ = self  # for introspection

                               #        print "load_module loaded: ", mod
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
