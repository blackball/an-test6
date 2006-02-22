#include <math.h>

#include "healpix.h"
#include "starutil.h"

int healpix_nested_to_ring_index(int nested,
								 int* p_ring, int* p_longitude,
								 int Nside) {
	int f, pnprime;
	int frow, F1, F2;
	uint xbitmask, ybitmask;
	int bit;
	uint x, y;
	uint v, h;
	int ringind, longind;
	int s;

	// "nested" is called p_n in the Healpix paper.

	// (this is right - we want floor)
	f = nested / (Nside * Nside);
	pnprime = nested % (Nside * Nside);

	if ((f < 0) || (f >= 12)) {
		fprintf(stderr, "Invalid nested index %i\n", nested);
		return -1;
	}

	x = y = 0;
	// oh, those wacky physicists...
	xbitmask = 1;
	ybitmask = 2;
	for (bit=0; bit<32; bit+=2) {
		x |= (pnprime & xbitmask);
		y |= (pnprime & ybitmask);
		xbitmask = xbitmask << 2;
		ybitmask = ybitmask << 2;
	}

	v = x + y;
	h = x - y;

	frow = f / 4;

	F1 = frow + 2;

	ringind = F1 * Nside - v - 1;

	F2 = 2 * (f % 4) - (frow % 2) + 1;

	/*
	  [1, Nside) : n pole
	  [Nside, 2Nside] : n equatorial
	  (2Nside+1, 3Nside] : s equat
	  (3Nside, 4Nside-1] : s pole
	*/

	if ((ringind < 1) || (ringind >= 4*Nside)) {
		fprintf(stderr, "Invalid ring index: %i\n", ringind);
		return -1;
	}

	if ((ringind < Nside) || (ringind > 3*Nside)) {
		// polar.
		s = 1;
	} else {
		s = (ringind - Nside + 1) % 2;
	}

	longind = (F2 * Nside + h + s) / 2;

	if (p_ring)
		*p_ring = ringind;
	if (p_longitude)
		*p_longitude = longind;
	return 0;
}

bool ispolar(int healpix) {
	// the north polar healpixes are 0,1,2,3
	// the south polar healpixes are 8,9,10,11
	return (healpix <= 3) || (healpix >= 8);
}

int xyztohealpix_nside(double x, double y, double z, int Nside) {
    double phi;
    double phioverpi;
    double twothirds = 2.0 / 3.0;

    phi = atan2(y, x);
    if (phi < 0.0)
		phi += 2.0 * M_PI;

    phioverpi = phi / M_PI;

    if ((z >= twothirds) || (z <= -twothirds)) {
		// definitely in the polar regions.

		if (z >= twothirds) {
			// north polar
			double zupper, zlower;
			double phit;
			int k;

			phit = fmod(phi, M_PI/2.0);

			for (k=0; k<Nside; k++) {
				zupper = 1.0 - (double)(k*k) / (double)(3*Nside*Nside) *
					square(M_PI / (2.0 * phit));

				zlower = 1.0 - (double)(k*k) / (double)(3*Nside*Nside) *
					square(M_PI / (2.0 * phit - M_PI));

				printf("k=%i, zlower=%g, zupper=%g\n", k, zlower, zupper);
			}

		} else {
			// south polar

		}

    } else {
		// could be polar or equatorial.
		double phimod;
		int offset;
		double z1, z2;

		offset = (int)(phioverpi * 2.0);
		phimod = phioverpi - offset * 0.5;

		z1 =  twothirds - (8.0 / 3.0) * phimod;
		z2 = -twothirds + (8.0 / 3.0) * phimod;

		if ((z >= z1) && (z >= z2)) {
			// north polar

		} else if ((z <= z1) && (z <= z2)) {
			// south polar

		} else if (phimod < 0.25) {
			// left equatorial

		} else {
			// right equatorial

		}

		// the polar pixel center is at (z,phi/pi) = (2/3, 1/4)
		// the left pixel center is at (z,phi/pi) = (0, 0)
		// the right pixel center is at (z,phi/pi) = (0, 1/2)
    }
	return -1;
}

int radectohealpix_nside(double ra, double dec, int nside) {
    double x, y, z;
    x = radec2x(ra, dec);
    y = radec2y(ra, dec);
    z = radec2z(ra, dec);
	return xyztohealpix_nside(x, y, z, nside);
}

/*
  We should be able to do some simple trig to convert.
  Just go via xyz for now.
*/
int radectohealpix(double ra, double dec) {
    double x, y, z;
    x = radec2x(ra, dec);
    y = radec2y(ra, dec);
    z = radec2z(ra, dec);
    return xyztohealpix(x, y, z);
}

int xyztohealpix(double x, double y, double z) {
    double phi;
    double phioverpi;
    double twothirds = 2.0 / 3.0;

    phi = atan2(y, x);
    if (phi < 0.0)
		phi += 2.0 * M_PI;

    phioverpi = phi / M_PI;

    if ((z >= twothirds) || (z <= -twothirds)) {
		// definitely in the polar regions.
		int offset;
		int pix;

		// the north polar healpixes are 0,1,2,3
		// the south polar healpixes are 8,9,10,11
		if (z >= twothirds)
			offset = 0;
		else
			offset = 8;

		pix = (int)(phioverpi * 2.0);

		return offset + pix;

    } else {
		// could be polar or equatorial.
		double phimod;
		int offset;
		double z1, z2;

		offset = (int)(phioverpi * 2.0);
		phimod = phioverpi - offset * 0.5;

		z1 =  twothirds - (8.0 / 3.0) * phimod;
		z2 = -twothirds + (8.0 / 3.0) * phimod;

		if ((z >= z1) && (z >= z2)) {
			// north polar
			return offset;
		} else if ((z <= z1) && (z <= z2)) {
			// south polar
			return offset + 8;
		} else if (phimod < 0.25) {
			// left equatorial
			return offset + 4;
		} else {
			// right equatorial
			return ((offset+1)%4) + 4;
		}

		// the polar pixel center is at (z,phi/pi) = (2/3, 1/4)
		// the left pixel center is at (z,phi/pi) = (0, 0)
		// the right pixel center is at (z,phi/pi) = (0, 1/2)
    }
}
