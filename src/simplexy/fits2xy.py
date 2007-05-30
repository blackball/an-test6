# This file is part of the Astrometry.net suite.
# Copyright 2007 Keir Mierle and David W. Hogg.
#
# The Astrometry.net suite is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, version 2.
#
# The Astrometry.net suite is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Astrometry.net suite ; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

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
       Hogg, 2007-May - simplexy options change
"""

# You need ctypes and a recent (1.0) numpy for this to work. I've included
# pyfits so you don't have to. 

import pyfits
import sys
import scipy
import os
import time
from simplexy import simplexy

# Default settings
dpsf = 1.0
plim = 8.0
dlim = dpsf
saddle = 5.0
maxper = 1000
maxnpeaks = 10000
maxsize = 1000
halfbox= 100

def extract(infile):
    fitsfile = pyfits.open(infile)
    outfile = pyfits.HDUList()

    # Make empty HDU; no image
    outfile.append(pyfits.PrimaryHDU()) 

    outfile[0].header.update('srcfn', infile, 'Source image')
    outfile[0].header.add_comment('Parameters used in source extraction')
    outfile[0].header.update('dpsf', dpsf, 'Gaussian psf width')
    outfile[0].header.update('plim', plim, 'Significance to keep')
    outfile[0].header.update('dlim', dlim, 'Closest two peaks can be')
    outfile[0].header.update('saddle', saddle, 'Saddle in difference (in sig)')
    outfile[0].header.update('maxper', maxper, 'Max num of peaks per object')
    outfile[0].header.update('maxpeaks', maxnpeaks, 'Max num of peaks total')
    outfile[0].header.update('maxsize', maxsize, 'Max size of extended objects')
    outfile[0].header.update('halfbox', halfbox, 'Half-size of sliding sky window')
    outfile[0].header.add_comment('Extracted by fits2xy.py')
    outfile[0].header.add_comment('on %s %s' % (time.ctime(), time.tzname[0]))

    for i, hdu in enumerate(fitsfile):
        if (i == 0 and hdu.data != None) or isinstance(hdu, pyfits.ImageHDU):
            print hdu.data.shape+(i,)
            if i == 0:
                print 'Image: Primary HDU (number 0) %sx%s' % hdu.data.shape
            else:
                print 'Image: Extension HDU (number %s) %sx%s' % tuple((i,)+hdu.data.shape)

            x,y,flux,sigma = simplexy(hdu.data, dpsf=dpsf, plim=plim,
                                      dlim=dlim, saddle=saddle, maxper=maxper,
                                      maxnpeaks=maxnpeaks, maxsize=maxsize,
                                      halfbox=halfbox)

            cx = pyfits.Column(name='X', format='E', array=x, unit='pix')
            cy = pyfits.Column(name='Y', format='E', array=y, unit='pix')
            cflux = pyfits.Column(name='FLUX', format='E', array=flux)
            tbhdu = pyfits.new_table([cx, cy, cflux])
            tbhdu.header.update('srcext', i, 'Extension number in src image')
            tbhdu.header.update('estsigma', sigma, 'Estimated source image variance')
            tbhdu.header.add_comment(
                    'The X and Y points are specified assuming 1,1 is '
                    'the center of the leftmost bottom pixel of the '
                    'image in accordance with the FITS standard.')
            cards = tbhdu.header.ascardlist()
            cards['TTYPE1'].comment = 'X coordinate'
            cards['TTYPE2'].comment = 'Y coordinate'
            cards['TTYPE3'].comment = 'Flux of source'

            outfile.append(tbhdu)

    newfile = infile.replace('.fits','.xy.fits')
    try:
        outfile.writeto(newfile)
    except IOError:
        # File probably exists
        print 'File %s appears to already exist; deleting!' % newfile
        import os
        os.unlink(newfile)
        outfile.writeto(newfile)

    return x,y,flux,sigma

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print "Usage: fits2xy.py image.fits"
    else:
        x,y,flux,sigma = extract(sys.argv[1])
