#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

/**
   This program gets called by "quad.php" in response to a client requesting a list
   of the polygons that are visible in a particular region of the map.

   The RA coordinates are passed as    -x <lower-RA> -X <upper-RA>
   The DEC coordinates are             -y <lower-DEC> -Y <upper-DEC>
   The width and height in pixels are  -w <width> -h <height>
   The maximum number of quads is      -m <maxquads>
*/

#define OPTIONS "x:y:X:Y:w:h:m:"

#define FALSE 0
#define TRUE  1

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	int gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	int maxquads = 0;
	int i;
	int steps = 20;

	double racenter, deccenter, rad;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'm':
			maxquads = atoi(optarg);
			break;
		case 'x':
			x0 = atof(optarg);
			gotx = TRUE;
			break;
		case 'X':
			x1 = atof(optarg);
			goty = TRUE;
			break;
		case 'y':
			y0 = atof(optarg);
			gotX = TRUE;
			break;
		case 'Y':
			y1 = atof(optarg);
			gotY = TRUE;
			break;
		case 'w':
			w = atoi(optarg);
			gotw = TRUE;
			break;
		case 'h':
			h = atoi(optarg);
			goth = TRUE;
			break;
		}

	if (!(gotx && goty && gotX && gotY && gotw && goth)) {
		fprintf(stderr, "Invalid inputs: need ");
		if (!gotx) fprintf(stderr, "-x ");
		if (!gotX) fprintf(stderr, "-X ");
		if (!goty) fprintf(stderr, "-y ");
		if (!gotY) fprintf(stderr, "-Y ");
		if (!gotw) fprintf(stderr, "-w ");
		if (!goth) fprintf(stderr, "-h ");
		exit(-1);
	}

	fprintf(stderr, "X range: [%g, %g] degrees\n", x0, x1);
	fprintf(stderr, "Y range: [%g, %g] degrees\n", y0, y1);

	// Let's just draw a circle in RA,DEC space... it will be a weird shape in
	// Mercator projection.
	racenter = (x0 + x1) / 2.0;
	deccenter = (y0 + y1) / 2.0;
	rad = hypot(x1 - x0, y1 - y0) / 6.0;

	printf("<polygons>\n");
	printf("  <poly");
	for (i=0; i<=steps; i++) {
		double angle = 2.0 * M_PI * (double)i / (double)steps;
		printf(" ra%i=\"%g\" dec%i=\"%g\"",
			   i, racenter + rad * sin(angle),
			   i, deccenter + rad * cos(angle));
	}
	printf("/>\n");
	printf("</polygons>\n");

	return 0;
}
