#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "libwcs/fitsfile.h"
#include "libwcs/wcs.h"
#include "starutil.h"
#include "fitsio.h"
#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "assert.h"
#include "healpix.h"

typedef struct WorldCoor wcs_t;

// hacky. pull the complete hdu from the current hdu, then use wcstools to
// figure out the wcs
wcs_t* get_wcs_from_hdu(fitsfile* infptr)
{
	int mystatus = 0;
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
	return wcsninit(tmpbuff, tmpbufflen);
}

int get_xy(fitsfile* fptr, int hdu, float **x, float **y, int *n)
{
	// find this extension in fptr (which should be open to the xylist)
	int nhdus, hdutype;
	int status=0;
	fits_get_num_hdus(fptr, &nhdus, &status);
	fprintf(stderr, "nhdus=%d\n", nhdus);
	int i, ext=33;
	for (i=1; i<=nhdus; i++) {
		fprintf(stderr, "+++++++++++++++++++++++++++++\n");
		fits_movabs_hdu(fptr, i, &hdutype, &status);
		if (status)
			fits_report_error(stderr, status);
		assert(!status);
		if (0) { // REMOVE ME DEBUG CODE!!!
			int mystatus=0;
			int* status = &mystatus;
			int nkeys, ii;
			fitsfile *infptr = fptr;

			if (ffghsp(infptr, &nkeys, NULL, status) > 0) {
				fprintf(stderr, "nomem\n");
				fits_report_error(stderr, *status);
				return 0;
			}
			fprintf(stderr, "nkeys=%d\n",nkeys);

			char tmpbuff[3*FLEN_CARD];

			for (ii = 0; ii < nkeys; ii++) {
				ffgrec(infptr, ii+1, tmpbuff, status);
				fits_report_error(stderr, *status);
				assert(!*status);
				fprintf(stderr,"%s\n",tmpbuff);
			}
		}
		if (i != 1)
			assert(hdutype != IMAGE_HDU) ;
		//fits_read_key(fptr, TINT, "BITPIX", &ext, NULL, &status);
		fits_read_key(fptr, TINT, "SRCEXT ", &ext, NULL, &status);
		if (status) {
			status = 0;
			continue;
		}
		fprintf(stderr, "SRCEXT=%d\n", ext);
		fprintf(stderr, "i=%d\n", i);
		fprintf(stderr, "status=%d\n", status);
		fits_report_error(stderr, status);
		if (ext == hdu)
			break;
	}
	fprintf(stderr, "Got it.\n");
	fits_get_hdu_type(fptr, &hdutype, &status);

	assert(hdutype != IMAGE_HDU);
	long nn;
	fits_get_num_rows(fptr, &nn, &status);
	fits_report_error(stderr, status);
	*n = nn;
	assert(!status);

	*x = malloc(sizeof(double)* *n);
	*y = malloc(sizeof(double)* *n);
	fits_read_col(fptr, TFLOAT, 1, 1, 1, *n, NULL, *x, NULL, &status);
	assert(!status);
	fits_read_col(fptr, TFLOAT, 2, 1, 1, *n, NULL, *y, NULL, &status);
	assert(!status);
	fprintf(stderr, "n=%d\n", *n);
	return 0;
}

int get_center_and_radius(double* ra, double* dec, int n,
                          double* ra_mean, double* dec_mean, double* radius)
{
	double* xyz = malloc(3*n*sizeof(double));
	double xyz_mean[3] = {0,0,0};
	int i, j;
	for (i=0; i<n; i++) {
		radec2xyzarr(deg2rad(ra[i]),deg2rad(dec[i]),xyz+3*i);
		//fprintf(stderr,"++%f,%f\n",ra[i],dec[i]);
		//fprintf(stderr,"%f,%f,%f\n",xyz[3*i],xyz[3*i+1],xyz[3*i+2]);
		//fprintf(stderr,".. %f,%f\n",rad2deg(xy2ra(xyz[3*i],xyz[3*i+1])),
				//rad2deg(z2dec(xyz[3*i+2])));
	}

	for (i=0; i<n; i++) 
		for (j=0; j<3; j++) 
			xyz_mean[j] += xyz[3*i+j];

	for (j=0; j<3; j++) 
		xyz_mean[j] /= (double) n;

	double maxdist2 = 1e-300;
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
	*radius = rad2deg(sqrt(maxdist2));
	*ra_mean = rad2deg(xy2ra(xyz_mean[0],xyz_mean[1]));
	*dec_mean = rad2deg(z2dec(xyz_mean[2]));
	return 0;
}

