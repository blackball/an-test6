/*
  NAME:
    tweak
  PURPOSE:
    Run WCS tweak on a FITS image (including all extensions)
  INPUTS:
    Run tweak -h to see help
  OPTIONAL INPUTS:
    None yet
  KEYWORD:
  BUGS:
    - Not rigorously tested.
    - Dependencies 
      - needs kdtree-ified usnob or an.net catalog
      - needs xylist generated by fits2xy, or any FITS file with X Y FLUX
  REVISION HISTORY:
  WHO TO SHOOT IF IT BREAKS:
    Keir
*/
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
#include "ezfits.h"

double max(double x, double y)
{
	if (x < y) return y;
	return x;
}
double min(double x, double y)
{
	if (x < y) return x;
	return y;
}

typedef struct WorldCoor wcs_t;

// Hacky. Pull the complete hdu from the current hdu, then use wcstools to
// figure out the wcs, and return it
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

	// Create a memory buffer to hold the header records
	int tmpbufflen = nkeys*(FLEN_CARD-1)*sizeof(char)+1;
	char* tmpbuff = malloc(tmpbufflen);
	assert(tmpbuff);

	// Read all of the header records in the input HDU
	for (ii = 0; ii < nkeys; ii++) {
		char* thiscard = tmpbuff + (ii * (FLEN_CARD-1));
		if (ffgrec(infptr, ii+1, thiscard, status)) {
			fits_report_error(stderr, *status);
			exit(-1);
		}

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
	// Find this extension in fptr (which should be open to the xylist)
	int nhdus, hdutype;
	int status=0;

	if (fits_get_num_hdus(fptr, &nhdus, &status)) {
		fits_report_error(stderr, status);
		exit(-1);
	}

	// Search each HDU looking for the one with SRCEXT==hdu
	int i, ext=-1;
	for (i=1; i<=nhdus; i++) {
		if (fits_movabs_hdu(fptr, i, &hdutype, &status)) {
			fits_report_error(stderr, status);
			exit(-1);
		}
		if (i != 1)
			assert(hdutype != IMAGE_HDU) ;

		if (fits_read_key(fptr, TINT, "SRCEXT ",
					&ext, NULL, &status)) {
			status = 0;
			continue;
		}
		if (ext == hdu)
			break;
	}

	// Sanity
	if (ext != hdu) {
		fprintf(stderr, "Couldn't find xylist for HDU %d\n",hdu);
		exit(-1);
	}
	if (fits_get_hdu_type(fptr, &hdutype, &status)) {
		fits_report_error(stderr, status);
		exit(-1);
	}
	assert(hdutype != IMAGE_HDU);

	// Now pull the X and Y columns
	long l;
	if (fits_get_num_rows(fptr, &l, &status)) {
		fits_report_error(stderr, status);
		exit(-1);
	}
	*n = l;
	fprintf(stderr, "n=%d\n", *n);

	*x = malloc(sizeof(double)* *n);
	*y = malloc(sizeof(double)* *n);
	if (fits_read_col(fptr, TFLOAT, 1, 1, 1, *n, NULL, *x, NULL, &status)) {
		fits_report_error(stderr, status);
		exit(-1);
	}
	if (fits_read_col(fptr, TFLOAT, 2, 1, 1, *n, NULL, *y, NULL, &status)) {
		fits_report_error(stderr, status);
		exit(-1);
	}
	return 0;
}

void get_center_and_radius(double* ra, double* dec, int n,
                          double* ra_mean, double* dec_mean, double* radius)
{
	double* xyz = malloc(3*n*sizeof(double));
	double xyz_mean[3] = {0,0,0};
	int i, j;
	for (i=0; i<n; i++) {
		radec2xyzarr(deg2rad(ra[i]),
		             deg2rad(dec[i]),
		             xyz + 3*i);
	}

	for (i=0; i<n; i++) 
		for (j=0; j<3; j++) 
			xyz_mean[j] += xyz[3*i+j];

	for (j=0; j<3; j++) 
		xyz_mean[j] /= (double) n;

	double norm=0;
	for (j=0; j<3; j++) 
		norm += xyz_mean[j]*xyz_mean[j];
	norm = sqrt(norm);

	for (j=0; j<3; j++) 
		xyz_mean[j] /= norm;

	double maxdist2 = 0;
	int maxind = -1;
	for (i=0; i<n; i++) {
		double dist2 = 0;
		for (j=0; j<3; j++) {
			double d = xyz_mean[j]-xyz[3*i+j];
			dist2 += d*d;
		}
		if (maxdist2 < dist2) {
			maxdist2 = dist2;
			maxind = i;
		}
	}
	*radius = sqrt(maxdist2);
	*ra_mean = rad2deg(xy2ra(xyz_mean[0],xyz_mean[1]));
	*dec_mean = rad2deg(z2dec(xyz_mean[2]));
}

