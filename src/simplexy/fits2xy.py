"""
NAME:
      fits2xy

PURPOSE:
      Extract sources from a FITS file

INPUTS:
      Takes a single FITS file as input

OPTIONAL INPUTS:

KEYWORD PARAMETERS:

OUTPUTS:

OPTIONAL OUTPUTS:

COMMENTS:

MODIFICATION HISTORY:
       K. Mierle, 2007-Jan - Initial version based on fits2xy.c
"""

# You need ctypes and a recent (1.0) numpy for this to work. I've included
# pyfits so you don't have to. 

import pyfits
import sys
import scipy
from simplexy import simplexy

#f = pyfits.open('thisfails.fits')
f = pyfits.open('test.fits')
x,y,flux,sigma = simplexy(f[0].data)
#x,y,flux,sigma = simplexy(scipy.randn(100,100))


"""
def extract(infile):
    f = pyfits.open(infile)
    x,y,flux,sigma = simplexy(f[0].data, dpsf=1.0, plim=8.0, dlim=1.0,
                              saddle=3.0, maxper=1000, maxnpeaks=100000):

    return x,y,flux,sigma

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print "Usage: fits2xy.py image.fits"
    else:
        x,y,flux,sigma = extract(sys.argv[1])
        """
