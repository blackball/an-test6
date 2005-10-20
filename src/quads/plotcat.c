#include "fileutil.h"
#include "math.h"
#define N 3000
#define OPTIONS "bf:"
char* help = "usage: plotcat [-b] -f catalog.objs\n";
unsigned int projection[N][N];
extern char *optarg;
extern int optind, opterr, optopt;
int main(int argc, char *argv[])
{
	char ASCII = READ_FAIL;
	int ii, jj, reverse=0;
	sidx numstars;
	dimension Dim_Stars;
	double ramin, ramax, decmin, decmax;
	double x,y,z,X,Y;
	FILE* fid = NULL;
	int argidx, argchar; //  opterr = 0;

	if (argc <= 2) {
		fprintf(stderr, help);
		return 1;
	} 

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'b':
			reverse = 1;
			break;
		case 'f':
			fid = fopen(optarg,"r");
			break;
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		return (OPT_ERR);
	}

	if (!fid) {
		fprintf(stderr, "ERROR: Failed to open catalog\n");
		return 1;
	}

	ASCII = read_objs_header(fid, &numstars, &Dim_Stars, &ramin, &ramax, &decmin, &decmax);
	if (ASCII == (char)READ_FAIL) {
		fprintf(stderr, "ERROR: Failed to read catalog\n");
		return 1;
	}

	if (Dim_Stars != 3) {
		fprintf(stderr, "ERROR: Catalog has dimension != 3\n");
		return 1;
	}

	for (ii = 0; ii < N; ii++)
		for (jj = 0; jj < N; jj++)
			projection[ii][jj] = 0;

	for (ii = 0; ii < numstars; ii++) {
		if (ASCII) {
			fscanf(fid, "%lf,%lf,%lf\n", &x, &y, &z);
		} else {
			fread(&x, sizeof(double), 1, fid);
			fread(&y, sizeof(double), 1, fid);
			fread(&z, sizeof(double), 1, fid);
		}
		if ((z <= 0 && !reverse) || (z >= 0 && reverse)) 
			continue;

		if (reverse)
			z = -z;

		X = x*sqrt(1./(1. + z));
		Y = y*sqrt(1./(1. + z));
		unsigned int xind, yind;
		xind = (unsigned int) floor(N*0.5*(1+0.9*X));
		yind = (unsigned int) floor(N*0.5*(1+0.9*Y));
		if (xind > N || yind > N) {
			fprintf(stderr, "BAIL: xind=%d, yind=%d\n", xind, yind);
			exit(1);
		}
		projection[xind][yind] += 1;
	}

	// Output PGM format
	printf("P5 %d %d %d\n", N, N, 255);
	for (ii = 0; ii < N; ii++) {
		for (jj = 0; jj < N; jj++) {
			if (projection[ii][jj] < 255)
				putchar(projection[ii][jj]);
				
			else
				putchar(255);
		}
	}
	return 0;
}
