#include <string.h>
#include <stdio.h>
#include "fitsio.h"
#include "dimage.h"
#include <assert.h>

#define MAXNPEAKS 100000

static float *x = NULL;
static float *y = NULL;
static float *flux = NULL;

int main(int argc, char *argv[])
{
	fitsfile *fptr;         /* FITS file pointer, defined in fitsio.h */
	fitsfile *ofptr;        /* FITS file pointer to output file */
	//char card[FLEN_CARD];   /* Standard string lengths defined in fitsio.h */
	int status = 0; // FIXME should have ostatus too
	int naxis;
	int maxnpeaks = MAXNPEAKS, npeaks;
	long naxisn[2];
	long fpixel[2] = {1L, 1L};
	int kk, jj;
	float *thedata = NULL;
	float sigma;

	if (argc != 2) {
		fprintf(stderr, "Usage: fits2xy fitsname.fits \n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Read a FITS file, find objects, and write out \n");
		fprintf(stderr, "X, Y, FLUX to stdout. \n");
		fprintf(stderr, "\n");
		fprintf(stderr, "   fits2xy 'file.fits[0]'   - list primary array header \n");
		fprintf(stderr, "   fits2xy 'file.fits[2]'   - list header of 2nd extension \n");
		fprintf(stderr, "   fits2xy file.fits+2    - same as above \n");
		fprintf(stderr, "\n");
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

	int hdu;
	fits_get_hdu_num(fptr, &hdu);
	fprintf(stderr, "hdu=%d\n", hdu);

	// Create xylist filename (by trimming '.fits')
	int fnamelen = strlen(argv[1]);
	char outfile[300];
	assert(argv[1][fnamelen] == '\0');
	assert(fnamelen > 5);
	sprintf(outfile, "%s", argv[1]);
	outfile[fnamelen-5] = '\0';
	sprintf(outfile, "%s.xy.fits",outfile);
	fprintf(stderr, "outfile=%s\n",outfile);
	
	
	// Create output file
	if (fits_create_file(&ofptr, outfile, &status)) {
		fits_report_error(stderr, status);
		exit( -1);
	}
	fits_create_img(ofptr, 8, 0, NULL, &status);
	assert(!status);

	fits_write_key(ofptr, TSTRING, "SRCFN", outfile, "Source image", &status);
	/* Parameters for simplexy; save for debugging */
	fits_write_comment(ofptr, "Parameters used for source extraction", &status);
	float dpsf = 1;     /* gaussian psf width; 1 is usually fine */
	float plim = 8;     /* significance to keep; 8 is usually fine */
	float dlim = 1;     /* closest two peaks can be; 1 is usually fine */
	float saddle = 3;   /* saddle difference (in sig); 3 is usually fine */
	int maxper = 1000;  /* maximum number of peaks per object; 1000 */
	fits_write_key(ofptr, TFLOAT, "DPSF", &dpsf, "Gaussian psf width", &status);
	fits_write_key(ofptr, TFLOAT, "PLIM", &plim, "Significance to keep", &status);
	fits_write_key(ofptr, TFLOAT, "DLIM", &dlim, "Closest two peaks can be", &status);
	fits_write_key(ofptr, TFLOAT, "SADDLE", &saddle, "Saddle difference (in sig)", &status);
	fits_write_key(ofptr, TINT, "MAXPER", &maxper, "Max num of peaks per object", &status);
	fits_write_key(ofptr, TINT, "MAXPEAKS", &maxnpeaks, "Max num of peaks total", &status);

	fits_write_history(ofptr, 
		"Created by astrometry.net's simplexy v1.0.4rc2 alpha-3+4",
		&status);
	assert(!status);
	fits_write_history(ofptr, 
		"Visit us on the web at http://astrometry.net/",
		&status);
	assert(!status);
	assert(!status);

	int hdutype;
	int nimgs = 0;

	// Run simplexy on each HDU
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

		fits_get_img_size(fptr, 2, naxisn, &status);
		if (status) {
			fits_report_error(stderr, status);
			exit( -1);
		}

		nimgs++;

		fprintf(stderr,"Got naxis=%d,na1=%lu,na2=%lu\n", naxis,naxisn[0],naxisn[1]);

		thedata = malloc(naxisn[0] * naxisn[1] * sizeof(float));
		if (thedata == NULL) {
			fprintf(stderr, "Failed allocating data array.\n");
			exit( -1);
		}

		// FIXME #199 Check and make sure this is float data
		fits_read_pix(fptr, TFLOAT, fpixel, naxisn[0]*naxisn[1], NULL, thedata,
			      NULL, &status);

		x = malloc(maxnpeaks * sizeof(float));
		y = malloc(maxnpeaks * sizeof(float));
		if (x == NULL || y == NULL) {
			fprintf(stderr, "Failed allocating output arrays.\n");
			exit( -1);
		}
		flux = malloc(maxnpeaks * sizeof(float));
		simplexy( thedata, naxisn[0], naxisn[1],
				dpsf, plim, dlim, saddle, maxper, maxnpeaks,
				&sigma, x, y, flux, &npeaks);

		// The FITS standard specifies that the center of the lower
		// left pixel is 1,1. Store our xylist according to FITS
		for (jj=0; jj<npeaks; jj++) {
			x[jj] += 1.0;
			y[jj] += 1.0;
		}

		char* ttype[] = {"X","Y","FLUX"};
		char* tform[] = {"E","E","E"};
		char* tunit[] = {"pixels","pixels","flux"};
		fits_create_tbl(ofptr, BINARY_TBL, npeaks, 3, ttype,tform,
				tunit, "SOURCES", &status);
		fits_write_col(ofptr, TFLOAT, 1, 1, 1, npeaks, x, &status);
		fits_report_error(stderr, status);
		assert(!status);
		fits_write_col(ofptr, TFLOAT, 2, 1, 1, npeaks, y, &status);
		fits_report_error(stderr, status);
		assert(!status);
		fits_write_col(ofptr, TFLOAT, 3, 1, 1, npeaks, flux, &status);
		fits_report_error(stderr, status);
		assert(!status);

		fits_modify_comment(ofptr, "TTYPE1", "X coordinate", &status);
		assert(!status);
		fits_modify_comment(ofptr, "TTYPE2", "Y coordinate", &status);
		assert(!status);
		fits_modify_comment(ofptr, "TTYPE3", "Flux of source", &status);
		assert(!status);
		fits_write_key(ofptr, TINT, "SRCEXT", &kk, "Extension number in src image", &status);
		assert(!status);
		fits_write_key(ofptr, TFLOAT, "ESTSIGMA", &sigma, "Estimated source image variance", &status);
		fits_write_comment(ofptr,
			"The X and Y points are specified assuming 1,1 is "
			"the center of the leftmost bottom pixel of the "
			"image in accordance with the FITS standard.", &status);
		assert(!status);


		fprintf(stderr, "sigma=%f\n", sigma);
		free(thedata);
		free(x);
		free(y);
	}

	// Put in the optional NEXTEND keywoard
	fits_movabs_hdu(ofptr, 1, &hdutype, &status);
	assert(hdutype == IMAGE_HDU);
	fits_write_key(ofptr, TINT, "NEXTEND", &nimgs, "Number of extensions", &status);
	assert(!status);

	if (status == END_OF_FILE)
		status = 0; /* Reset after normal error */

	fits_close_file(fptr, &status);
	fits_close_file(ofptr, &status);


	if (status)
		fits_report_error(stderr, status); /* print any error message */
	return (status);
}
