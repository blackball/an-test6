from distutils.core import setup, Extension
from numarray.numarrayext import NumarrayExtension
import sys
    
if not hasattr(sys, 'version_info') or sys.version_info < (2,2,0,'alpha',0):        raise SystemExit, "Python 2.2 or later required to build this module."
    
setup(name = "PyWrapperTemplate",
	version = "0.1",
	description = "",
	packages=[""],
	package_dir={"":""},
	ext_modules=[NumarrayExtension("PyWrapperTemplate",
                                 ['PyWrapperTemplate.c',
                                  'wrappertemplate_print.c'],
                                 include_dirs=["./"],
                                 library_dirs=["./"],
                                 libraries=['m'])])
