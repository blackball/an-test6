#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "healpix.h"
#include "mathutil.h"

int healpix_get_neighbours(int hp, int* neighbour, int* xdir, int* ydir);

void swap(uint* i1, uint* i2)
{
	uint tmp;
	tmp = *i1;
	*i1 = *i2;
	*i2 = tmp;
}

void swap_double(double* i1, double* i2) {
	double tmp;
	tmp = *i1;
	*i1 = *i2;
	*i2 = tmp;
}

bool ispolar(int healpix)
{
	// the north polar healpixes are 0,1,2,3
	// the south polar healpixes are 8,9,10,11
	return (healpix <= 3) || (healpix >= 8);
}

bool isequatorial(int healpix)
{
	// the north polar healpixes are 0,1,2,3
	// the south polar healpixes are 8,9,10,11
	return (healpix >= 4) && (healpix <= 7);
}

bool isnorthpolar(int healpix)
{
	return (healpix <= 3);
}

bool issouthpolar(int healpix)
{
	return (healpix >= 8);
}

bool ispowerof4(uint x) {
	int bit, nbits=0, firstbit=-1;
	for (bit=0; bit<32; bit++) {
		if ((x >> bit) & 1) {
			nbits++;
			firstbit = bit;
		}
		if (nbits > 1)
			return FALSE;
	}
	return (firstbit % 2) == 0;
}

void pnprime_to_xy(uint pnprime, uint* px, uint* py, uint Nside)
{
	uint xbitmask, ybitmask;
	uint x, y;
	int bit;

	if (!ispowerof4(Nside)) {
		*px = pnprime / Nside;
		*py = pnprime % Nside;
		return;
	}

	assert(px);
	assert(py);

	x = y = 0;
	// oh, those wacky physicists...
	xbitmask = 1;
	ybitmask = 2;
	for (bit = 0; bit < 16; bit++) {
		x |= ( (pnprime & xbitmask) ? (1 << bit) : 0 );
		y |= ( (pnprime & ybitmask) ? (1 << bit) : 0 );
		xbitmask = xbitmask << 2;
		ybitmask = ybitmask << 2;
	}

	*px = x;
	*py = y;
}

uint xy_to_pnprime(uint x, uint y, uint Nside)
{
	uint pnprime = 0;
	uint mask;
	int bit;

	if (!ispowerof4(Nside)) 
		return x*Nside + y;

	mask = 1;
	for (bit = 0; bit < 16; bit++) {
		pnprime |=
		    ((x & mask) << bit) |
		    ((y & mask) << (bit + 1));
		mask = mask << 1;
	}
	return pnprime;
}

void healpix_decompose(uint finehp, uint* pbighp, uint* px, uint* py, uint Nside) {
	uint bighp;
	uint pnprime;
	bighp   = finehp / (Nside * Nside);
	pnprime = finehp % (Nside * Nside);
	pnprime_to_xy(pnprime, px, py, Nside);
	if (pbighp)
		*pbighp = bighp;
}

int healpix_get_neighbour(int hp, int dx, int dy)
{
	if (isnorthpolar(hp)) {
		if ((dx ==  1) && (dy ==  0)) return (hp + 1) % 4;
		if ((dx ==  0) && (dy ==  1)) return (hp + 3) % 4;
		if ((dx ==  1) && (dy ==  1)) return (hp + 2) % 4;
		if ((dx == -1) && (dy ==  0)) return (hp + 4);
		if ((dx ==  0) && (dy == -1)) return 4 + ((hp + 1) % 4);
		if ((dx == -1) && (dy == -1)) return hp + 8;
		return -1;
	} else if (issouthpolar(hp)) {
		if ((dx ==  1) && (dy ==  0)) return 4 + ((hp + 1) % 4);
		if ((dx ==  0) && (dy ==  1)) return hp - 4;
		if ((dx == -1) && (dy ==  0)) return 8 + ((hp + 3) % 4);
		if ((dx ==  0) && (dy == -1)) return 8 + ((hp + 1) % 4);
		if ((dx == -1) && (dy == -1)) return 8 + ((hp + 2) % 4);
		if ((dx ==  1) && (dy ==  1)) return hp - 8;
		return -1;
	} else {
		if ((dx ==  1) && (dy ==  0)) return hp - 4;
		if ((dx ==  0) && (dy ==  1)) return (hp + 3) % 4;
		if ((dx == -1) && (dy ==  0)) return 8 + ((hp + 3) % 4);
		if ((dx ==  0) && (dy == -1)) return hp + 4;
		if ((dx ==  1) && (dy == -1)) return 4 + ((hp + 1) % 4);
		if ((dx == -1) && (dy ==  1)) return 4 + ((hp - 1) % 4);
		return -1;
	}
	return -1;
}