void get_reference_stars(double ra_mean, double dec_mean, double radius,
                         double** ra, double **dec, int *n, char* hppat)
{
	// FIXME magical 9 constant == an_cat hp res NSide
	int hp = radectohealpix_nside(deg2rad(ra_mean), deg2rad(dec_mean), 9); 

	char buf[1000];
	snprintf(buf,1000, hppat, hp);
	fprintf(stderr, "opening %s\n",buf);
	kdtree_t* kd = kdtree_fits_read_file(buf);
	fprintf(stderr, "success\n");
	assert(kd);

	double xyz[3];
	radec2xyzarr(deg2rad(ra_mean), deg2rad(dec_mean), xyz);
	radec2xyzarr(deg2rad(158.70829), deg2rad(51.919442), xyz);

	// Fudge radius factor because if the shift is really big, then we
	// can't actually find the correct astrometry.
	double radius_factor = 1.3;
	kdtree_qres_t* kq = kdtree_rangesearch(kd, xyz,
			radius*radius*radius_factor);
	fprintf(stderr, "Did range search got %u stars\n", kq->nres);

	*n = kq->nres;

	// No stars? That's bad. Run away.
	if (!*n)
		return;

	*ra = malloc(sizeof(double)*kq->nres);
	*dec = malloc(sizeof(double)*kq->nres);
	assert(*ra);
	assert(*dec);

	int i;
	fprintf(stderr, "ref stars:\n");
	for (i=0; i<kq->nres; i++) {
		double *xyz = kq->results+3*i;
		(*ra)[i] = rad2deg(xy2ra(xyz[0],xyz[1]));
		(*dec)[i] = rad2deg(z2dec(xyz[2]));
		/*
		if (i < 30) {
			fprintf(stderr, "a=%f d=%f\n",(*ra)[i],(*dec)[i]);
			fprintf(stderr, "x=%f y=%f z=%f\n",xyz[0],xyz[1], xyz[2]);
			fprintf(stderr, "dist=%f\n",sqrt(kq->sdists[i]));
			fprintf(stderr, "distdeg=%f\n",sqrt(kq->sdists[i]));
		}
		*/
	}

	kdtree_free_query(kq);
	kdtree_fits_close(kd);
}

void get_shift(double* ximg, double* yimg, int nimg,
               double* xcat, double* ycat, int ncat, 
               double* xshift, double* yshift)
{

	int i, j;

	// hough transform 
	int hsz = 1000; // hough size
	int *hough = malloc(hsz*hsz*sizeof(double));
	for (i=0; i<hsz*hsz; i++)
		hough[i] = 0;

	double maxdx = -1e100;
	double mindx = 1e100;
	double maxdy = -1e100;
	double mindy = 1e100;
	for (i=0; i<nimg; i++) {
		for (j=0; j<ncat; j++) {
			double dx = ximg[i]-xcat[j];
			double dy = yimg[i]-ycat[j];
			maxdx = max(dx,maxdx);
			maxdy = max(dy,maxdy);
			mindx = min(dx,mindx);
			mindy = min(dy,mindy);
		}
	}


	for (i=0; i<nimg; i++) {
		for (j=0; j<ncat; j++) {
			double dx = ximg[i]-xcat[j];
			double dy = yimg[i]-ycat[j];
			int hszi = hsz-1;
			int iy = hszi*( (dy-mindy)/(maxdy-mindy) );
			int ix = hszi*( (dx-mindx)/(maxdx-mindx) );
			assert (iy >=0);
			assert (ix >=0);
			assert (iy*hsz+ ix < hsz*hsz);
			if (0 < iy && iy < hsz+1 &&
					0 < ix && ix < hsz+1) {
				// approx gauss
				hough[(iy-1)*hsz + (ix-1)] += 1;
				hough[(iy+1)*hsz + (ix+1)] += 1;
				hough[(iy-1)*hsz + (ix+1)] += 1;
				hough[(iy+1)*hsz + (ix-1)] += 1;
				hough[(iy-0)*hsz + (ix-1)] += 4;
				hough[(iy-1)*hsz + (ix-0)] += 4;
				hough[(iy+0)*hsz + (ix+1)] += 4;
				hough[(iy+1)*hsz + (ix+0)] += 4;
				hough[iy*hsz + ix] += 10;
			}
		}
	}

	int themax = 0;
	int themaxind;
	for (i=0; i<hsz*hsz; i++) {
		if (themax < hough[i]) {
			themaxind = i;
			themax = hough[i];
		}
	}

	int ys = themaxind/hsz;
	int xs = themaxind%hsz;

	fprintf(stderr, "xshsz = %d, yshsz=%d\n",xs,ys);

	*yshift = ((double)(themaxind/hsz)/(double)hsz)*(maxdy-mindy)+mindy;
	*xshift = ((double)(themaxind % hsz)/(double)hsz)*(maxdx-mindx)+mindx;
	fprintf(stderr, "xs=%lf, ys=%lf\n", *xshift, *yshift);

	static char c = '1';
	static char fn[] = "houghN.fits";
	fn[5] = c++;
	ezwriteimage(fn, TINT, hough, hsz, hsz);
}