int get_reference_stars(double ra_mean, double dec_mean, double radius,
                        double** ra, double **dec, int *n)
{
	// FIXME magical 9 constant == an_cat hp res NSide
	int hp = radectohealpix_nside(ra_mean, dec_mean, 9); 

	char buf[1000];
	snprintf(buf,1000, "/global/metal1/scr/keir/AN/AN/an_hp%03d.rkdt.fits", hp);
	fprintf(stderr, "opening %s\n",buf);
	kdtree_t* kd = kdtree_fits_read_file(buf);
	fprintf(stderr, "success\n");
	assert(kd);

	double xyz[3];
	radec2xyzarr(deg2rad(ra_mean), deg2rad(dec_mean), xyz);

	kdtree_qres_t* kq = kdtree_rangesearch_nosort(kd, xyz, radius);
	fprintf(stderr, "Did range search got %u stars\n", kq->nres);

	*ra = malloc(sizeof(double)*kq->nres);
	*dec = malloc(sizeof(double)*kq->nres);
	*n = kq->nres;

	int i;
	for (i=0; i<kq->nres; i++) {
		double *xyz = kq->results+3*i;
		(*ra)[i] = rad2deg(xy2ra(xyz[0],xyz[1]));
		(*dec)[i] = rad2deg(z2dec(xyz[2]));
	}

	kdtree_free_query(kq);
	kdtree_fits_close(kd);

	return 0;
}

/* Fink-Hogg shift */
/* This one isn't going to be fun. */

/* spherematch */

int main(int argc, char *argv[])
{
	wcs_t* wcs;
	/* Are there multiple images? */
	fitsfile *fptr, *xyfptr;  /* FITS file pointer, defined in fitsio.h */
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

	// Create xylist filename (by trimming '.fits')
	int fnamelen = strlen(argv[1]);
	int fnlen=1024;
	char xyfile[fnlen]; 
	char xysfile[fnlen]; 
	char sbuf[fnlen]; 
	assert(argv[1][fnamelen] == '\0');
	assert(fnamelen > 5);
	snprintf(sbuf, fnlen, "%s", argv[1]);
	sbuf[fnamelen-5] = '\0';
	snprintf(xyfile, fnlen, "%s.xy.fits",sbuf);
	snprintf(xysfile, fnlen, "%s.xys.fits",sbuf);
	fprintf(stderr, "xyfile=%s\n",xyfile);
	fprintf(stderr, "xysfile=%s\n",xyfile);

	// Check if it exists, create it if it doesn't
	struct stat buf;
	if (stat(xyfile, &buf)) {
		if (errno == ENOENT) {
			// Detect sources with simplexy
			fprintf(stderr, "need to detect sources. running simplexy...----\n");
			char tmpbuff[2*fnlen];
			int ret;
			snprintf(tmpbuff, 2*fnlen, "../simplexy/fits2xy %s", argv[1]);
			fprintf(stderr, "CMD: %s\n", tmpbuff);
			ret = system(tmpbuff);
			if (ret) {
				fprintf(stderr, "Error executing fits2xy. exiting");
				exit(1);
			}
			fprintf(stderr, "---done\n");

			// Sort by flux. in theory simplexy should do this but
			// the code was already written.
			fprintf(stderr, "sorting on flux...\n");
			snprintf(tmpbuff, 2*fnlen, "../quads/tabsort -d -i %s -o %s -c flux", xyfile, xysfile);
			ret = system(tmpbuff);
			if (ret) {
				fprintf(stderr, "Error executing fits2xy. exiting");
				exit(1);
			}
			fprintf(stderr, "done\n");
		}
	}

	// Load xylist
	if (fits_open_file(&xyfptr, xysfile, READONLY, &status)) {
		fprintf(stderr, "Error reading xy file %s\n", xysfile);
		fits_report_error(stderr, status);
		exit(-1);
	}

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

		// Extract xy
		int n,jj;
		float *x, *y;
		get_xy(xyfptr, kk, &x, &y, &n);
		//for (jj=0; jj<n; jj++) 
			//fprintf(stderr, "%f %f\n",x[jj], y[jj]);


		// Convert to ra dec
		//int jj;
		double *a, *d;
		a = malloc(sizeof(double)*n);
		d = malloc(sizeof(double)*n);
		for (jj=0; jj<n; jj++) {
			pix2wcs(wcs, x[jj], y[jj], a+jj, d+jj);
			//fprintf(stderr, "%f %f\n",a[jj],d[jj]);
		}

		// Find field center/radius
		double ra_mean, dec_mean, radius;
		get_center_and_radius(a, d, n, &ra_mean, &dec_mean, &radius);
		fprintf(stderr, "abar=%f, dbar=%f, rad=%f\n",ra_mean,dec_mean,radius);

		double *a_ref, *d_ref;
		int n_ref;
		get_reference_stars(ra_mean, dec_mean, radius,
					&a_ref, &d_ref, &n_ref);

	}


	if (status == END_OF_FILE)
		status = 0; /* Reset after normal error */

	fits_close_file(fptr, &status);

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
