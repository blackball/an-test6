#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

	HP = 12 * Nside * Nside;
	printf("Nside=%i, HP=%i.\n", Nside, HP);

	for (; optind<argc; optind++) {
		double minra, maxra, mindec, maxdec;
		double x,y,z;
		// I think the RA,DEC limits of each healpix are at the corners...
		/*
		  double dx[4] = {0.0, 0.0, 1.0, 1.0};
		  double dy[4] = {0.0, 1.0, 1.0, 0.0};
		*/
		int i;
		int j;

		int hp = atoi(args[optind]);
		/*
		  ;
		  // start from the center...
		  healpix_to_xyz(0.5, 0.5, hp, Nside, &x, &y, &z);
		  minra = maxra = xy2ra(x, y);
		  mindec = maxdec = z2dec(z);
		  printf("center: z=%g, phi/pi=%g, RA,DEC=(%g, %g) degrees\n", z, atan2(y,x)/M_PI, rad2deg(minra), rad2deg(mindec));
		  for (i=0; i<4; i++) {
		  double ra, dec;
		  healpix_to_xyz(dx[i], dy[i], hp, Nside, &x, &y, &z);
		  ra = xy2ra(x, y);
		  dec = z2dec(z);
		  printf("hp corner (%g,%g): z=%g, phi/pi=%g, RA,DEC=(%g, %g) degrees\n", dx[i], dy[i], z, atan2(y,x)/M_PI, rad2deg(ra), rad2deg(dec));
		  }
		*/
		printf("zphi%i=[", hp);
		for (i=0; i<=5; i++) {
			for (j=0; j<=5; j++) {
				double ra, dec;
				double dx, dy;

				dx = (double)i / 5.0;
				dy = (double)j / 5.0;
				healpix_to_xyz(dx, dy, hp, Nside, &x, &y, &z);
				ra = xy2ra(x, y);
				dec = z2dec(z);
				printf("%g,%g;", z, atan2(y,x));
			}
		}
		printf("];\n");


	}
	return 0;
}