uint healpix_get_neighbours_nside(uint pix, uint* neighbour, uint Nside)
{
	uint base;
	uint x, y;
	int nn = 0;
	int nbase;
	int Ns2 = Nside * Nside;
	uint nx, ny;

	healpix_decompose(pix, &base, &x, &y, Nside);

	// ( + , 0 )
	nx = (x + 1) % Nside;
	ny = y;
	if (x == (Nside - 1)) {
		nbase = healpix_get_neighbour(base, 1, 0);
		if (isnorthpolar(base)) {
			nx = x;
			swap(&nx, &ny);
		}
	} else
		nbase = base;

	//printf("(+ 0): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));
	neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
	nn++;


	// ( + , + )
	nx = (x + 1) % Nside;
	ny = (y + 1) % Nside;
	if ((x == Nside - 1) && (y == Nside - 1)) {
		if (ispolar(base))
			nbase = healpix_get_neighbour(base, 1, 1);
		else
			nbase = -1;
	} else if (x == (Nside - 1))
		nbase = healpix_get_neighbour(base, 1, 0);
	else if (y == (Nside - 1))
		nbase = healpix_get_neighbour(base, 0, 1);
	else
		nbase = base;

	if (isnorthpolar(base)) {
		if (x == (Nside - 1))
			nx = Nside - 1;
		if (y == (Nside - 1))
			ny = Nside - 1;
		if ((x == (Nside - 1)) || (y == (Nside - 1)))
			swap(&nx, &ny);
	}

	//printf("(+ +): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));

	if (nbase != -1) {
		neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
		nn++;
	}



	// ( 0 , + )
	nx = x;
	ny = (y + 1) % Nside;
	if (y == (Nside - 1)) {
		nbase = healpix_get_neighbour(base, 0, 1);
		if (isnorthpolar(base)) {
			ny = y;
			swap(&nx, &ny);
		}
	} else
		nbase = base;

	//printf("(0 +): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));

	neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
	nn++;



	// ( - , + )
	nx = (x + Nside - 1) % Nside;
	ny = (y + 1) % Nside;
	if ((x == 0) && (y == (Nside - 1))) {
		if (isequatorial(base))
			nbase = healpix_get_neighbour(base, -1, 1);
		else
			nbase = -1;
	} else if (x == 0) {
		nbase = healpix_get_neighbour(base, -1, 0);
		if (issouthpolar(base)) {
			nx = 0;
			swap(&nx, &ny);
		}
	} else if (y == (Nside - 1)) {
		nbase = healpix_get_neighbour(base, 0, 1);
		if (isnorthpolar(base)) {
			ny = y;
			swap(&nx, &ny);
		}
	} else
		nbase = base;

	//printf("(- +): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));

	if (nbase != -1) {
		neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
		nn++;
	}


	// ( - , 0 )
	nx = (x + Nside - 1) % Nside;
	ny = y;
	if (x == 0) {
		nbase = healpix_get_neighbour(base, -1, 0);
		if (issouthpolar(base)) {
			nx = 0;
			swap(&nx, &ny);
		}
	} else
		nbase = base;

	//printf("(- 0): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));

	neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
	nn++;



	// ( - , - )
	nx = (x + Nside - 1) % Nside;
	ny = (y + Nside - 1) % Nside;
	if ((x == 0) && (y == 0)) {
		if (ispolar(base))
			nbase = healpix_get_neighbour(base, -1, -1);
		else
			nbase = -1;
	} else if (x == 0)
		nbase = healpix_get_neighbour(base, -1, 0);
	else if (y == 0)
		nbase = healpix_get_neighbour(base, 0, -1);
	else
		nbase = base;

	if (issouthpolar(base)) {
		if (x == 0)
			nx = 0;
		if (y == 0)
			ny = 0;
		if ((x == 0) || (y == 0))
			swap(&nx, &ny);
	}

	//printf("(- -): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));

	if (nbase != -1) {
		neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
		nn++;
	}


	// ( 0 , - )
	ny = (y + Nside - 1) % Nside;
	nx = x;
	if (y == 0) {
		nbase = healpix_get_neighbour(base, 0, -1);
		if (issouthpolar(base)) {
			ny = y;
			swap(&nx, &ny);
		}
	} else
		nbase = base;

	//printf("(0 -): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));

	neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
	nn++;


	// ( + , - )
	nx = (x + 1) % Nside;
	ny = (y + Nside - 1) % Nside;
	if ((x == (Nside - 1)) && (y == 0)) {
		if (isequatorial(base)) {
			nbase = healpix_get_neighbour(base, 1, -1);
		} else
			nbase = -1;

	} else if (x == (Nside - 1)) {
		nbase = healpix_get_neighbour(base, 1, 0);
		if (isnorthpolar(base)) {
			nx = x;
			swap(&nx, &ny);
		}
	} else if (y == 0) {
		nbase = healpix_get_neighbour(base, 0, -1);
		if (issouthpolar(base)) {
			ny = y;
			swap(&nx, &ny);
		}
	} else
		nbase = base;

	//printf("(+ -): nbase=%i, nx=%i, ny=%i, pix=%i\n", nbase, nx, ny, nbase*Ns2+xy_to_pnprime(nx,ny,Nside));

	if (nbase != -1) {
		neighbour[nn] = nbase * Ns2 + xy_to_pnprime(nx, ny, Nside);
		nn++;
	}

	return nn;
}

