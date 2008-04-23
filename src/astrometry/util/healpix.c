/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "healpix.h"
#include "mathutil.h"
#include "starutil.h"
#include "keywords.h"

Const static Inline double mysquare(double d) {
	return d*d;
}

Const int healpix_xy_to_nested(uint hp, uint Nside) {
	uint bighp,x,y;
	int index;
	int i;

	healpix_decompose_xy(hp, &bighp, &x, &y, Nside);
	if (!is_power_of_two(Nside)) {
		fprintf(stderr, "healpix_xy_to_nested: Nside must be a power of two.\n");
		return -1;
	}

	// We construct the index called p_n' in the healpix paper, whose bits
	// are taken from the bits of x and y:
	//    x = ... b4 b2 b0
	//    y = ... b5 b3 b1
	// We go through the bits of x,y, building up "index":
	index = 0;
	for (i=0; i<(8*sizeof(uint)/2); i++) {
		index |= (((y & 1) << 1) | (x & 1)) << (i*2);
		y >>= 1;
		x >>= 1;
		if (!x && !y) break;
	}

	return index + bighp * Nside * Nside;
}

Const int healpix_nested_to_xy(uint hp, uint Nside) {
	uint bighp, x, y;
	int index;
	int i;
	if (!is_power_of_two(Nside)) {
		fprintf(stderr, "healpix_xy_to_nested: Nside must be a power of two.\n");
		return -1;
	}
	bighp = hp / (Nside*Nside);
	index = hp % (Nside*Nside);
	x = y = 0;
	for (i=0; i<(8*sizeof(uint)/2); i++) {
		x |= (index & 0x1) << i;
		index >>= 1;
		y |= (index & 0x1) << i;
		index >>= 1;
		if (!index) break;
	}
	return healpix_compose_xy(bighp, x, y, Nside);
}

Const int healpix_compose_ring(uint ring, uint longind, uint Nside) {
	if (ring <= Nside)
		// north polar
		return ring * (ring-1) * 2 + longind;
	if (ring < 3*Nside)
		// equatorial
		return Nside*(Nside-1)*2 + Nside*4*(ring-Nside) + longind;
	{
		int ri;
		ri = 4*Nside - ring;
		return 12*Nside*Nside-1 - ( ri*(ri-1)*2 + (ri*4 - 1 - longind) );
	}
}

void healpix_decompose_ring(uint hp, uint Nside, uint* p_ring, uint* p_longind) {
	// FIXME: this could be written in closed form...
	int longind;
	int ring;
	int offset = 0;
	for (ring=1; ring<=Nside; ring++) {
		if (offset + ring*4 > hp) {
			longind = hp - offset;
			goto gotit;
		}
		offset += ring*4;
	}
	for (; ring<(3*Nside); ring++) {
		if (offset + Nside*4 > hp) {
			longind = hp - offset;
			goto gotit;
		}
		offset += Nside*4;
	}
	for (; ring<(4*Nside); ring++) {
		if (offset + (Nside*4 - ring)*4 > hp) {
			longind = hp - offset;
			goto gotit;
		}
		offset += (Nside*4 - ring)*4;
	}
	fprintf(stderr, "healpix_decompose_ring: shouldn't get here!\n");
	if (p_ring) *p_ring = (uint)-1;
	if (p_longind) *p_longind = (uint)-1;
	return;
 gotit:
	if (p_ring)
		*p_ring = ring;
	if (p_longind)
		*p_longind = longind;
}

