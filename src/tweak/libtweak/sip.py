# This file is part of the Astrometry.net suite.
# Copyright 2006-2007, Keir Mierle.
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

import ctypes
from ctypes import c_int, c_double

# pwd
# /data/wrk/astrometry/src/tweak/libtweak
# gcc -shared -o _sip.so sip.o ../an-common/starutil.o ../an-common/mathutil.o
# gdb --args `which python` ./sip.py
_sip = ctypes.CDLL('./_sip.so')

from ctypes import *

class Tan(ctypes.Structure):
    _fields_ = [("crval", c_double*2),
                ("crpix", c_double*2),
                ("cd",    c_double*4)]

    def pixelxy2radec(self, px,py):
        'Return ra,dec of px,py'
        ra = ctypes.c_double(3.14159)
        dec = ctypes.c_double(2.71828)
        print ra, dec
        _sip.tan_pixelxy2radec(
                ctypes.pointer(self),
                px, py,
                ctypes.pointer(ra),
                ctypes.pointer(dec))

#        return ra.value, dec.value

_sip.tan_pixelxy2radec

#tan_t* tan, double px, double py, double *xyz);
#tan_pixelxy2radec(tan_t* wcs_tan, double px, double py, double *ra, double *dec);
#tan_radec2pixelxy(tan_t* wcs_tan, double ra, double dec, double *px, double *py);

if __name__ == '__main__':
    t= Tan()
    t.crval[0] = 0.0
    t.crval[1] = 0.0
    t.crpix[0] = 0.0
    t.crpix[1] = 0.0
    t.cd[0] = 1.0
    t.cd[1] = 0.0
    t.cd[2] = 0.0
    t.cd[3] = 1.0

    ra,dec = t.pixelxy2radec(2,3)