/*
  "x" points in the northeast direction
  "y" points in the northwest direction
 */
int healpix_get_neighbours(int hp, int* neighbour, int* xdir, int* ydir)
{
	int nn = 0;
	if (isnorthpolar(hp)) {

		// NE
		neighbour[nn] = (hp + 1) % 4;
		xdir[nn] = 1;
		ydir[nn] = 0;
		nn++;

		/*
		  ;
		  // E = NE
		  neighbour[nn] = (hp + 1) % 4;
		  xdir[nn] = 1;
		  ydir[nn] = -1;
		  nn++;
		*/

		// NW
		neighbour[nn] = (hp + 3) % 4;
		xdir[nn] = 0;
		ydir[nn] = 1;
		nn++;

		/*
		  ;
		  // W = NW
		  neighbour[nn] = (hp + 3) % 4;
		  xdir[nn] = -1;
		  ydir[nn] = 1;
		  nn++;
		*/

		// N
		neighbour[nn] = (hp + 2) % 4;
		xdir[nn] = 1;
		ydir[nn] = 1;
		nn++;

		// SW
		neighbour[nn] = (hp + 4);
		xdir[nn] = -1;
		ydir[nn] = 0;
		nn++;

		// SE
		neighbour[nn] = 4 + ((hp + 1) % 4);
		xdir[nn] = 0;
		ydir[nn] = -1;
		nn++;

		// S
		neighbour[nn] = hp + 8;
		xdir[nn] = -1;
		ydir[nn] = -1;
		nn++;

	} else if (issouthpolar(hp)) {

		/*
		  ;
		  // E
		  neighbour[nn] = 8 + ((hp + 1) % 4);
		  xdir[nn] = 1;
		  ydir[nn] = -1;
		  nn++;
		*/

		// SE
		neighbour[nn] = 8 + ((hp + 1) % 4);
		xdir[nn] = 0;
		ydir[nn] = -1;
		nn++;

		/*
		  ;
		  // W
		  neighbour[nn] = 8 + ((hp + 3) % 4);
		  xdir[nn] = -1;
		  ydir[nn] = 1;
		  nn++;
		*/

		// SW
		neighbour[nn] = 8 + ((hp + 3) % 4);
		xdir[nn] = -1;
		ydir[nn] = 0;
		nn++;

		// S
		neighbour[nn] = 8 + ((hp + 2) % 4);
		xdir[nn] = -1;
		ydir[nn] = -1;
		nn++;

		// NW
		neighbour[nn] = (hp - 4);
		xdir[nn] = 0;
		ydir[nn] = 1;
		nn++;

		// NE
		neighbour[nn] = 4 + ((hp + 1) % 4);
		xdir[nn] = 1;
		ydir[nn] = 0;
		nn++;

		// N
		neighbour[nn] = hp - 8;
		xdir[nn] = 1;
		ydir[nn] = 1;
		nn++;

	} else {
		// equatorial.

		// E
		neighbour[nn] = 4 + ((hp + 1) % 4);
		xdir[nn] = 1;
		ydir[nn] = -1;
		nn++;

		// W
		neighbour[nn] = 4 + ((hp - 1) % 4);
		xdir[nn] = -1;
		ydir[nn] = 1;
		nn++;

		// NE
		neighbour[nn] = hp - 4;
		xdir[nn] = 1;
		ydir[nn] = 0;
		nn++;

		// NW
		neighbour[nn] = (hp + 3) % 4;
		xdir[nn] = 0;
		ydir[nn] = 1;
		nn++;

		// SE
		neighbour[nn] = hp + 4;
		xdir[nn] = 0;
		ydir[nn] = -1;
		nn++;

		// SW
		neighbour[nn] = 8 + ((hp + 3) % 4);
		xdir[nn] = -1;
		ydir[nn] = 0;
		nn++;
	}
	return nn;
}

