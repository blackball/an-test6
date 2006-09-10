#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include "libwcs/fitsfile.h"
#include "libwcs/wcs.h"
#include "starutil.h"

int get_xy(char* filename, double **x, double **y, int *n)
{

}

int get_center_and_radius(double* ra, double* dec, int n,
                          double* ra_mean, double* dec_mean, double* radius)
{
	double* xyz = malloc(3*n*sizeof(double));
	double xyz_mean[3] = {0,0,0};
	int i, j;
	for (i=0; i<n; i++) 
		radec2xyzarr(ra[i],dec[i],xyz+3*i);

	for (i=0; i<n; i++) 
		for (j=0; j<3; j++) 
			xyz_mean[j] += xyz[3*i+j];

	for (j=0; j<3; j++) 
		xyz_mean[j] /= (double) n;

	double maxdist2 = 1e300;
	int maxind = -1;
	for (i=0; i<n; i++) {
		double dist2 = 0;
		for (j=0; j<3; j++) 
			dist2 = += (xyz_mean[j]-xyz[3*i+j])*(xyz_mean[j]-xyz[3*i+j]);
		if (maxdist2 < dist2) {
			maxdist2 = dist2;
			maxind = i;
		}
	}
}

int get_reference_stars(double ra_mean, double dec_mean, double radius,
                        double** ra, double **dec, int *n)
{

}

/* Fink-Hogg shift */
/* This one isn't going to be fun. */

/* spherematch */

int main(int argc, char *argv[])
{
	struct WorldCoor* wcs;

	wcs = GetWCSFITS(argv[1], 0)

	/* Get original WCS from image */
	/* Get xy positions of stars in image */
	/* Find field center and radius */
	/* Get standard stars from USNO or whatever */
	/* Fink-Hogg shift */
	/* Fink-Hogg shift */
	/* Correspondeces via spherematch -- switch to dualtree */
	/* Linear tweak */
	/* Non-linear tweak */

}