wcs_t* copy_wcs(wcs_t* wcs)
{
	wcs_t* nwcs = malloc(sizeof(wcs_t));
	memcpy(nwcs, wcs, sizeof(wcs_t));

	// FIXME
	if (wcs->lngcor || wcs->latcor) {
		//struct IRAFsurface *lngcor;	/* RA/longitude correction structure */
		//struct IRAFsurface *latcor;	/* Dec/latitude correction structure */
		fprintf(stderr, "tweak: not implemented.");
		assert(0);
	}

	int i;
	for (i=0; i<10; i++)
		if (wcs->command_format[i]) 
			nwcs->command_format[i] = strdup(wcs->command_format[i]);

	// FIXME is this cool? Can we just copy this over blindly?
	//struct WorldCoor *wcs;	/* WCS upon which this WCS depends */
	//struct WorldCoor *wcsdep;	/* WCS depending on this WCS */

  	if (wcs->wcsname)
		nwcs->wcsname = strdup(wcs->wcsname);

	return nwcs;
}

// Take shift in image plane and do a switcharoo to make the wcs something
// better
wcs_t* wcs_shift(wcs_t* wcs, double xs, double ys)
{
	wcs_t* swcs = copy_wcs(wcs);

	double crpixx = wcs->xrefpix;
	double crpixy = wcs->yrefpix;

	wcs->xrefpix -= xs;
	wcs->yrefpix -= ys;

	// now reproject the old crpix[xy] into swcs
	pix2wcs(wcs, crpixx, crpixy, &swcs->xref, &swcs->yref);

	wcs->xrefpix = crpixx;
	wcs->yrefpix = crpixy;

	//fprintf(stderr, "ra shift: %f", swcs->xref

	return swcs;
}

/* Fink-Hogg shift */

/* This one isn't going to be fun. */

/* spherematch */

