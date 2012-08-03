"""import_cache - bytecode and extension module caching for fast import

The methods in this module are responsible for assembling and using caches of
bytecode and extension modules for fast imports over networked/high-latency file
systems.  The resulting dual import cache (bytecode into a zip file, extension
modules under a single cache directory) is designed to be used with the
CachedImporter object.  See the build_import_cache method for usage details, and
the test_cached_importer method for a complete example.

This module uses the standard library logging package,
call import_cache.disable_debug() for less verbose output
"""

import os
import cPickle as pickle
import shutil
import logging
import zipfile
import os
import imp

logging.basicConfig(level = logging.DEBUG,
                    format = 'import_cache.py - %(message)s')
#logging.debug('logging enabled for import_cache module, call disable_debug() to silence')

def disable_debug():
    logging.disable(logging.INFO)

def build_import_cache(search_paths=None,
                       bytecode_zipfile=None,
                       so_dir='./cache/so',
                       so_cache_pickle=None):
    """build_import_cache - build a fast import cache of bytecode and extensions

    The build_import_cache function assembles a dual cache of bytecode and extension
    modules.  The bytecode is stored into a single zip file, identified by zipname.  

    search_paths - a list of paths to cache (same format as sys.path),
    default: ['./test']
    Packages with the same name are not allowed in search_paths

    bytecode_zipfile - an open zipfile to store cached bytecode in,
    './cache/bytecode.zip' is opened by default if None or not specified

    so_dir - the directory to store cached extension modules in,
    './cache/so' is used by default if not specified

    so_cache_pickle - the file to store the dict mapping .so extension module
    names to their locations in the cached so_dir, './cache/so/dict.pkl' is used
    by default if None or not specified
    
    returns (bytecode_zipfile_name, so_dir, so_cache_pickle_name, so_cache_dict)
    """

    if search_paths is None:
        search_paths = ['./test']

    so_cache_dict = {}

    if not os.path.exists(so_dir):
        logging.debug('creating new directory ' + so_dir)
        os.makedirs(so_dir)

    if bytecode_zipfile is None:
        with zipfile.ZipFile('./cache/bytecode.zip', 'w') as bytecode_zipfile:
            for top_search_dir in search_paths:
                path_walk_filter(top_search_dir, bytecode_zipfile, so_dir, so_cache_dict)
                bytecode_zipfile_name = bytecode_zipfile.filename
    else:
        for top_search_dir in search_paths:
            path_walk_filter(top_search_dir, bytecode_zipfile, so_dir, so_cache_dict)
            bytecode_zipfile_name = bytecode_zipfile.filename

    logging.debug('bytecode cache archive saved to ' + bytecode_zipfile_name)
    logging.debug('so_cache saved to ' + so_dir)

    if so_cache_pickle is None:
        with open('./cache/so/dict.pkl', 'w') as so_cache_pickle:
            pickle.dump(so_cache_dict, so_cache_pickle)
            so_cache_pickle_name = so_cache_pickle.name
    else:
        pickle.dump(so_cache_dict, so_cache_pickle)
        so_cache_pickle_name = so_cache_pickle.name

    logging.debug('pickle dictionary saved to ' + so_cache_pickle_name)

    return (bytecode_zipfile_name, so_dir, so_cache_pickle_name, so_cache_dict)
    
def path_walk_filter(top_search_dir, bytecode_zipfile, so_dir, so_cache_dict):
    """path_walk_filter - os path-walking function for import caching

    This function is responsible for caching compiled bytecode and .so extension
    modules from a particular top-level directory.  The bytecode is stored in
    the bytecode_zipfile, with its relative path to top_search_dir preserved.
    Similarly, the .so files are copied to the so_dir with their relative paths
    from top_search_dir preserved.

    top_search_dir is explicitly assumed to be a path containing Python packages, so all 
    top-level Python modules will be copied into their respective caches.

    Bytecode-caching functionality is similar to PyZipFile with the added
    capability of extension module caching.
    """

    ignore, dir_names, file_names = os.walk(top_search_dir).next()

    cache_files(top_search_dir, '', bytecode_zipfile, so_dir, file_names, so_cache_dict)
    
    for dir_name in dir_names:
        package_walk_filter(top_search_dir, dir_name,
                            bytecode_zipfile, so_dir, so_cache_dict)

