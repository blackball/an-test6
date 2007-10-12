/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Michael Blanton, Keir Mierle, David W. Hogg,
  Sam Roweis and Dustin Lang.

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

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>

#include "an-bool.h"
#include "fitsio.h"
#include "ioutils.h"
#include "simplexy.h"
#include "svn.h"

#define MAXNPEAKS 10000

static float *x = NULL;
static float *y = NULL;
static float *flux = NULL;

static const char* OPTIONS = "hpOo:q8";

void printHelp() {
	fprintf(stderr,
			"Usage: image2xy [options] fitsname.fits \n"
			"\n"
			"Read a FITS file, find objects, and write out \n"
			"X, Y, FLUX to   fitsname.xy.fits .\n"
			"\n"
			"   [-O]  overwrite existing output file.\n"
			"   [-p]  compute image percentiles.\n"
            "   [-8]  don't use optimization for byte (u8) images.\n"
			"   [-o <output-filename>]  write XYlist to given filename.\n"
			"\n"
			"   image2xy 'file.fits[1]'   - process first extension.\n"
			"   image2xy 'file.fits[2]'   - process second extension \n"
			"   image2xy file.fits+2      - same as above \n"
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

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	fitsfile *fptr;         /* FITS file pointer, defined in fitsio.h */
	fitsfile *ofptr;        /* FITS file pointer to output file */
	char outfile[300];
	char* outfn = NULL;
	int status = 0; // FIXME should have ostatus too
	int naxis;
	int maxnpeaks = MAXNPEAKS, npeaks;
	long naxisn[2];
	int kk, jj;
	float *thedata = NULL;
    unsigned char* theu8data = NULL;
	float sigma;
	int percentiles = 0;
	char* infn;
	int nhdus,maxper,maxsize,halfbox,hdutype,nimgs;
	float dpsf,plim,dlim,saddle;
	int overwrite = 0;
    bool verbose = TRUE;
    char* str;
    bool do_u8 = TRUE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case '8':
            do_u8 = FALSE;
            break;
        case 'q':
            verbose = FALSE;
            break;
		case 'p':
			percentiles = 1;
			break;
		case 'O':
			overwrite = 1;
			break;
		case 'o':
			outfn = optarg;
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
    if (verbose)
        fprintf(stderr, "infile=%s\n", infn);
	if (fits_open_file(&fptr, infn, READONLY, &status)) {
		fprintf(stderr, "Error reading file %s\n", infn);
		fits_report_error(stderr, status);
		exit(-1);
	}

	// Are there multiple HDU's?
	fits_get_num_hdus(fptr, &nhdus, &status);
    if (verbose)
        fprintf(stderr, "nhdus=%d\n", nhdus);

	if (!outfn) {
		outfn = outfile;
		// Create xylist filename (by trimming '.fits')
		snprintf(outfile, sizeof(outfile), "%.*s.xy.fits", (int) (strlen(infn)-5), infn);
        if (verbose)
            fprintf(stderr, "outfile=%s\n",outfile);
	}

	if (overwrite) {
        if (file_exists(outfn)) {
            if (verbose)
                fprintf(stderr, "Deleting existing output file \"%s\"...\n", outfn);
			if (unlink(outfn)) {
				fprintf(stderr, "Failed to delete existing output file \"%s\": %s\n",
						outfn, strerror(errno));
				exit(-1);
			}
		}
	}

	// Create output file
	if (fits_create_file(&ofptr, outfn, &status)) {
		fits_report_error(stderr, status);
		exit( -1);
	}
	fits_create_img(ofptr, 8, 0, NULL, &status);
	assert(!status);

	fits_write_key(ofptr, TSTRING, "SRCFN", infn, "Source image", &status);
	/* Parameters for simplexy; save for debugging */
	fits_write_comment(ofptr, "Parameters used for source extraction", &status);
	dpsf = 1.0;       /* gaussian psf width (pix) */
	plim = 8.0;       /* significance to keep */
	dlim = dpsf;      /* closest two peaks can be */
	saddle = 5.0;     /* saddle difference (in sig) */
	maxper = 1000;    /* maximum number of peaks per object */
	maxsize = 1000;   /* maximum size for extended objects */
	halfbox = 100;    /* half-width for sliding sky median box */
	fits_write_key(ofptr, TFLOAT, "DPSF", &dpsf, "image2xy Assumed gaussian psf width", &status);
	fits_write_key(ofptr, TFLOAT, "PLIM", &plim, "image2xy Significance to keep", &status);
	fits_write_key(ofptr, TFLOAT, "DLIM", &dlim, "image2xy Closest two peaks can be", &status);
	fits_write_key(ofptr, TFLOAT, "SADDLE", &saddle, "image2xy Saddle difference (in sig)", &status);
	fits_write_key(ofptr, TINT, "MAXPER", &maxper, "image2xy Max num of peaks per object", &status);
	fits_write_key(ofptr, TINT, "MAXPEAKS", &maxnpeaks, "image2xy Max num of peaks total", &status);
	fits_write_key(ofptr, TINT, "MAXSIZE", &maxsize, "image2xy Max size for extended objects", &status);
	fits_write_key(ofptr, TINT, "HALFBOX", &halfbox, "image2xy Half-size for sliding sky window", &status);

	fits_write_history(ofptr, 
                       "Created by Astrometry.net's simplexy.",
                       &status);
	assert(!status);
    asprintf_safe(&str, "SVN URL: %s", svn_url());
	fits_write_history(ofptr, str, &status);
	assert(!status);
    free(str);
    asprintf_safe(&str, "SVN Rev: %i", svn_revision());
	fits_write_history(ofptr, str, &status);
	assert(!status);
    free(str);
    asprintf_safe(&str, "SVN Date: %s", svn_date());
	fits_write_history(ofptr, str, &status);
	assert(!status);
    free(str);
	fits_write_history(ofptr, 
                       "Visit us on the web at http://astrometry.net/",
                       &status);
	assert(!status);

	nimgs = 0;

	// Run simplexy on each HDU
	for (kk=1; kk <= nhdus; kk++) {
		char* ttype[] = {"X","Y","FLUX"};
		char* tform[] = {"E","E","E"};
		char* tunit[] = {"pix","pix","unknown"};
		long* fpixel;
		int a;
		int w, h;
        int bitpix;

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

        if (verbose)
            fprintf(stderr,"Got naxis=%d,na1=%lu,na2=%lu\n", naxis,naxisn[0],naxisn[1]);

		if (naxis > 2) {
            if (verbose)
                fprintf(stderr, "NAXIS > 2: processing the first image plane only.\n");
		}

        fits_get_img_type(fptr, &bitpix, &status);
		if (status) {
			fits_report_error(stderr, status);
			exit( -1);
		}
        fprintf(stderr, "BITPIX: %i\n", bitpix);

		fpixel = malloc(naxis * sizeof(long));
		for (a=0; a<naxis; a++)
			fpixel[a] = 1;

        if (bitpix == 8 && do_u8) {
            // u8 image.
            theu8data = malloc(naxisn[0] * naxisn[1]);
            if (!theu8data) {
                fprintf(stderr, "Failed allocating data array.\n");
                exit( -1);
            }
            fits_read_pix(fptr, TBYTE, fpixel, naxisn[0]*naxisn[1], NULL,
                          theu8data, NULL, &status);
        } else {
            thedata = malloc(naxisn[0] * naxisn[1] * sizeof(float));
            if (!thedata) {
                fprintf(stderr, "Failed allocating data array.\n");
                exit( -1);
            }
            fits_read_pix(fptr, TFLOAT, fpixel, naxisn[0]*naxisn[1], NULL,
                          thedata, NULL, &status);
        }

		free(fpixel);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		x = malloc(maxnpeaks * sizeof(float));
		y = malloc(maxnpeaks * sizeof(float));
		if (x == NULL || y == NULL) {
			fprintf(stderr, "Failed allocating output arrays.\n");
			exit( -1);
		}
		flux = malloc(maxnpeaks * sizeof(float));

        if (bitpix == 8 && do_u8) {
            simplexy_u8(theu8data, naxisn[0], naxisn[1],
                        dpsf, plim, dlim, saddle, maxper, maxnpeaks,
                        maxsize, halfbox, &sigma, x, y, flux, &npeaks,
                        (verbose?1:0));
        } else {
            simplexy(thedata, naxisn[0], naxisn[1],
                     dpsf, plim, dlim, saddle, maxper, maxnpeaks,
                     maxsize, halfbox, &sigma, x, y, flux, &npeaks,
                     (verbose?1:0));
        }

		// The FITS standard specifies that the center of the lower
		// left pixel is 1,1. Store our xylist according to FITS
		for (jj=0; jj<npeaks; jj++) {
			x[jj] += 1.0;
			y[jj] += 1.0;
		}

		fits_create_tbl(ofptr, BINARY_TBL, npeaks, 3, ttype,tform,
				tunit, "SOURCES", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		fits_write_col(ofptr, TFLOAT, 1, 1, 1, npeaks, x, &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		fits_write_col(ofptr, TFLOAT, 2, 1, 1, npeaks, y, &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		fits_write_col(ofptr, TFLOAT, 3, 1, 1, npeaks, flux, &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		fits_modify_comment(ofptr, "TTYPE1", "X coordinate", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		fits_modify_comment(ofptr, "TTYPE2", "Y coordinate", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		fits_modify_comment(ofptr, "TTYPE3", "Flux of source", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

		fits_write_key(ofptr, TINT, "SRCEXT", &kk,
				"Extension number in src image", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}
		w = naxisn[0];
		h = naxisn[1];
		fits_write_key(ofptr, TINT, "IMAGEW", &w,
					   "Input image width", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}
		fits_write_key(ofptr, TINT, "IMAGEH", &h,
					   "Input image height", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}
		fits_write_key(ofptr, TFLOAT, "ESTSIGMA", &sigma,
				"Estimated source image variance", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}
		fits_write_comment(ofptr,
			"The X and Y points are specified assuming 1,1 is "
			"the center of the leftmost bottom pixel of the "
			"image in accordance with the FITS standard.", &status);
		if (status) {
			fits_report_error(stderr, status);
			assert(!status);
			exit(-1);
		}

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
			np = MIN(n, NPIX);
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
		free(theu8data);
		free(x);
		free(y);
	}

	// Put in the optional NEXTEND keywoard
	fits_movabs_hdu(ofptr, 1, &hdutype, &status);
	assert(hdutype == IMAGE_HDU);
	fits_write_key(ofptr, TINT, "NEXTEND", &nimgs, "Number of extensions", &status);
	if (status == END_OF_FILE)
		status = 0; /* Reset after normal error */
	if (status) {
		fits_report_error(stderr, status);
		assert(!status);
		exit(-1);
	}

	fits_close_file(fptr, &status);
	if (status) {
		fits_report_error(stderr, status);
		assert(!status);
		exit(-1);
	}
	fits_close_file(ofptr, &status);
	if (status) {
		fits_report_error(stderr, status);
		assert(!status);
		exit(-1);
	}

	return 0;
}
