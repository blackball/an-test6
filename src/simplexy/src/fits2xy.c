#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "fitsio.h"
#include "dimage.h"

#define MAXNPEAKS 100000

static float *x = NULL;
static float *y = NULL;
static float *flux = NULL;

static const char* OPTIONS = "hp";

void printHelp() {
	fprintf(stderr,
			"Usage: fits2xy [-p] fitsname.fits \n"
			"\n"
			"Read a FITS file, find objects, and write out \n"
			"X, Y, FLUX to   fitsname.xy.fits .\n"
			"\n"
			"   [-p]  compute image percentiles.\n"
			"\n"
			"   fits2xy 'file.fits[1]'   - process first extension.\n"
			"   fits2xy 'file.fits[2]'   - process second extension \n"
			"   fits2xy file.fits+2      - same as above \n"
			"\n");
}

static int compare_floats(const void* v1, const void* v2) {
	float f1 = *(float*)v1;
	float f2 = *(float*)v2;
	if (f1 < f2)
		return -1;
	if (f1 > f2)
		return 1;
	return 0;
}

#define min(a,b) (((a)<(b))?(a):(b))

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
    int argchar;
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
	int percentiles = 0;
	char* infn;

	if (!((argc == 2) || (argc == 3))) {
		printHelp();
		return (0);
	}

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'p':
			percentiles = 1;
			break;
		case '?':
		case 'h':
			printHelp();
			exit(0);
		}

	if (optind != argc - 1) {
		printHelp();
		exit(-1);
	}

	infn = argv[optind];
	fprintf(stderr, "infile=%s\n", infn);
	if (fits_open_file(&fptr, infn, READONLY, &status)) {
		fprintf(stderr, "Error reading file %s\n", infn);
		fits_report_error(stderr, status);
		exit(-1);
	}

	// Are there multiple HDU's?
	int nhdus;
	fits_get_num_hdus(fptr, &nhdus, &status);
	fprintf(stderr, "nhdus=%d\n", nhdus);

	// Create xylist filename (by trimming '.fits')
	char outfile[300];
	snprintf(outfile, sizeof(outfile), "%.*s.xy.fits", strlen(infn)-5, infn);
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

		// OH MY GOD the horror.    <-- Keir is easily spooked.

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

		fprintf(stderr, "sigma=%g\n", sigma);

		fprintf(stderr, "Found %i peaks.\n", npeaks);

		// The FITS standard specifies that the center of the lower
		// left pixel is 1,1. Store our xylist according to FITS
		for (jj=0; jj<npeaks; jj++) {
			x[jj] += 1.0;
			y[jj] += 1.0;
		}

		char* ttype[] = {"X","Y","FLUX"};
		char* tform[] = {"E","E","E"};
		char* tunit[] = {"pix","pix","unknown"};
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
		fits_write_key(ofptr, TINT, "SRCEXT", &kk,
				"Extension number in src image", &status);
		assert(!status);
		fits_write_key(ofptr, TFLOAT, "ESTSIGMA", &sigma,
				"Estimated source image variance", &status);
		fits_write_comment(ofptr,
			"The X and Y points are specified assuming 1,1 is "
			"the center of the leftmost bottom pixel of the "
			"image in accordance with the FITS standard.", &status);
		assert(!status);


		if (percentiles) {
			// the number of pixels around the margin of the image to avoid.
			int margin = 5;
			// the maximum number of pixels to sample
			int NPIX = 10000;
			int nx, ny, n, np;
			float* pix;
			int x, y;
			int i;
			int pctls[] = { 0, 25, 50, 75, 95, 100 };
			nx = naxisn[0];
			ny = naxisn[1];
			n = (nx - 2*margin) * (ny - 2*margin);
			np = min(n, NPIX);
			pix = malloc(np * sizeof(float));
			if (n < NPIX) {
				i=0;
				for (y=margin; y<(ny-margin); y++)
					for (x=margin; x<(nx-margin); x++) {
						pix[i] = thedata[y*nx + x];
						i++;
					}
			} else {
				for (i=0; i<NPIX; i++) {
					x = margin + (nx - 2*margin) * ( (double)random() / (((double)RAND_MAX)+1.0) );
					y = margin + (ny - 2*margin) * ( (double)random() / (((double)RAND_MAX)+1.0) );
					pix[i] = thedata[y*nx + x];
				}
			}
			// just sort it, because I'm lazy.
			qsort(pix, np, sizeof(float), compare_floats);

			for (i=0; i<(sizeof(pctls)/sizeof(int)); i++) {
				int j = (int)((pctls[i] * 0.01) * np) - 1;
				if (j < 0) j = 0;
				if (j >= np) j = np-1;
				fprintf(stderr, "percentile%i %g\n", pctls[i], pix[j]);
			}
 
		}

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