def package_walk_filter(top_search_dir, package_dir, bytecode_zipfile, so_dir, so_cache_dict):
    """package_walk_filter - os package-walking helper function for import caching

    This function walks the given path and recursively caches compiled bytecode
    and .so extension modules at each level
    """

    abs_package_dir = os.path.join(top_search_dir, package_dir)
    ignore, dir_names, file_names = os.walk(abs_package_dir).next()

    if not ('__init__.py' in file_names or '__init__.pyc' in file_names):
        logging.debug('skipping non-package directory: ' + abs_package_dir)
        return

    cache_files(top_search_dir, package_dir, bytecode_zipfile, so_dir, file_names, so_cache_dict)
    
    for dir_name in dir_names:
        package_walk_filter(top_search_dir, os.path.join(package_dir, dir_name),
                            bytecode_zipfile, so_dir, so_cache_dict)

def cache_files(top_search_dir, package_dir, bytecode_zipfile, so_dir, file_names, so_cache_dict):
    """cache_files - cache bytecode and extension modules among file_names"""

    for f in file_names:
        # add all .pyc files into the zipfile
        if f.endswith('.pyc'):
            src = os.path.join(top_search_dir, package_dir, f)
            dst = os.path.join(package_dir, f)
            logging.debug('appending %s to zipfile as %s' % (src, dst))
            bytecode_zipfile.write(src,dst)

        # copy all .so files into the so_dir directory 
        if f.endswith('.so'):
            src     = os.path.join(top_search_dir, package_dir, f)
            dst_dir = os.path.join(so_dir, package_dir)
            dst     = os.path.join(dst_dir, f)
            logging.debug('copying %s to %s' % (src, dst))
            if not os.path.exists(dst_dir):
                os.makedirs(dst_dir)
            shutil.copy(src, dst)
            package_name = package_dir.replace(os.path.sep,'.')
            full_module_name = package_name+'.'+f.rsplit('.so')[0]
            logging.debug('caching extension module %s in path %s',
                          full_module_name, dst_dir)
            so_cache_dict[full_module_name] = dst_dir

class CachedImporter:
    """CachedImporter - class for implementing PEP 302 searches for cached .so"""
    def __init__(self,  so_cache_dict, path=None):
        if path is not None and not os.path.isdir(path):
            raise ImportError
        self.path = path
        self.so_cache_dict = so_cache_dict

    def find_module(self, fullname, path=None):
        if fullname not in self.so_cache_dict:
            return None

        so_path = self.so_cache_dict[fullname]
        logging.debug('Importer loading cached %s from %s'
                      % (fullname, so_path))

        subname = fullname.split(".")[-1]
        file, filename, stuff = imp.find_module(subname, [so_path])

        logging.debug('imp.find_module located cached: ' + filename)
        return CachedLoader(file, filename, stuff)

class CachedLoader:
    """CachedLoader - class for implementing PEP 302 loads of found modules"""
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
            
def test_build_import_cache():
    """ test both the build and import cache mechanism, pollutes sys.meta_path and sys.path """

    (bytecode_zipfile_name,
     so_dir,
     so_cache_pickle_name,
     so_cache_dict) = build_import_cache()

    test_numpy_import(bytecode_zipfile_name, so_cache_dict)

def test_pickled_cache_dict():
    """ test importing a previously built cache, pollutes sys.meta_path and sys.path """
    
    bytecode_zipfile_name = './cache/bytecode.zip'
    so_cache_pickle_file_name = './cache/so/dict.pkl'

    with open(so_cache_pickle_file_name, 'rb') as pickle_file:
        so_cache_dict = pickle.load(pickle_file)

    test_numpy_import(bytecode_zipfile_name, so_cache_dict)

def test_numpy_import(bytecode_zipfile_name, so_cache_dict):
    import sys

    sys.meta_path.insert(0, CachedImporter(so_cache_dict))
    sys.path.insert(0, bytecode_zipfile_name)

    import numpy
    import zipimport
    import collective_zipimport

    logging.debug('asserting numpy loader is zipimport or collective_zipimport')
    assert(numpy.__loader__.__class__ == zipimport.zipimporter or 
           numpy.__loader__.__class__ == collective_zipimport.collective_zipimporter)
    logging.debug('ok')
    logging.debug('asserting numpy.core.multiarray loader is import_cache.CachedLoader')
    assert(numpy.core.multiarray.__loader__.__class__ == CachedLoader)
    logging.debug('ok')
    logging.debug('asserting numpy loader is at least somewhat functional')
    assert(numpy.random.randint(1) >= 0)
    logging.debug('ok')

if __name__ == 'main':
    test_build_import_cache()
