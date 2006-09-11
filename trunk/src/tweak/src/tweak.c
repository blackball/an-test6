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
#include "fitsio.h"

typedef struct WorldCoor wcs_t;

// hacky. pull the complete hdu from the current hdu, then use wcstools to
// figure out the wcs
wcs_t* get_wcs_from_hdu(fitsfile* infptr)
{
	int mystatus;
	int* status = &mystatus;
	int nkeys, ii;

	if (ffghsp(infptr, &nkeys, NULL, status) > 0) {
		fprintf(stderr, "nomem\n");
		return NULL;
	}
	fprintf(stderr, "nkeys=%d\n",nkeys);

	/* create a memory buffer to hold the header records */
	int tmpbufflen = nkeys*(FLEN_CARD-1)*sizeof(char)+1;
	char* tmpbuff = malloc(tmpbufflen);
	if (!tmpbuff) {
		fprintf(stderr, "nomem\n");
		return NULL;
	}

	/* read all of the header records in the input HDU */
	for (ii = 0; ii < nkeys; ii++) {
		char* thiscard = tmpbuff + (ii * (FLEN_CARD-1));
		ffgrec(infptr, ii+1, thiscard, status);

		// Stupid hack because ffgrec null terminates
		int n = strlen(thiscard);
		if (n!=80) {
			int jj;
			for(jj=n;jj<80;jj++) 
				thiscard[jj] = ' ';
		}
	}
	//tmpbuff[tmpbufflen] = '\0';
	//fprintf(stderr, "%s\n",tmpbuff);
	return wcsninit(tmpbuff, tmpbufflen);
}

int get_xy(char* filename, double **x, double **y, int *n)
{
	return 0;
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
			dist2 += (xyz_mean[j]-xyz[3*i+j])*(xyz_mean[j]-xyz[3*i+j]);
		if (maxdist2 < dist2) {
			maxdist2 = dist2;
			maxind = i;
		}
	}
	return 0;
}

int get_reference_stars(double ra_mean, double dec_mean, double radius,
                        double** ra, double **dec, int *n)
{
	return 0;
}

/* Fink-Hogg shift */
/* This one isn't going to be fun. */

/* spherematch */

int main(int argc, char *argv[])
{
	wcs_t* wcs;
	/* Are there multiple images? */
	fitsfile *fptr;         /* FITS file pointer, defined in fitsio.h */
	//fitsfile *ofptr;        /* FITS file pointer to output file */
	int status = 0; // FIXME should have ostatus too
	int naxis;
	//long naxisn[2];
	int kk;

	if (argc != 2) {
		fprintf(stderr, "Usage: tweak filename.fits \n");
		fprintf(stderr, "Assumes filename.xy.fits already exists\n");
		return (0);
	}

	if (fits_open_file(&fptr, argv[1], READONLY, &status)) {
		fprintf(stderr, "Error reading file %s\n", argv[1]);
		fits_report_error(stderr, status);
		exit(-1);
	}

	// Are there multiple HDU's?
	int nhdus;
	fits_get_num_hdus(fptr, &nhdus, &status);
	fprintf(stderr, "nhdus=%d\n", nhdus);

	int hdutype;

	// Tweak each HDU independently
	for (kk=1; kk <= nhdus; kk++) {
		fits_movabs_hdu(fptr, kk, &hdutype, &status);
		fits_get_hdu_type(fptr, &hdutype, &status);

		if (hdutype != IMAGE_HDU) 
			continue;

		fits_get_img_dim(fptr, &naxis, &status);
		if (status) {
			fits_report_error(stderr, status);
			exit( -1);
		}

		if (naxis != 2) {
			fprintf(stderr, "Invalid image: NAXIS is not 2 in HDU %d!\n", kk);
			continue;
		}

		// At this point, we have an image. Now get the WCS data
		wcs = get_wcs_from_hdu(fptr);
		fprintf(stderr, "GOT WCS INFO!!! %p \n", wcs);

	}


	if (status == END_OF_FILE)
		status = 0; /* Reset after normal error */

	fits_close_file(fptr, &status);
	//fits_close_file(ofptr, &status);


	if (status)
		fits_report_error(stderr, status); /* print any error message */
	return (status);
	

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
