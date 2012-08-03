from distutils.core import setup, Extension

	
setup(name='buffered_zipimport', version="1.0",
      ext_modules=[Extension("buffered_zipimport", ["buffered_zipimport.c"])])


