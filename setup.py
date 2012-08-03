from distutils.core import setup, Extension

	
setup(name='collective_zipimport', version="1.0",
      ext_modules=[Extension("collective_zipimport", ["collective_zipimport.c"])])