Const int healpix_ring_to_xy(uint ring, uint Nside) {
	int bighp, x, y;
	int ringind, longind;
	healpix_decompose_ring(ring, Nside, (uint*)&ringind, (uint*)&longind);
	if (ringind <= Nside) {
		int ind;
		int v;
		int F1;
		int frow;
		bighp = longind / ringind;
		ind = longind - bighp * ringind;
		y = (Nside - 1 - ind);
		frow = bighp / 4;
		F1 = frow + 2;
		v = F1*Nside - ringind - 1;
		x = v - y;
		return healpix_compose_xy(bighp, x, y, Nside);
	} else if (ringind < 3*Nside) {
		int panel;
		int ind;
		int bottomleft;
		int topleft;
		int frow, F1, F2, s, v, h;
		int bighp = -1;
		int x, y;
		int hp;
		int R = 0;

		panel = longind / Nside;
		ind = longind % Nside;
		bottomleft = ind < (ringind - Nside + 1) / 2;
		topleft = ind < (3*Nside - ringind + 1)/2;

		if (!bottomleft && topleft) {
			// top row.
			bighp = panel;
		} else if (bottomleft && !topleft) {
			// bottom row.
			bighp = 8 + panel;
		} else if (bottomleft && topleft) {
			// left side.
			bighp = 4 + panel;
		} else if (!bottomleft && !topleft) {
			// right side.
			bighp = 4 + (panel + 1) % 4;
			if (bighp == 4) {
				longind -= (4*Nside - 1);
				// Gah!  Wacky hack - it seems that since
				// "longind" is negative in this case, the
				// rounding behaves differently, so we end up
				// computing the wrong "h" and have to correct
				// for it.
				R = 1;
			}
		}

		frow = bighp / 4;
		F1 = frow + 2;
		F2 = 2*(bighp % 4) - (frow % 2) + 1;
		s = (ringind - Nside) % 2;
		v = F1*Nside - ringind - 1;
		h = 2*longind - s - F2*Nside;
		if (R)
			h--;
		x = (v + h) / 2;
		y = (v - h) / 2;
		//fprintf(stderr, "bighp=%i, frow=%i, F1=%i, F2=%i, s=%i, v=%i, h=%i, x=%i, y=%i.\n", bighp, frow, F1, F2, s, v, h, x, y);

		if ((v != (x+y)) || (h != (x-y))) {
			h++;
			x = (v + h) / 2;
			y = (v - h) / 2;
			//fprintf(stderr, "tweak h=%i, x=%i, y=%i\n", h, x, y);

			if ((v != (x+y)) || (h != (x-y))) {
				//fprintf(stderr, "still not right.\n");
			}
		}
		hp = healpix_compose_xy(bighp, x, y, Nside);
		//fprintf(stderr, "hp %i\n", hp);
		return hp;
	} else {
		int ind;
		int v;
		int F1;
		int frow;
		int ri;
		ri = 4*Nside - ringind;
		bighp = 8 + longind / ri;
		ind = longind - (bighp%4) * ri;
		y = (ri-1) - ind;
		frow = bighp / 4;
		F1 = frow + 2;
		v = F1*Nside - ringind - 1;
		x = v - y;
		return healpix_compose_xy(bighp, x, y, Nside);
	}
}

Const int healpix_xy_to_ring(uint hp, uint Nside) {
	uint bighp,x,y;
	int frow;
	int F1;
	int v;
	int ring;
	int index;

	healpix_decompose_xy(hp, &bighp, &x, &y, Nside);
	frow = bighp / 4;
	F1 = frow + 2;
	v = x + y;
	// "ring" starts from 1 at the north pole and goes to 4Nside-1 at
	// the south pole; the pixels in each ring have the same latitude.
	ring = F1*Nside - v - 1;
	/*
	  ring:
	  [1, Nside] : n pole
	  (Nside, 2Nside] : n equatorial
	  (2Nside+1, 3Nside) : s equat
	  [3Nside, 4Nside-1] : s pole
	*/
	// this probably can't happen...
	if ((ring < 1) || (ring >= 4*Nside)) {
		fprintf(stderr, "Invalid ring index: %i\n", ring);
		return -1;
	}
	if (ring <= Nside) {
		// north polar.
		// left-to-right coordinate within this healpix
		index = (Nside - 1 - y);
		// offset from the other big healpixes
		index += ((bighp % 4) * ring);
		// offset from the other rings
		index += ring*(ring-1)*2;
	} else if (ring >= 3*Nside) {
		// south polar.
		// Here I first flip everything so that we label the pixels
		// at zero starting in the southeast corner, increasing to the
		// west and north, then subtract that from the total number of
		// healpixels.
		int ri = 4*Nside - ring;
		// index within this healpix
		index = (ri-1) - x;
		// big healpixes
		index += ((3-(bighp % 4)) * ri);
		// other rings
		index += ri*(ri-1)*2;
		// flip!
		index = 12*Nside*Nside - 1 - index;
	} else {
		// equatorial.
		int s, F2, h;
		s = (ring - Nside) % 2;
		F2 = 2*((int)bighp % 4) - (frow % 2) + 1;
		h = x - y;
		index = (F2 * (int)Nside + h + s) / 2;
		// offset from the north polar region:
		index += Nside*(Nside-1)*2;
		// offset within the equatorial region:
		index += Nside * 4 * (ring - Nside);
		// handle healpix #4 wrap-around
		if ((bighp == 4) && (y > x))
			index += (4 * Nside - 1);
		//fprintf(stderr, "frow=%i, F1=%i, v=%i, ringind=%i, s=%i, F2=%i, h=%i, longind=%i.\n", frow, F1, v, ring, s, F2, h, (F2*(int)Nside+h+s)/2);
	}
	return index;
}

