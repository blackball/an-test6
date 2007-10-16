# This file is part of the Astrometry.net suite.
# Copyright 2006, 2007 Keir Mierle.
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
import sys

# pwd
# /data/wrk/astrometry/src/tweak/libtweak
# gcc -shared -o _sip.so sip.o ../an-common/starutil.o ../an-common/mathutil.o
# gdb --args `which python` ./sip.py
_sip = ctypes.CDLL('./_sip.so')

from ctypes import *

class Tan(ctypes.Structure):
    _fields_ = [("crval", c_double*2),
                ("crpix", c_double*2),
                ("cd",    c_double*4),
                ("imagew", c_double),
                ("imageh", c_double)]

    def pixelxy2radec(self, px,py):
        'Return ra,dec of px,py'
        ra = ctypes.c_double(3.14159)
        dec = ctypes.c_double(2.71828)
        fpx = ctypes.c_double(px)
        fpy = ctypes.c_double(py)
        _sip.tan_pixelxy2radec(
                ctypes.pointer(self),
                fpx, fpy,
                ctypes.pointer(ra),
                ctypes.pointer(dec))
        return ra.value, dec.value

    def __str__(self):
        return '<Tan: crval=(%g, %g), crpix=(%g, %g), cd=(%g, %g; %g, %g), imagew=%d, imageh=%d>' % \
               (self.crval[0], self.crval[1], self.crpix[0], self.crpix[1], self.cd[0], self.cd[1],
                self.cd[2], self.cd[3], self.imagew, self.imageh)

SIP_MAXORDER = 10

class Sip(ctypes.Structure):
    _fields_ = [('wcstan', Tan),
                ('a_order', c_int),
                ('b_order', c_int),
                ('a', c_double*(SIP_MAXORDER**2)),
                ('b', c_double*(SIP_MAXORDER**2)),
                ('ap_order', c_int),
                ('bp_order', c_int),
                ('ap', c_double*(SIP_MAXORDER**2)),
                ('bp', c_double*(SIP_MAXORDER**2)),]

    def __init__(self, filename=None):
        if not filename is None:
            cfn = c_char_p(filename)
            _sip.sip_read_header_file(cfn, ctypes.pointer(self))

    def __str__(self):
        return '<Sip: ' + str(self.wcstan) + \
               ', a_order=%d, b_order=%d, ap_order=%d>' % (self.a_order, self.b_order, self.ap_order)

    def pixelxy2radec(self, px,py):
        'Return ra,dec of px,py'
        ra = ctypes.c_double(0)
        dec = ctypes.c_double(0)
        fpx = ctypes.c_double(px)
        fpy = ctypes.c_double(py)
        _sip.sip_pixelxy2radec(
                ctypes.pointer(self),
                fpx, fpy,
                ctypes.pointer(ra),
                ctypes.pointer(dec))
        return ra.value, dec.value


_sip.tan_pixelxy2radec
_sip.sip_pixelxy2radec
_sip.tan_read_header_file
_sip.sip_read_header_file

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

    ra,dec = t.pixelxy2radec(2.0,3.0)
    print ra,dec

    s = Sip()
    s.wcstan = t
    s.a_order = 1
    s.b_order = 1
    s.ap_order = 1
    s.bp_order = 1

    ra,dec = s.pixelxy2radec(2.0,3.0)
    print ra,dec

    if len(sys.argv) > 1:
        s = Sip(sys.argv[1])
        print s
        ra,dec = s.pixelxy2radec(2.0,3.0)
        print ra,dec