void printHelp(char* progname)
{
	fprintf(stderr, "%s usage:\n"
	        "   -i <input file>      file to tweak\n"
	        "   -o <output-file>     destination file\n"
	        "   -s <xy-list>         sources (produced by fits2xy)\n"
	        "   -d <database-subst>  pattern for the index\n"
	        "   -n <healpix-nside>   number of healpixes in index\n"
	        , progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
	wcs_t* wcs;
	fitsfile *fptr, *xyfptr;  /* FITS file pointer, defined in fitsio.h */
	//fitsfile *ofptr;        /* FITS file pointer to output file */
	int status = 0; // FIXME should have ostatus too
	int naxis;
	int kk;
	char* xyfile = NULL;
	char* outfile = NULL;
	char* infile = NULL;
	char* hppat = NULL;
	int Nside = 0;
	char argchar;

	while ((argchar = getopt(argc, argv, "hi:s:o:d:n:")) != -1)
		switch (argchar) {
		case 'h':
			printHelp(argv[0]);
			exit(0);
		case 'i':
			infile = optarg;
			break;
		case 's':
			xyfile = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'd':
			hppat = optarg;
			break;
		case 'n':
			Nside = atoi(optarg);
			break;
		}

	if (!outfile || !infile || !xyfile || !hppat || !Nside) {
		printHelp(argv[0]);
		exit( -1);
	}

	if (fits_open_file(&fptr, infile, READONLY, &status)) {
		fprintf(stderr, "Error reading image file %s\n", infile);
		fits_report_error(stderr, status);
		exit(-1);
	}

	int nhdus;
	if (fits_get_num_hdus(fptr, &nhdus, &status)) {
		fits_report_error(stderr, status);
		exit(-1);
	}
	fprintf(stderr, "nhdus=%d\n", nhdus);

	// Load xylist
	if (fits_open_file(&xyfptr, xyfile, READONLY, &status)) {
		fprintf(stderr, "Error reading XY file %s\n", xyfile);
		fits_report_error(stderr, status);
		exit(-1);
	}

	// Tweak each HDU independently
	for (kk=1; kk <= nhdus; kk++) {
		int hdutype;
		if (fits_movabs_hdu(fptr, kk, &hdutype, &status)) {
			fits_report_error(stderr, status);
			exit(-1);
		}

		if (fits_get_hdu_type(fptr, &hdutype, &status)) {
			fits_report_error(stderr, status);
			exit(-1);
		}

		if (hdutype != IMAGE_HDU) 
			continue;

		if (fits_get_img_dim(fptr, &naxis, &status)) {
			fits_report_error(stderr, status);
			exit( -1);
		}

		if (naxis != 2) {
			fprintf(stderr, "Invalid image: NAXIS is not 2 in HDU %d!\n", kk);
			continue;
		}

		// FIXME BREAK HERE into new function

		// At this point, we have an image. Now get the WCS data
		wcs = get_wcs_from_hdu(fptr);
		if (!wcs) {
			fprintf(stderr, "Problems with WCS info, skipping HDU\n");
			continue;
		}

		// Extract xy
		int n,jj;
		float *xf, *yf;
		get_xy(xyfptr, kk, &xf, &yf, &n);

		// Convert to doubles
		double *x, *y;
		x = malloc(sizeof(double)*n);
		y = malloc(sizeof(double)*n);
		for (jj=0; jj<n; jj++) {
			x[jj] = xf[jj];
			y[jj] = yf[jj];
		}

		// Convert to ra dec
		double *a, *d;
		a = malloc(sizeof(double)*n);
		d = malloc(sizeof(double)*n);
		for (jj=0; jj<n; jj++) {
			pix2wcs(wcs, x[jj], y[jj], a+jj, d+jj);
		}

		// Find field center/radius
		double ra_mean, dec_mean, radius;
		get_center_and_radius(a, d, n, &ra_mean, &dec_mean, &radius);
		fprintf(stderr, "abar=%f, dbar=%f, radius in rad=%f\n",ra_mean, dec_mean, radius);

		// Get the reference stars from our catalog
		double *a_ref, *d_ref;
		int n_ref;
		get_reference_stars(ra_mean, dec_mean, radius,
		                    &a_ref, &d_ref, &n_ref, hppat);
		if (!n_ref)  {
			// No reference stars? This is bad.
			fprintf(stderr, "No reference stars; failing\n");
			free(a);
			free(d);
			free(x);
			free(y);
			continue;
		}

		// Shift runs in XY; project reference stars
		double *x_ref = malloc(sizeof(double)*n_ref);
		double *y_ref = malloc(sizeof(double)*n_ref);
		double xshift, yshift;
		int offscr;
		for (jj=0; jj<n_ref; jj++) {
			wcs2pix(wcs, a_ref[jj], d_ref[jj],
			        x_ref+jj, y_ref+jj, &offscr);
		}

		// Run our wonderful shift algorithm
		get_shift(x, y, n, x_ref, y_ref, n_ref, &xshift, &yshift);
		//exit(1);
	}


	if (status == END_OF_FILE)
		status = 0; /* Reset after normal error */

	fits_close_file(fptr, &status);

	if (status)
		fits_report_error(stderr, status); /* print any error message */
	return (status);
	

	/* Fink-Hogg shift */
	/* Fink-Hogg shift */
	/* Correspondeces via spherematch -- switch to dualtree */
	/* Linear tweak */
	/* Non-linear tweak */

}
