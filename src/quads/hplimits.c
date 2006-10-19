/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "healpix.h"
#include "starutil.h"

#define OPTIONS "hN:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s [-N <nside>] <hp> [<hp> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int c;
	int Nside = 1;
	int HP;
	int optstart;

    if (argc == 1) {
		print_help(args[0]);
		exit(0);
    }

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'N':
			Nside = atoi(optarg);
			break;
		}
	}
	optstart = optind;


	HP = 12 * Nside * Nside;
	printf("Nside=%i, HP=%i.\n", Nside, HP);

	{
		int* owned;
		int i;
		owned = malloc(HP * sizeof(int));
		for (optind=optstart; optind<argc; optind++) {
			int bighp = atoi(args[optind]);
			memset(owned, 0, HP * sizeof(int));
			// for each big healpix, find the set of small healpixes it owns
			// (including a bit of overlap)
			for (i=0; i<HP; i++) {
				uint big, x, y;
				uint nn, neigh[8], k;
				healpix_decompose(i, &big, &x, &y, Nside);
				if (big != bighp)
					continue;
				owned[i] = 1;
				if (x == 0 || y == 0 || (x == Nside-1) || (y == Nside-1)) {
					// add its neighbours.
					nn = healpix_get_neighbours_nside(i, neigh, Nside);
					for (k=0; k<nn; k++)
						owned[neigh[k]] = 1;
				}
			}
			printf("HP %i owns:\n", bighp);
			for (i=0; i<HP; i++) {
				if (owned[i])
					printf("%03i ", i);
			}
			printf("\n");
		}
		free(owned);
	}
	exit(0);





	{
		//int hp = 674;
		int i;
		int mx = 10;
		printf("radec=[");
		for (i=0; i<=mx; i++) {
			int j;
			for (j=0; j<=mx; j++) {
				double ra, dec;
				double dx, dy;
				double x,y,z;
				dx = (double)i / (double)mx;
				dy = (double)j / (double)mx;
				//healpix_to_xyz(dx, dy, hp, Nside, &x, &y, &z);
				healpix_to_xyz(dx, dy, 8, 1, &x, &y, &z);
				ra = xy2ra(x, y);
				dec = z2dec(z);
				printf("%g,%g;", rad2deg(ra), rad2deg(dec));
				//printf("%g,%g;", z, atan2(y,x));
			}
		}
		printf("];\n");
		//exit(0);
	}

	{
		double x,y,z;
		int hp;
		x = 0.965926;
		y = -0.258819;
		z = -0.222222;
		hp = xyztohealpix_nside(x,y,z,Nside);
		printf("hp=%i\n", hp);

		x = 0.93695;
		y = 0.349464;
		z = -0.502058;
		hp = xyztohealpix_nside(x,y,z,Nside);
		printf("hp=%i\n", hp);


	}
	{
		int i;
		for (i=0; i<HP; i++) {
			double x,y,z;
			int hp;
			healpix_to_xyz(0.5, 0.5, i, Nside, &x, &y, &z);
			hp = xyztohealpix_nside(x, y, z, Nside);
			if (i != hp) {
				printf("HP %i, RA,DEC (%g,%g), x,y,z (%g, %g, %g)\n", i, rad2deg(xy2ra(x,y)), rad2deg(z2dec(z)), x, y, z);
			}
		}
	}

	  {
		int i,j;
		int seen[HP];
		memset(seen, 0, HP*sizeof(int));
		for (i=0; i<=1000; i++) {
			double ra = 2.0 * M_PI * i * 0.001;
			for (j=0; j<=1000; j++) {
				double dec = -0.5*M_PI + M_PI * (j * 0.001);
				int hp = radectohealpix_nside(ra, dec, Nside);
				seen[hp] = 1;
			}
		}
		printf("Didn't see: [ ");
		for (i=0; i<HP; i++) {
			if (!seen[i])
				printf("%i ", i);
		}
		printf("]\n");
		}


	printf("rdc=[");
	for (optind=optstart; optind<argc; optind++) {
		double x,y,z;
		double ra, dec;
		int hp = atoi(args[optind]);
		healpix_to_xyz(0.5, 0.5, hp, Nside, &x, &y, &z);
		ra = xy2ra(x, y);
		dec = z2dec(z);
		printf("%g,%g;", rad2deg(ra), rad2deg(dec));
	}
	printf("];\n");

	printf("radec=[");
	for (optind=optstart; optind<argc; optind++) {
		//double minra, maxra, mindec, maxdec;
		double x,y,z;
		int i;
		int hp = atoi(args[optind]);
		// start from the center...
		/*
		  healpix_to_xyz(0.5, 0.5, hp, Nside, &x, &y, &z);
		  minra = maxra = xy2ra(x, y);
		  mindec = maxdec = z2dec(z);
		  //printf("center: RA,DEC=(%g, %g) degrees\n", rad2deg(minra), rad2deg(mindec));
		  */
		/*
		  for (i=0; i<4; i++) {
		  double ra, dec;
		  // I think the RA,DEC limits of each healpix are at the corners...
		  double dx[4] = {0.0, 0.0, 1.0, 1.0};
		  double dy[4] = {0.0, 1.0, 0.0, 1.0};
		  healpix_to_xyz(dx[i], dy[i], hp, Nside, &x, &y, &z);
		  ra = xy2ra(x, y);
		  dec = z2dec(z);
		  printf("hp corner (%g,%g): RA,DEC=(%g, %g) degrees\n", dx[i], dy[i], rad2deg(ra), rad2deg(dec));
		  }
		*/

		//printf("%g,%g;", rad2deg(minra), rad2deg(mindec));

		//printf("zphi%i=[", hp);
		for (i=0; i<=5; i++) {
			int j;
			for (j=0; j<=5; j++) {
				double ra, dec;
				double dx, dy;
				dx = (double)i / 5.0;
				dy = (double)j / 5.0;
				healpix_to_xyz(dx, dy, hp, Nside, &x, &y, &z);
				ra = xy2ra(x, y);
				dec = z2dec(z);
				printf("%g,%g;", rad2deg(ra), rad2deg(dec));
				//printf("%g,%g;", z, atan2(y,x));
			}
		}
		//printf("];\n");

	}
	printf("];\n");

	return 0;
}