int healpix_nested_to_ring_index(int nested,
                                 int* p_ring, int* p_longitude,
                                 uint Nside)
{
	int f;
	uint pnprime;
	int frow, F1, F2;
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

	pnprime_to_xy(pnprime, &x, &y, Nside);

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

uint xyztohealpix_nside(double x, double y, double z, uint Nside)
{
	double phi;
	double phioverpi;
	double twothirds = 2.0 / 3.0;
	double twopi = 2.0 * M_PI;
	double pi = M_PI;

	/* Convert our point into cylindrical coordinates for middle ring */
	phi = atan2(y, x);
	if (phi < 0.0)
		phi += twopi;

	phioverpi = phi / pi;

	// North or south pole
	if ((z >= twothirds) || (z <= -twothirds)) {
		double zfactor;
		bool north;
		double phit;
		uint x, y;
		uint pnprime;
		int column;
		int basehp;
		int hp;
		double root;

		// Which pole?
		if (z >= twothirds) {
			north = TRUE;
			zfactor = 1.0;
		} else {
			north = FALSE;
			zfactor = -1.0;
		}

		phit = fmod(phi, pi / 2.0);
		assert (phit >= 0.00);

		root = (1.0 - z*zfactor) * 3.0 * square(Nside * (2.0 * phit - pi) / pi);
		if (root <= 0.0)
			x = 1;
		else
			x = (int)ceil(sqrt(root));

		assert(x >= 1);
		assert(x <= Nside);

		root = (1.0 - z*zfactor) * 3.0 * square(Nside * 2.0 * phit / pi);
		if (root <= 0.0)
			y = 1;
		else
			y = (int)ceil(sqrt(root));

		assert(y >= 1);
		assert(y <= Nside);

		x = Nside - x;
		y = Nside - y;

		if (!north)
			swap(&x, &y);

		pnprime = xy_to_pnprime(x, y, Nside);

		if (!north)
			pnprime = Nside * Nside - 1 - pnprime;

		column = (int)(phi / (pi / 2.0));

		if (north)
			basehp = column;
		else
			basehp = 8 + column;

		hp = basehp * (Nside * Nside) + pnprime;

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
		int hp;

		phim = fmod(phi, pi / 2.0);

		// project into the unit square z=[-2/3, 2/3], phi=[0, pi/2]
		zunits = (z + twothirds) / (4.0 / 3.0);
		phiunits = phim / (pi / 2.0);
		u1 = (zunits + phiunits) / 2.0;
		u2 = (zunits - phiunits) / 2.0;
		// x is the northeast direction, y is the northwest.
		x = (int)floor(u1 * 2.0 * Nside);
		y = (int)floor(u2 * 2.0 * Nside);
		x %= Nside;
		y %= Nside;
		if (x < 0)
			x += Nside;
		if (y < 0)
			y += Nside;
		pnprime = xy_to_pnprime(x, y, Nside);

		// now compute which big healpix it's in.
		offset = (int)(phioverpi * 2.0);
		phimod = phioverpi - offset * 0.5;
		offset = ((offset % 4) + 4) % 4;
		z1 = twothirds - (8.0 / 3.0) * phimod;
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
			basehp = ((offset + 1) % 4) + 4;
		}

		hp = basehp * Nside * Nside + pnprime;

		return hp;
	}
	return -1;
}

