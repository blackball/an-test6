from distutils.core import setup, Extension
import os.path
from astrometry.util.run_command import run_command

import numpy
numpy_inc = numpy.get_include()

def get_libs(pkg):
	(rtn,out,err) = run_command('pkg-config --libs-only-l ' + pkg)
	if rtn:
		raise Exception('Failed to find libraries for package ' + pkg)
	if err and len(err):
		print 'pkg-config complained:', err
	#print 'pkg-config said:', out
	#libs = out.replace('\n', ' ').split(' ')
	libs = out.split()
	libs = [l for l in libs if len(l)]
	# Strip off the leading "-l"
	libs = [l[2:] for l in libs]
	print 'returning libs:', libs
	return libs

def get_include_dirs(pkg):
	(rtn,out,err) = run_command('pkg-config --cflags-only-I ' + pkg)
	if rtn:
		raise Exception('Failed to find include paths for package ' + pkg)
	if err and len(err):
		print 'pkg-config complained:', err
	dirs = out.split()
	dirs = [l for l in dirs if len(l)]
	# Strip off the leading "-I"
	dirs = [l[2:] for l in dirs]
	print 'returning include dirs:', dirs
	return dirs

def get_lib_dirs(pkg):
	(rtn,out,err) = run_command('pkg-config --libs-only-L ' + pkg)
	if rtn:
		raise Exception('Failed to find libraries for package ' + pkg)
	if err and len(err):
		print 'pkg-config said:', err
	libs = out.split()
	libs = [l for l in libs if len(l)]
	# Strip off the leading "-L"
	libs = [l[2:] for l in libs]
	return libs

netpbm_inc = os.environ.get('NETPBM_INC', '')
netpbm_lib = os.environ.get('NETPBM_LIB', '-lnetpbm')

compile_args = ['-O0', '-g']
if len(netpbm_inc):
	compile_args.append(netpbm_inc)
link_args = ['-O0', '-g']
if len(netpbm_lib):
	link_args.append(netpbm_lib)

c_module = Extension('_plotstuff_c',
                     sources = ['plotstuff_wrap.c'],
                     include_dirs = [
						 '../qfits-an/include',
						 '../libkd',
						 '../util', '.'] + get_include_dirs('cairo') +
					 [numpy_inc],
					 extra_objects = [
						 'plotstuff.o', 'plotfill.o', 'plotxy.o',
						 'plotimage.o', 'plotannotations.o',
						 'plotgrid.o', 'plotoutline.o', 'plotindex.o',
						 'plotradec.o', 'plothealpix.o', 'plotmatch.o',
						 'matchfile.o', 'matchobj.o', 'wcs-resample.o',
						 '../util/cairoutils.o',
						 '../util/libanfiles.a',
						 '../libkd/libkd.a',
						 '../util/libanutils.a',
						 '../qfits-an/lib/libqfits.a',
						 '../gsl-an/libgsl-an.a',
						 ],
					 libraries=reduce(lambda x,y: x+y, [get_libs(x) for x in ['cairo', 'wcslib']]) + ['jpeg'], #'netpbm'],
					 library_dirs=reduce(lambda x,y: x+y, [get_lib_dirs(x) for x in ['cairo', 'wcslib']]),
		     extra_compile_args=compile_args,
		     extra_link_args=link_args,
					 )

setup(name = 'Plotting stuff in python',
      version = '1.0',
      description = 'Just what you need, another plotting package!',
      author = 'Astrometry.net (Dustin Lang)',
      author_email = 'dstn@astro.princeton.edu',
      url = 'http://astrometry.net',
      py_modules = [ 'plotstuff' ],
      ext_modules = [c_module])

