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
	F2 = 2 * (f % 4) - (frow % 2) + 1;
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
		double zfactor;
		bool north;
		double zupper, zlower;
		double phit;
		uint x,y;
		uint pnprime;
		int bit;
		uint mask;
		int column;
		int basehp;
		int hp;

		if (z >= twothirds) {
			// north polar
			north = TRUE;
			zfactor = 1.0;
		} else {
			north = FALSE;
			zfactor = -1.0;
		}

		phit = fmod(phi, M_PI/2.0);
		if (phit < 0.0)
			phit += M_PI/2.0;

		// could do this in closed form...
		for (x=1; x<=Nside; x++) {
			zlower = 1.0 - (double)(x*x) / (double)(3*Nside*Nside) *
				square(M_PI / (2.0 * phit - M_PI));
			if (z*zfactor >= zlower)
				break;
		}
		for (y=1; y<=Nside; y++) {
			zupper = 1.0 - (double)(y*y) / (double)(3*Nside*Nside) *
				square(M_PI / (2.0 * phit));
			if (z*zfactor >= zupper)
				break;
		}

		x = Nside - x;
		y = Nside - y;

		if (!north) {
			uint tmp = x;
			x = y;
			y = tmp;
		}

		pnprime = 0;
		mask = 1;
		for (bit=0; bit<16; bit++) {
			pnprime |=
				((x & mask) << bit) |
				((y & mask) << (bit + 1));
			mask = mask << 1;
		}

		if (!north)
			pnprime = Nside*Nside - 1 - pnprime;

		column = (int)(phi / (M_PI/2.0));

		if (north)
			basehp = column;
		else
			basehp = 8 + column;

		hp = basehp * (Nside*Nside) + pnprime;

		return hp;

    } else {
		// could be polar or equatorial.
		double phimod;
		int offset;
		double z1, z2;
		int basehp;
		double phim;
		double u1, u2;
		double zunits, phiunits;
		int x, y;
		uint pnprime;
		int bit;
		uint mask;
		int hp;

		phim = fmod(phi, M_PI/2.0);
		if (phim < 0.0)
			phim += M_PI/2.0;

		// project into the unit square z=[-2/3, 2/3], phi=[0, pi/2]
		zunits = (z + twothirds) / (4.0/3.0);
		phiunits = phim / (M_PI/2.0);
		u1 = (zunits + phiunits) / 2.0;
		u2 = (zunits - phiunits) / 2.0;
		// x is the northeast direction, y is the northwest.
		x = (int)floor(u1 * 2.0 * Nside);
		y = (int)floor(u2 * 2.0 * Nside);
		x %= Nside;
		y %= Nside;
		if (x < 0) x += Nside;
		if (y < 0) y += Nside;
		pnprime = 0;
		mask = 1;
		for (bit=0; bit<16; bit++) {
			pnprime |=
				((x & mask) << bit) |
				((y & mask) << (bit + 1));
			mask = mask << 1;
		}

		// now compute which big healpix it's in.
		offset = (int)(phioverpi * 2.0);
		phimod = phioverpi - offset * 0.5;
		z1 =  twothirds - (8.0 / 3.0) * phimod;
		z2 = -twothirds + (8.0 / 3.0) * phimod;
		if ((z >= z1) && (z >= z2)) {
			// north polar
			basehp = offset;
		} else if ((z <= z1) && (z <= z2)) {
			// south polar
			basehp = 8 + offset;
		} else if (phimod < 0.25) {
			// left equatorial
			basehp = offset + 4;
		} else {
			// right equatorial
			basehp = ((offset+1)%4) + 4;
		}

		hp = basehp * Nside * Nside + pnprime;

		return hp;
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