Const double healpix_side_length_arcmin(uint Nside) {
	return sqrt((4.0 * M_PI * mysquare(180.0 * 60.0 / M_PI)) /
				(double)(12 * Nside * Nside));
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

static uint compose_xy(uint x, uint y, uint Nside) {
	return (x * Nside) + y;
}

uint healpix_compose_xy(uint bighp, uint x, uint y, uint Nside) {
	return (bighp * Nside * Nside) + compose_xy(x, y, Nside);
}

void healpix_decompose_xy(uint finehp, uint* pbighp, uint* px, uint* py, uint Nside) {
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

/**
   Given a large-scale healpix number, computes its neighbour in the
   direction (dx,dy).  Returns -1 if there is no such neighbour.
 */
static int healpix_get_neighbour(int hp, int dx, int dy)
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

uint healpix_get_neighbours(uint pix, uint* neighbour, uint Nside)
{
	uint base;
	uint x, y;
	int nn = 0;
	int nbase;
	int Ns2 = Nside * Nside;
	uint nx, ny;

	healpix_decompose_xy(pix, &base, &x, &y, Nside);

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
	neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
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
		neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
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

	neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
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
		neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
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

	neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
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
		neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
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

	neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
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
		neighbour[nn] = nbase * Ns2 + compose_xy(nx, ny, Nside);
		nn++;
	}

	return nn;
}

uint xyztohealpix(double x, double y, double z, uint Nside) {
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

		pnprime = compose_xy(x, y, Nside);

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
		pnprime = compose_xy(x, y, Nside);

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

uint radectohealpix(double ra, double dec, uint Nside) {
	double x, y, z;
	x = radec2x(ra, dec);
	y = radec2y(ra, dec);
	z = radec2z(ra, dec);
	return xyztohealpix(x, y, z, Nside);
}

Const uint radecdegtohealpix(double ra, double dec, uint Nside) {
	return radectohealpix(deg2rad(ra), deg2rad(dec), Nside);
}

uint xyzarrtohealpix(double* xyz, uint Nside) {
	return xyztohealpix(xyz[0], xyz[1], xyz[2], Nside);
}

void healpix_to_xyz(uint hp, uint Nside,
					double dx, double dy, 
					double* rx, double *ry, double *rz)
{
	uint chp;
	uint equatorial = 1;
	double zfactor = 1.0;
	uint xp, yp;
	double x, y, z;
	double pi = M_PI, phi;
	double rad;

	healpix_decompose_xy(hp, &chp, &xp, &yp, Nside);

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

		A = (Nside - y) * (Nside - y);
		B = (Nside - x) * (Nside - x);
		a = (A - B);
		b = -A * pi;
		c = A * pi * pi / 4.0;

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

	rad = sqrt(1.0 - z*z);
	*rx = rad * cos(phi);
	*ry = rad * sin(phi);
	*rz = z;
}

void healpix_to_xyzarr(uint hp, uint Nside,
					   double dx, double dy,
					   double* xyz) {
	healpix_to_xyz(hp, Nside, dx, dy, xyz, xyz+1, xyz+2);
}

void healpix_to_radec(uint hp, uint Nside,
					  double dx, double dy,
					  double* ra, double* dec) {
	double xyz[3];
	healpix_to_xyzarr(hp, Nside, dx, dy, xyz);
	xyzarr2radec(xyz, ra, dec);
}

void healpix_to_radecdeg(uint hp, uint Nside,
                         double dx, double dy,
                         double* ra, double* dec) {
	double xyz[3];
	healpix_to_xyzarr(hp, Nside, dx, dy, xyz);
	xyzarr2radecdeg(xyz, ra, dec);
}

void healpix_to_radecarr(uint hp, uint Nside,
						 double dx, double dy,
						 double* radec) {
	double xyz[3];
	healpix_to_xyzarr(hp, Nside, dx, dy, xyz);
	xyzarr2radec(xyz, radec, radec+1);
}

void healpix_to_radecdegarr(uint hp, uint Nside,
                            double dx, double dy,
                            double* radec) {
	double xyz[3];
	healpix_to_xyzarr(hp, Nside, dx, dy, xyz);
	xyzarr2radecdeg(xyz, radec, radec+1);
}