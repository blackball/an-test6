#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "healpix.h"
#include "starutil.h"

#define OPTIONS "hd"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s [-d] ra dec\n"
		   "     (-d means values are in degree; by default they're in radians)\n"
		   "  %s x y z\n\n"
		   "If your values are negative, add \"--\" in between \"-d\" (if you're using it) and \"ra\" or \"x\".\n\n",
		   progname, progname);
}

int main(int argc, char** args) {
    int healpix;
    double argvals[argc];
    int i;
    bool degrees = FALSE;
    int nargs;
    int c;

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
	case 'd':
	    degrees = TRUE;
	    break;
	}
    }

    nargs = argc - optind;
    if (!((nargs == 2) || (nargs == 3))) {
	print_help(args[0]);
	exit(-1);
    }

    for (i=0; i<nargs; i++) {
	argvals[i] = atof(args[optind + i]);
    }

    if (nargs == 2) {
	double ra = argvals[0];
	double dec = argvals[1];

	if (degrees) {
	    ra  *= M_PI / 180.0;
	    dec *= M_PI / 180.0;
	}

	healpix = radectohealpix(ra, dec);
	
	printf("(RA, DEC) = (%g, %g) degrees\n", ra*180/M_PI, dec*180/M_PI);
	printf("(RA, DEC) = (%g, %g) radians\n", ra, dec);
	printf("Healpix=%i\n", healpix);

    } else if (nargs == 3) {
	double x = argvals[0];
	double y = argvals[1];
	double z = argvals[2];
	double r;

	r = 

	healpix = xyztohealpix(x, y, z);
	
	printf("(x, y, z) = (%g, %g, %g)\n", x, y, z);
	printf("Healpix=%i\n", healpix);
    }

    return 0;
}
