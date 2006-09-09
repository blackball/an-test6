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
	//unsigned long int bscale = 1L, bzero = 0L, kk, jj;
	int kk;
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

	// Run simplexy on each HDU
	for (kk=1; kk <= nhdus; kk++) {
		int hdutype;
		fits_movabs_hdu(fptr, kk, &hdutype, &status);
		fits_get_hdu_type(fptr, &hdutype, &status);

		// Create dummy HDU for tables
		if (hdutype != IMAGE_HDU) {
			fprintf(stderr, "dummy hdr\n");
			fits_create_tbl(ofptr, hdutype, 0, 0, NULL,NULL, NULL,
					"dummy", &status);
			continue;
		}

		fits_get_img_dim(fptr, &naxis, &status);
		if (status) {
			fits_report_error(stderr, status);
			exit( -1);
		}

		if (naxis != 2) {
			// FIXME perhaps just continue?
			fprintf(stderr, "Invalid header: NAXIS is not 2 on hdu%d!\n", kk);
			fits_create_img(ofptr, 8, 0, NULL, &status);
			assert(!status);
			//exit( -1);
			continue;
		}
		fits_get_img_size(fptr, 2, naxisn, &status);
		if (status) {
			fits_report_error(stderr, status);
			exit( -1);
		}

		//fprintf(stderr,"Got naxis=%d,na1=%d,na2=%d,bitpix=%d\n", naxis,naxisn[0],naxisn[1],bitpix);
		fprintf(stderr,"Got naxis=%d,na1=%lu,na2=%lu\n", naxis,naxisn[0],naxisn[1]);

		thedata = (float *)malloc(naxisn[0] * naxisn[1] * sizeof(float));
		if (thedata == NULL) {
			fprintf(stderr, "Failed allocating data array.\n");
			exit( -1);
		}

		// FIXME Check and make sure this is float data
		fits_read_pix(fptr, TFLOAT, fpixel, naxisn[0]*naxisn[1], NULL, thedata,
			      NULL, &status);

		x = (float *) malloc(maxnpeaks * sizeof(float));
		y = (float *) malloc(maxnpeaks * sizeof(float));
		if (x == NULL || y == NULL) {
			fprintf(stderr, "Failed allocating output arrays.\n");
			exit( -1);
		}
		flux = (float *) malloc(maxnpeaks * sizeof(float));
		simplexy( thedata, naxisn[0], naxisn[1], 1., 8., 1., 3., 1000,
			  maxnpeaks, &sigma, x, y, flux, &npeaks);

		char* ttype[] = {"X","Y","FLUX"};
		char* tform[] = {"E","E","E"};
		char* tunit[] = {"PIXEL","PIXEL","FLUX"};
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

		fprintf(stderr, "sigma=%f\n", sigma);
		free(thedata);
		free(x);
		free(y);
	}



	if (status == END_OF_FILE)
		status = 0; /* Reset after normal error */

	fits_close_file(fptr, &status);
	fits_close_file(ofptr, &status);


	if (status)
		fits_report_error(stderr, status); /* print any error message */
	return (status);
}
