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
