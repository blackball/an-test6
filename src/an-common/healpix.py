from math import pi, sqrt, log

class healpix(object):

    @staticmethod
    # radius of the field, in degrees.
    def get_closest_pow2_nside(radius):
        # 4 pi steradians on the sky, into 12 base-level healpixes
        area = 4. * pi / 12.
        # in radians:
        baselen = sqrt(area)
        n = baselen / (radians(radius*2.))
        p = int(round(log(n, 2.0)))
        return 2**p


