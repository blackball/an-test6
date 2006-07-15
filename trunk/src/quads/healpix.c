#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "healpix.h"
#include "mathutil.h"

int healpix_get_neighbours(int hp, int* neighbour, int* xdir, int* ydir);

Const static Inline double mysquare(double d) {
	return d*d;
}

static Inline void swap(uint* i1, uint* i2) {
	uint tmp;
	tmp = *i1;
	*i1 = *i2;
	*i2 = tmp;
}

static Inline void swap_double(double* i1, double* i2) {
	double tmp;
	tmp = *i1;
	*i1 = *i2;
	*i2 = tmp;
}

static Inline bool ispolar(int healpix)
{
	// the north polar healpixes are 0,1,2,3
	// the south polar healpixes are 8,9,10,11
	return (healpix <= 3) || (healpix >= 8);
}

static Inline bool isequatorial(int healpix)
{
	// the north polar healpixes are 0,1,2,3
	// the south polar healpixes are 8,9,10,11
	return (healpix >= 4) && (healpix <= 7);
}

static Inline bool isnorthpolar(int healpix)
{
	return (healpix <= 3);
}

static Inline bool issouthpolar(int healpix)
{
	return (healpix >= 8);
}

static Inline bool ispowerof4(uint x) {
	if (x >= 0x40000)
		return (                    x == 0x40000   ||
				 x == 0x100000   || x == 0x400000  ||
				 x == 0x1000000  || x == 0x4000000 ||
				 x == 0x10000000 || x == 0x40000000);
	else 
		return (x == 0x1        || x == 0x4       || 
				x == 0x10       || x == 0x40      ||
				x == 0x100      || x == 0x400     ||
				x == 0x1000     || x == 0x4000    ||
				x == 0x10000);
}

static void pnprime_to_xy(uint pnprime, uint* px, uint* py, uint Nside)
{
	uint xbitmask, ybitmask;
	uint x, y;
	int bit;

	if (!ispowerof4(Nside)) {
		if (px) *px = pnprime / Nside;
		if (py) *py = pnprime % (int)Nside;
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

	if (px) *px = x;
	if (py) *py = y;
}

static uint xy_to_pnprime(uint x, uint y, uint Nside)
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

uint healpix_compose(uint bighp, uint x, uint y, uint Nside) {
	uint pnprime = xy_to_pnprime(x, y, Nside);
	return bighp * Nside*Nside + pnprime;
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

uint healpix_compose_lex(uint bighp, uint x, uint y, uint Nside) {
	return (bighp * Nside * Nside) + (x * Nside) + y;
}

void healpix_decompose_lex(uint finehp, uint* pbighp, uint* px, uint* py, uint Nside) {
	uint hp;
	if (pbighp) {
		uint bighp   = finehp / (Nside * Nside);
		*pbighp = bighp;
	}
	hp = finehp % (Nside * Nside);
	if (px)
		*px = hp / Nside;
	if (py)
		*py = hp % Nside;
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

		root = (1.0 - z*zfactor) * 3.0 * mysquare(Nside * (2.0 * phit - pi) / pi);
		if (root <= 0.0)
			x = 1;
		else
			x = (int)ceil(sqrt(root));

		assert(x >= 1);
		assert(x <= Nside);

		root = (1.0 - z*zfactor) * 3.0 * mysquare(Nside * 2.0 * phit / pi);
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
		x %= (int)Nside;
		y %= (int)Nside;

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

void healpix_to_xyz_common(double dx, double dy, uint hp, uint Nside,
						   double* rx, double *ry, double *rz, int lex)
{
	uint chp;
	uint equatorial = 1;
	double zfactor = 1.0;
	uint xp, yp;
	double x, y, z;
	double pi = PIl, phi;
	
	if (lex)
		healpix_decompose_lex(hp, &chp, &xp, &yp, Nside);
	else
		healpix_decompose(hp, &chp, &xp, &yp, Nside);

	// this is x,y position in the healpix reference frame
	x = xp+dx;
	y = yp+dy;

	if (isnorthpolar(chp)) {
		if ((x + y) > Nside) {
			equatorial = 0;
			zfactor = 1.0;
		}
	}
	if (issouthpolar(chp)) {
		if ((x + y) < Nside) {
			equatorial = 0;
			zfactor = -1.0;
		}
	}

	if (equatorial) {
		double zoff=0;
		double phioff=0;
		x /= Nside;
		y /= Nside;

		if (chp <= 3) {
			phioff = 1.0;
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
		phi = pi/4*(x - y + phioff + 2*chp);

	} else {
		// do other magic

		// get z/phi using magical equations
		double phiP, phiN, phit;
		double A, B, a, b, c;

		if (zfactor == -1.0) {
			swap_double(&x, &y);
			x = (Nside - x);
			y = (Nside - y);
		}

		A = mysquare(Nside - y);
		B = mysquare(Nside - x);
		a = (A - B);
		b = -A * pi;
		c = A * mysquare(pi / 2.0);

		if (a == 0.0) {
			phit = pi / 4.0;
		} else {
			double disc = b*b - 4.0*a*c;
			if (disc <= 0.0) {
				phit = -b / (2.0 * a);
				// with Nside=200, hp 349000, and -O2 with gcc 4.0.1, this
				// assert fails (marginally).
				//assert(0.0 <= phit && phit <= pi/2.0);
				if (phit < 0.0) phit = 0.0;
				if (phit > pi/2.0) phit = pi/2.0;
			} else {
				phiP = (-b + sqrt(disc)) / (2.0*a);
				phiN = (-b - sqrt(disc)) / (2.0*a);
				if (0.0 <= phiP && phiP <= pi/2.0) {
					phit = phiP;
				} else {
					phit = phiN;
					assert(0.0 <= phiN && phiN <= pi/2.0);
				}
			}
		}

		if (phit == 0.0) {
			z = 1.0 - mysquare(pi * (Nside - x) / ((2.0 * phit - pi) * Nside)) / 3.0;
		} else {
			z = 1.0 - mysquare(pi * (Nside - y) / (2.0 * phit * Nside)) / 3.0;
		}
		assert(0.0 <= fabs(z) && fabs(z) <= 1.0);
		z *= zfactor;
		assert(0.0 <= fabs(z) && fabs(z) <= 1.0);

		// Need to get phi
		if (issouthpolar(chp))
			phi = pi/2.0* (chp-8) + phit;
		else
			phi = pi/2.0 * chp + phit;

	}

	if (phi < 0.0)
		phi += 2*pi;

	*rx = cos(phi);
	*ry = sin(phi);
	*rz = z;
}

void healpix_to_xyz_lex(double dx, double dy, uint hp, uint Nside,
						double* p_x, double *p_y, double *p_z) {
	healpix_to_xyz_common(dx, dy, hp, Nside, p_x, p_y, p_z, 1);
}

void healpix_to_xyz(double dx, double dy, uint hp, uint Nside,
                    double* p_x, double *p_y, double *p_z) {
	healpix_to_xyz_common(dx, dy, hp, Nside, p_x, p_y, p_z, 0);
}