uint radectohealpix_nside(double ra, double dec, uint Nside)
{
	double x, y, z;
	x = radec2x(ra, dec);
	y = radec2y(ra, dec);
	z = radec2z(ra, dec);
	return xyztohealpix_nside(x, y, z, Nside);
}

/*
  We should be able to do some simple trig to convert.
  Just go via xyz for now.
*/
int radectohealpix(double ra, double dec)
{
	double x, y, z;
	x = radec2x(ra, dec);
	y = radec2y(ra, dec);
	z = radec2z(ra, dec);
	return xyztohealpix(x, y, z);
}

int xyztohealpix(double x, double y, double z)
{
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

		z1 = twothirds - (8.0 / 3.0) * phimod;
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
			return ((offset + 1) % 4) + 4;
		}

		// the polar pixel center is at (z,phi/pi) = (2/3, 1/4)
		// the left pixel center is at (z,phi/pi) = (0, 0)
		// the right pixel center is at (z,phi/pi) = (0, 1/2)
	}
}

void healpix_to_xyz(double dx, double dy, uint hp, uint Nside,
                    double* rx, double *ry, double *rz)
{
	uint chp = hp / Nside;
	uint pnp = hp % Nside;
	uint equatorial = 1;
	uint zfactor = 1;
	uint xp, yp;
	double x, y, z;
	double pi = PIl, phi;

	pnprime_to_xy(pnp, &xp, &yp, Nside);

	if (isnorthpolar(chp) &&
		  ( ((xp+yp+1)/Nside > 1) ||
		    ((xp+yp+1)/Nside == 1 && dx+dy >= 1.0))) {
		equatorial = 0;
		zfactor = 1;
	}
	if (issouthpolar(chp) &&
		  ( ((xp+yp+1)/Nside < 1) ||
		    ((xp+yp+1)/Nside == 1 && dx+dy <= 1.0))) {
		equatorial = 0;
		zfactor = -1;
	}

	x = xp+dx;
	y = yp+dy;

	if (equatorial) {
		double zoff=0;
		double phioff=0;
		x /= Nside;
		y /= Nside;

		if (chp <= 3) {
			phioff = pi/4.0;
		} else if (chp <= 7) {
			zoff = -1.0;
			chp -= 4;
		} else if (chp <= 11) {
			phioff = 1.0;
			zoff = -2.0;
			chp -= 8;
		} else {
			// should never get here
			assert(0);
		}

		z = 2.0/3.0*(x + y + zoff);
		phi = pi/4*(y - x + phioff + 2*chp);


	} else {
		// do other magic

		// get z/phi using magical equations
		double phiP, phiN, phit, t;
		if (zfactor == -1)
			swap_double(&x, &y);

		// We solve two equations in two unknows to get z,phit from
		// x,y. They are of the form z = f(x,phit) and z = f(y,phit).
		// The solution to phit is a quadratic so we take the one that
		// falls in the right region.
		phiP = (-pi+pi*x/y)/2.0/(x*x/y/y-1);
		phiN = (-pi-pi*x/y)/2.0/(x*x/y/y-1);
		if (0 <= phiP && phiP <= pi/2) {
			phit = phiP;
			assert(0 > phiN || phiN > pi/2);
		} else {
			phit = phiN;
			assert(0 > phiP || phiP > pi/2);
		}

		// Now that we have phit we can get z
		t = pi*y/2/phit/Nside;
		z = 1-t*t/3;

		// Need to get phi
		if (issouthpolar(chp))
			phi = pi/2*(chp-8) + phit;
		else
			phi = pi/2*chp + phit;

	}

	if (phi < 0.0)
		phi += 2*pi;

	*rx = cos(phi);
	*ry = sin(phi);
	*rz = z;
}
