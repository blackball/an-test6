"""
NAME:
      simplexy

PURPOSE:
      Extract sources from an image

INPUTS:
      image     - The actual image. This will be converted to a 32-bit floating
                  point number, regardless of the input so make sure you are
                  aware of this!
      dpsf      - gaussian psf width; the default 1 is usually fine 
      plim      - significance to keep; the default 8 is usually fine 
      dlim      - closest two peaks can be; 1 is usually fine (default)
      saddle    - saddle difference (in sig); 3 is usually fine (default)
      maxper    - maximum number of peaks per object; 1000
      maxnpeaks - maximum number of peaks total; 100000

OPTIONAL INPUTS:
      None yet

KEYWORD PARAMETERS:

OUTPUTS:
      Returns a tuple (x, y, flux, sigma) where
          x,y   - detected sources
          flux  - estimated flux for each source
          sigma - noise estimated in the image

COMMENTS:
   - This is a ctypes wrapping of the simplexy C code originally from Blanton,
     which uses parts of Numerical Recipes in C.

BUGS:
   - The 

MODIFICATION HISTORY:
    K. Mierle, 2007-Jan-23 - Original version
"""

import ctypes
import numpy
# Use the wonderful ctypeslib
from ctypes import byref, c_float, c_int, POINTER
from numpy.ctypeslib import ndpointer

OutArrayPtr = ndpointer(dtype=numpy.float32, ndim=1, flags='CONTIGUOUS')
InArrayPtr = ndpointer(dtype=numpy.float32, ndim=2, flags='CONTIGUOUS')

simplexy_dll = ctypes.CDLL('./simplexy_module.so')
simplexy_fn = simplexy_dll.simplexy
simplexy_fn.restype = ctypes.c_int
simplexy_fn.argtypes = [InArrayPtr,           # image
                        c_int,         # nx
                        c_int,         # ny
                        c_float,       # dpsf
                        c_float,       # plim
                        c_float,       # dlim
                        c_float,       # saddle
                        c_int,         # max peaks per object
                        c_int,         # max peaks total
                        POINTER(ctypes.c_float),     # sigma  OUT
                        OutArrayPtr,          # x      OUT
                        OutArrayPtr,          # y      OUT
                        OutArrayPtr,          # flux   OUT
                        POINTER(ctypes.c_int),       # npeaks OUT
                       ]

def simplexy(image, dpsf=1.0, plim=8.0, dlim=1.0, saddle=3.0, maxper=1000,
             maxnpeaks=100000):

    x = numpy.zeros(maxpeaks, dtype=numpy.float32)
    y = numpy.zeros(maxpeaks, dtype=numpy.float32)
    sigma = ctypes.c_float()
    flux = numpy.zeros(maxpeaks, dtype=numpy.float32)
    npeaks = ctypes.c_int()

    success = simplexy_fn(byref(image.astype(numpy.float64)),
                          image.shape[1],
                          image.shape[0],
                          dpsf, plim, dlim, saddle, maxper, maxpeaks,
                          byref(sigma),
                          byref(x),
                          byref(y),
                          byref(flux),
                          byref(npeaks))

    assert success # bail on no success for now

    return x, y, float(flux), float(sigma)

simplexy.__doc__ = __doc__
