/*
 This file is part of the Astrometry.net suite.
 Copyright 2006-2007, Keir Mierle, David W. Hogg, Sam Roweis and Dustin Lang.

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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_vector_double.h>
#include <gsl/gsl_blas.h>

#include "tweak_internal.h"
#include "healpix.h"
#include "dualtree_rangesearch.h"
#include "kdtree_fits_io.h"
#include "mathutil.h"

// TODO:
//
//  1. Write the document which explains every step of tweak in detail with
//     comments relating exactly to the code.
//  2. Implement polynomial terms - use order parameter to zero out A matrix
//  3. Make it robust to outliers
//     - Make jitter evolve as fit evolves
//     - Sigma clipping
//  4. Need test image with non-trivial rotation to test CD transpose problem
//
//
//  - Put CD inverse into its own function
//
//  BUG? transpose of CD matrix is similar to CD matrix!
//  BUG? inverse when computing sx/sy (i.e. same transpose issue)
//  Ability to fit without re-doing correspondences
//  Note: USNO has about 1 arcsecond jitter, so don't go tighter than that!
//  Split fit x/y (i.e. two fits one for x one for y)

#define KERNEL_SIZE 5
#define KERNEL_MARG ((KERNEL_SIZE-1)/2)
typedef double doublereal;
typedef long int integer;
extern int dgelsd_(integer *m, integer *n, integer *nrhs, doublereal *a,
	                   integer *lda, doublereal *b, integer *ldb, doublereal *s,
	                   doublereal *rcond, integer *rank, doublereal *work, integer
	                   *lwork, integer *iwork, integer *info);

double dblmax(double x, double y)
{
	if (x < y)
		return y;
	return x;
}
double dblmin(double x, double y)
{
	if (x < y)
		return x;
	return y;
}

void get_dydx_range(double* ximg, double* yimg, int nimg,
                    double* xcat, double* ycat, int ncat,
                    double *mindx, double *mindy, double *maxdx, double *maxdy)
{
	int i, j;
	*maxdx = -1e100;
	*mindx = 1e100;
	*maxdy = -1e100;
	*mindy = 1e100;

	for (i = 0; i < nimg; i++) {
		for (j = 0; j < ncat; j++) {
			double dx = ximg[i] - xcat[j];
			double dy = yimg[i] - ycat[j];
			*maxdx = dblmax(dx, *maxdx);
			*maxdy = dblmax(dy, *maxdy);
			*mindx = dblmin(dx, *mindx);
			*mindy = dblmin(dy, *mindy);
		}
	}
}

void get_shift(double* ximg, double* yimg, int nimg,
               double* xcat, double* ycat, int ncat,
               double mindx, double mindy, double maxdx, double maxdy,
               double* xshift, double* yshift)
{

	int i, j;
	int themax, themaxind, ys, xs;

	// hough transform
	int hsz = 1000; // hough histogram size (per side)
	int *hough = calloc(hsz * hsz, sizeof(int)); // allocate bins
	int kern[] = {0, 2, 3, 2, 0,  // approximate gaussian smoother
	              2, 7, 12, 7, 2,       // should be KERNEL_SIZE x KERNEL_SIZE
	              3, 12, 20, 12, 3,
	              2, 7, 12, 7, 2,
	              0, 2, 3, 2, 0};

	for (i = 0; i < nimg; i++) {    // loop over all pairs of source-catalog objs
		for (j = 0; j < ncat; j++) {
			double dx = ximg[i] - xcat[j];
			double dy = yimg[i] - ycat[j];
			int hszi = hsz - 1;
			int iy = hszi * ( (dy - mindy) / (maxdy - mindy) ); // compute deltay using implicit floor
			int ix = hszi * ( (dx - mindx) / (maxdx - mindx) ); // compute deltax using implicit floor

			// check to make sure the point is in the box
			if (KERNEL_MARG <= iy && iy < hsz - KERNEL_MARG &&
			        KERNEL_MARG <= ix && ix < hsz - KERNEL_MARG) {
				int kx, ky;
				for (ky = -2; ky <= 2; ky++)
					for (kx = -2; kx <= 2; kx++)
						hough[(iy - ky)*hsz + (ix - kx)] += kern[(ky + 2) * 5 + (kx + 2)];
			}
		}
	}

	/*
	  FILE* ff = fopen("hough.dat", "w");
	  fwrite(hough, sizeof(int), hsz*hsz, ff);
	  fclose(ff);
	*/

	/*
	{
	     FILE *ff;
		static char c = '1';
		static char fn[] = "hough#.pgm";
		fn[5] = c++;
		ff = fopen(fn, "wb");
		fprintf(ff, "P5 %i %i 65535\n", hsz, hsz);
		for (i=0; i<(hsz*hsz); i++) {
			uint16_t u = htons((hough[i] < 65535) ? hough[i] : 65535);
			fwrite(&u, 2, 1, ff);
		}
		fclose(ff);
	}
	*/

	themax = 0;
	themaxind = -1;
	for (i = 0; i < hsz*hsz; i++) {
		if (themax < hough[i]) { // find peak in hough
			themaxind = i;
			themax = hough[i];
		}
	}

	ys = themaxind / hsz; // where is this best index in x,y space?
	xs = themaxind % hsz;

	printf("xshsz = %d, yshsz=%d\n", xs, ys); // FIXME logging

	*yshift = ((double)(themaxind / hsz) / (double)hsz) * (maxdy - mindy) + mindy;
	*xshift = ((double)(themaxind % hsz) / (double)hsz) * (maxdx - mindx) + mindx;
	printf("get_shift: mindx=%lf, maxdx=%lf, mindy=%lf, maxdy=%lf\n", mindx, maxdx, mindy, maxdy);
	printf("get_shift: xs=%lf, ys=%lf\n", *xshift, *yshift);

	/*
	  static char c = '1';
	  static char fn[] = "houghN.fits";
	  fn[5] = c++;
	  ezwriteimage(fn, TINT, hough, hsz, hsz);
	*/
	free(hough);
}

// Take shift in image plane and do a switcharoo to make the wcs something better
// in other words, take the shift in pixels and reset the WCS (in WCS coords)
// so that the new pixel shift would be zero
// FIXME -- dstn says, why not just
// sip_pixelxy2radec(wcs, crpix0 +- xs, crpix1 +- ys, wcs->wcstan.crval+0, wcs->wcstan.crval+1);

sip_t* wcs_shift(sip_t* wcs, double xs, double ys)
{
	// UNITS: crpix and xs/ys in pixels, crvals in degrees, nx/nyref and theta in degrees
	double crpix0, crpix1, crval0, crval1;
	double nxref, nyref, theta, sintheta, costheta;
	double newCD[2][2]; //the new CD matrix
	sip_t* swcs = malloc(sizeof(sip_t));
	memcpy(swcs, wcs, sizeof(sip_t));

	// Save
	crpix0 = wcs->wcstan.crpix[0]; // save old crpix
	crpix1 = wcs->wcstan.crpix[1];
	crval0 = wcs->wcstan.crval[0]; // save old crval
	crval1 = wcs->wcstan.crval[1];

	wcs->wcstan.crpix[0] += xs; // compute the desired projection of the new tangent point by
	wcs->wcstan.crpix[1] += ys; // shifting the projection of the current tangent point
	//fprintf(stderr,"wcs_shift: shifting crpix by (%g,%g)\n",xs,ys);

	// now reproject the old crpix[xy] into shifted wcs
	sip_pixelxy2radec(wcs, crpix0, crpix1, &nxref, &nyref);

	swcs->wcstan.crval[0] = nxref; // RA,DEC coords of new tangent point
	swcs->wcstan.crval[1] = nyref;
	theta = -deg2rad(nxref - crval0); // deltaRA = new minus old RA;
	theta *= sin(deg2rad(nyref));  // multiply by the sin of the NEW Dec; at equator this correctly evals to zero
	sintheta = sin(theta);
	costheta = cos(theta);
	//fprintf(stderr,"wcs_shift: crval0: %g->%g   crval1:%g->%g\ntheta=%g   (sintheta,costheta)=(%g,%g)\n",
	//	  crval0,nxref,crval1,nyref,theta,sintheta,costheta);

	// Restore
	wcs->wcstan.crpix[0] = crpix0; // restore old crpix
	wcs->wcstan.crpix[1] = crpix1;

	// Fix the CD matrix since "northwards" has changed due to moving RA
	newCD[0][0] = costheta * swcs->wcstan.cd[0][0] - sintheta * swcs->wcstan.cd[0][1];
	newCD[0][1] = sintheta * swcs->wcstan.cd[0][0] + costheta * swcs->wcstan.cd[0][1];
	newCD[1][0] = costheta * swcs->wcstan.cd[1][0] - sintheta * swcs->wcstan.cd[1][1];
	newCD[1][1] = sintheta * swcs->wcstan.cd[1][0] + costheta * swcs->wcstan.cd[1][1];
	swcs->wcstan.cd[0][0] = newCD[0][0];
	swcs->wcstan.cd[0][1] = newCD[0][1];
	swcs->wcstan.cd[1][0] = newCD[1][0];
	swcs->wcstan.cd[1][1] = newCD[1][1];

	// go into sanity_check and try this:
	// make something one sq. degree with DEC=89degrees and north up
	//make xy, convert to to RA/DEC
	//shift the WCS by .5 degrees of RA (give this in terms of pixels), use this new WCS to convert RA/DEC back to pixels
	//compare those pixels with pixels that have been shifted by .5 degrees worth of pixels to the left (in x direction only)

	return swcs;
}

sip_t* do_entire_shift_operation(tweak_t* t, double rho)
{
	sip_t* swcs;
	get_shift(t->x, t->y, t->n,
	          t->x_ref, t->y_ref, t->n_ref,
	          rho*t->mindx, rho*t->mindy, rho*t->maxdx, rho*t->maxdy,
	          &t->xs, &t->ys);
	swcs = wcs_shift(t->sip, t->xs, t->ys); // apply shift
	sip_free(t->sip);
	t->sip = swcs;
	printf("xshift=%lf, yshift=%lf\n", t->xs, t->ys);
	return NULL;
}


/* This function is intended only for initializing newly allocated tweak
 * structures, NOT for operating on existing ones.*/
void tweak_init(tweak_t* t)
{
	memset(t, 0, sizeof(tweak_t));
}

tweak_t* tweak_new()
{
	tweak_t* t = malloc(sizeof(tweak_t));
	tweak_init(t);
	return t;
}

void tweak_print4_fp(FILE* f, double x, double y,
                     double z, double w)
{
	fprintf(f, "%.15le ", x);
	fprintf(f, "%.15le ", y);
	fprintf(f, "%.15le ", z);
	fprintf(f, "%.15le ", w);
	fprintf(f, "\n");
}

void tweak_print5_fp(FILE* f, double x, double y,
                     double z, double w, double u)
{
	fprintf(f, "%.15le ", x);
	fprintf(f, "%.15le ", y);
	fprintf(f, "%.15le ", z);
	fprintf(f, "%.15le ", w);
	fprintf(f, "%.15le ", u);
	fprintf(f, "\n");
}

void tweak_print2_fp(FILE* f, double x, double y)
{
	fprintf(f, "%.15le ", x);
	fprintf(f, "%.15le ", y);
	fprintf(f, "\n");
}

void tweak_print4(char* fn, double* x, double* y,
                  double* z, double* w, int n)
{
	FILE* f = fopen(fn, "w");
	int i = 0;
	for (i = 0; i < n; i++)
		if (!z && !w)
			tweak_print2_fp(f, x[i], y[i]);
		else
			tweak_print4_fp(f, x[i], y[i], z[i], w[i]);
	fclose(f);
}

void tweak_print_the_state(unsigned int state)
{
	if (state & TWEAK_HAS_SIP )
		printf("TWEAK_HAS_SIP, ");
	if (state & TWEAK_HAS_IMAGE_XY )
		printf("TWEAK_HAS_IMAGE_XY, ");
	if (state & TWEAK_HAS_IMAGE_XYZ )
		printf("TWEAK_HAS_IMAGE_XYZ, ");
	if (state & TWEAK_HAS_IMAGE_AD )
		printf("TWEAK_HAS_IMAGE_AD, ");
	if (state & TWEAK_HAS_REF_XY )
		printf("TWEAK_HAS_REF_XY, ");
	if (state & TWEAK_HAS_REF_XYZ )
		printf("TWEAK_HAS_REF_XYZ, ");
	if (state & TWEAK_HAS_REF_AD )
		printf("TWEAK_HAS_REF_AD, ");
	if (state & TWEAK_HAS_AD_BAR_AND_R )
		printf("TWEAK_HAS_AD_BAR_AND_R, ");
	if (state & TWEAK_HAS_CORRESPONDENCES)
		printf("TWEAK_HAS_CORRESPONDENCES, ");
	if (state & TWEAK_HAS_RUN_OPT )
		printf("TWEAK_HAS_RUN_OPT, ");
	if (state & TWEAK_HAS_RUN_RANSAC_OPT )
		printf("TWEAK_HAS_RUN_RANSAC_OPT, ");
	if (state & TWEAK_HAS_COARSLY_SHIFTED)
		printf("TWEAK_HAS_COARSLY_SHIFTED, ");
	if (state & TWEAK_HAS_FINELY_SHIFTED )
		printf("TWEAK_HAS_FINELY_SHIFTED, ");
	if (state & TWEAK_HAS_REALLY_FINELY_SHIFTED )
		printf("TWEAK_HAS_REALLY_FINELY_SHIFTED, ");
	if (state & TWEAK_HAS_HEALPIX_PATH )
		printf("TWEAK_HAS_HEALPIX_PATH, ");
	if (state & TWEAK_HAS_LINEAR_CD )
		printf("TWEAK_HAS_LINEAR_CD, ");
}

void tweak_print_state(tweak_t* t)
{
	tweak_print_the_state(t->state);
}

#define BUFSZ 100 
//#define USE_FILE(x) snprintf(fn, BUFSZ, x "_%p_%d.dat", (void*)t->state, dump_nr)
#define USE_FILE(x) snprintf(fn, BUFSZ, x "_%03d.dat", dump_nr)
void tweak_dump_ascii(tweak_t* t)
{
	static int dump_nr = 0;
	char fn[BUFSZ];
	FILE* cor_im; // correspondences
	FILE* cor_ref;
	FILE* cor_delta;

	USE_FILE("scatter_image");
	tweak_print4(fn, t->x, t->y, t->a, t->d, t->n);

	if (t->state & TWEAK_HAS_REF_XY &&
	        t->state & TWEAK_HAS_REF_AD) {
		USE_FILE("scatter_ref");
		tweak_print4(fn, t->x_ref, t->y_ref, t->a_ref, t->d_ref, t->n_ref);
	}

	if (t->image) {
		int i;
		USE_FILE("corr_im");
		cor_im = fopen(fn, "w");
		USE_FILE("corr_ref");
		cor_ref = fopen(fn, "w");
		USE_FILE("corr_delta");
		cor_delta = fopen(fn, "w");
		for (i = 0; i < il_size(t->image); i++) {
			double a, d;
			int im_ind = il_get(t->image, i);
			sip_pixelxy2radec(t->sip, t->x[im_ind], t->y[im_ind], &a, &d);
			tweak_print4_fp(cor_im, t->x[im_ind], t->y[im_ind],
			                a, d);

			if (t->state & TWEAK_HAS_REF_XY) {
				int ref_ind = il_get(t->ref, i);
				tweak_print4_fp(cor_ref,
				                t->x_ref[ref_ind], t->y_ref[ref_ind],
				                t->a_ref[ref_ind], t->d_ref[ref_ind]);

				tweak_print5_fp(cor_delta,
						t->x[im_ind], t->y[im_ind],
						t->x_ref[ref_ind] - t->x[im_ind],
						t->y_ref[ref_ind] - t->y[im_ind],
						il_get(t->included, i));
			}

		}
		fclose(cor_im);
		fclose(cor_ref);
		fclose(cor_delta);
	}
	printf("dump=%d\n", dump_nr);
	dump_nr++;
}
void get_center_and_radius(double* ra, double* dec, int n,
                           double* ra_mean, double* dec_mean, double* radius)
{
	double* xyz = malloc(3 * n * sizeof(double));
	double xyz_mean[3] = {0, 0, 0};
	double maxdist2 = 0;
	int maxind = -1;
	int i, j;

	for (i = 0; i < n; i++) {
		radec2xyzarr(deg2rad(ra[i]),
		             deg2rad(dec[i]),
		             xyz + 3*i);
	}

	for (i = 0; i < n; i++)  // dumb average
		for (j = 0; j < 3; j++)
			xyz_mean[j] += xyz[3 * i + j];

	normalize_3(xyz_mean);

	for (i = 0; i < n; i++) { // find largest distance from average
		double dist2 = distsq(xyz_mean, xyz + 3*i, 3);
		if (maxdist2 < dist2) {
			maxdist2 = dist2;
			maxind = i;
		}
	}
	*radius = sqrt(maxdist2);
	xyzarr2radecdeg(xyz_mean, ra_mean, dec_mean);
	free(xyz);
}

void tweak_clear_correspondences(tweak_t* t)
{
	if (t->state & TWEAK_HAS_CORRESPONDENCES) {
		// our correspondences are also now toast
		assert(t->image);
		assert(t->ref);
		assert(t->dist2);
		il_remove_all(t->image);
		il_remove_all(t->ref);
		dl_remove_all(t->dist2);
		il_free(t->image);
		il_free(t->ref);
		dl_free(t->dist2);
		if (t->weight)
			dl_free(t->weight);
		t->image = NULL;
		t->ref = NULL;
		t->dist2 = NULL;
		t->weight = NULL;
		t->state &= ~TWEAK_HAS_CORRESPONDENCES;
	} else {
		assert(!t->image);
		assert(!t->ref);
		assert(!t->dist2);
		assert(!t->weight);
	}
	assert(!t->image);
	assert(!t->ref);
	assert(!t->dist2);
	assert(!t->weight);
}

void tweak_clear_on_sip_change(tweak_t* t)
{
//	tweak_clear_correspondences(t);
	tweak_clear_image_ad(t);
	tweak_clear_ref_xy(t);
	tweak_clear_image_xyz(t);

}

void tweak_clear_ref_xy(tweak_t* t)  // ref_xy are the catalog star positions in image coordinates
{
	if (t->state & TWEAK_HAS_REF_XY)
	{
		assert(t->x_ref);
		free(t->x_ref);
		assert(t->y_ref);
		t->x_ref = NULL;
		free(t->y_ref);
		t->y_ref = NULL;
		t->state &= ~TWEAK_HAS_REF_XY;

	} else
	{
		assert(!t->x_ref);
		assert(!t->y_ref);
	}
}

void tweak_clear_ref_ad(tweak_t* t) // radec of catalog stars
{
	if (t->state & TWEAK_HAS_REF_AD)
	{
		assert(t->a_ref);
		free(t->a_ref);
		t->a_ref = NULL;
		assert(t->d_ref);
		free(t->d_ref);
		t->d_ref = NULL;
		t->n_ref = 0;

		tweak_clear_correspondences(t);
		tweak_clear_ref_xy(t);
		t->state &= ~TWEAK_HAS_REF_AD;

	} else
	{
		assert(!t->a_ref);
		assert(!t->d_ref);
	}
}

void tweak_clear_image_ad(tweak_t* t) // source (image) objs in ra,dec according to current tweak
{
	if (t->state & TWEAK_HAS_IMAGE_AD)
	{
		assert(t->a);
		free(t->a);
		t->a = NULL;
		assert(t->d);
		free(t->d);
		t->d = NULL;

		t->state &= ~TWEAK_HAS_IMAGE_AD;

	} else
	{
		assert(!t->a);
		assert(!t->d);
	}
}

void tweak_clear_image_xyz(tweak_t* t)
{
	if (t->state & TWEAK_HAS_IMAGE_XYZ) {
		assert(t->xyz);
		free(t->xyz);
		t->xyz = NULL;
		t->state &= ~TWEAK_HAS_IMAGE_XYZ;

	} else {
		assert(!t->xyz);
	}
}

void tweak_clear_image_xy(tweak_t* t)
{
	if (t->state & TWEAK_HAS_IMAGE_XY) {
		assert(t->x);
		free(t->x);
		t->x = NULL;
		assert(t->y);
		free(t->y);
		t->y = NULL;

		t->state &= ~TWEAK_HAS_IMAGE_XY;

	} else {
		assert(!t->x);
		assert(!t->y);
	}
}

void tweak_push_ref_ad(tweak_t* t, double* a, double *d, int n) // tell us (from outside tweak) where the catalog stars are
{
	tweak_clear_ref_ad(t);

	assert(a);
	assert(d);
	assert(n);

	assert(!t->a_ref); // no leaky
	assert(!t->d_ref); // no leaky
	t->a_ref = malloc(sizeof(double) * n);
	t->d_ref = malloc(sizeof(double) * n);
	memcpy(t->a_ref, a, n*sizeof(double));
	memcpy(t->d_ref, d, n*sizeof(double));

	t->n_ref = n;

	t->state |= TWEAK_HAS_REF_AD;
}

void tweak_ref_find_xyz_from_ad(tweak_t* t) // tell us (from outside tweak) where the catalog stars are
{

	assert(t->state & TWEAK_HAS_REF_AD);

	assert(!t->xyz_ref); // no leaky
	t->xyz_ref = malloc(sizeof(double) * 3 * t->n_ref);

	int i;
	for (i = 0; i < t->n_ref; i++)
	{ // fill em up
		double *pt = t->xyz_ref + 3 * i;
		radecdeg2xyzarr(t->a_ref[i], t->d_ref[i], pt);
	}

	t->state |= TWEAK_HAS_REF_XYZ;
}

void tweak_push_ref_xyz(tweak_t* t, double* xyz, int n) // tell us (from outside tweak) where the catalog stars are
{
	double *ra, *dec;
	int i;

	tweak_clear_ref_ad(t);

	assert(xyz);
	assert(n);

	assert(!t->xyz_ref); // no leaky
	t->xyz_ref = malloc(sizeof(double) * 3 * n);
	memcpy(t->xyz_ref, xyz, 3*n*sizeof(double));

	ra = malloc(sizeof(double) * n);
	dec = malloc(sizeof(double) * n);
	assert(ra);
	assert(dec);

	for (i = 0; i < n; i++)
	{ // fill em up
		double *pt = xyz + 3 * i;
		xyzarr2radecdeg(pt, ra+i, dec+i);
	}

	t->a_ref = ra;
	t->d_ref = dec;
	t->n_ref = n;

	t->state |= TWEAK_HAS_REF_AD;
	t->state |= TWEAK_HAS_REF_XYZ;
}

void tweak_push_image_xy(tweak_t* t, double* x, double *y, int n) // tell us the input dude
{
	tweak_clear_image_xy(t);

	assert(n);

	t->x = malloc(sizeof(double) * n);
	t->y = malloc(sizeof(double) * n);
	memcpy(t->x, x, sizeof(double)*n);
	memcpy(t->y, y, sizeof(double)*n);
	t->n = n;

	t->state |= TWEAK_HAS_IMAGE_XY;
}

void tweak_push_hppath(tweak_t* t, char* hppath) //healpix path
{
	t->hppath = strdup(hppath);
	t->state |= TWEAK_HAS_HEALPIX_PATH;
}

void tweak_skip_shift(tweak_t* t)
{
	t->state |= (TWEAK_HAS_COARSLY_SHIFTED | TWEAK_HAS_FINELY_SHIFTED |
	             TWEAK_HAS_REALLY_FINELY_SHIFTED);
}

// DualTree RangeSearch callback. We want to keep track of correspondences.
// Potentially the matching could be many-to-many; we allow this and hope the
// optimizer can take care of it.
void dtrs_match_callback(void* extra, int image_ind, int ref_ind, double dist2)
{
	tweak_t* t = extra;

	image_ind = t->kd_image->perm[image_ind];
	ref_ind = t->kd_ref->perm[ref_ind];

	//	double dx = t->x[image_ind] - t->x_ref[ref_ind];
	//	double dy = t->y[image_ind] - t->y_ref[ref_ind];

	//	fprintf(stderr,"found new one!: dx=%lf, dy=%lf, dist=%lf\n", dx,dy,sqrt(dx*dx+dy*dy));
	il_append(t->image, image_ind);
	il_append(t->ref, ref_ind);
	dl_append(t->dist2, dist2);
	il_append(t->included, 1);

	if (t->weight)
		dl_append(t->weight, exp(-dist2 / (2.0 * t->jitterd2)));

}

// Dualtree rangesearch callback for distance calc. this should be integrated
// with real dualtree rangesearch.
double dtrs_dist2_callback(void* p1, void* p2, int D)
{
	double* pp1 = p1, *pp2 = p2;
	return distsq(pp1, pp2, D);
}

// The jitter is in radians
void find_correspondences(tweak_t* t, double jitter)  // actually call the dualtree
{
	double dist;
	double* data_image = malloc(sizeof(double) * t->n * 3);
	double* data_ref = malloc(sizeof(double) * t->n_ref * 3);

	assert(t->state & TWEAK_HAS_IMAGE_XYZ);
	assert(t->state & TWEAK_HAS_REF_XYZ);
	tweak_clear_correspondences(t);

	memcpy(data_image, t->xyz, 3*t->n*sizeof(double));
	memcpy(data_ref, t->xyz_ref, 3*t->n_ref*sizeof(double));

	t->kd_image = kdtree_build(NULL, data_image, t->n, 3, 4, KDTT_DOUBLE,
	                           KD_BUILD_BBOX);

	t->kd_ref = kdtree_build(NULL, data_ref, t->n_ref, 3, 4, KDTT_DOUBLE,
	                         KD_BUILD_BBOX);

	// Storage for correspondences
	t->image = il_new(600);
	t->ref = il_new(600);
	t->dist2 = dl_new(600);
	if (t->weighted_fit)
		t->weight = dl_new(600);
	t->included = il_new(600);

	printf("search radius = %g arcsec / %g arcmin / %g deg\n",
	        rad2arcsec(jitter), rad2arcmin(jitter), rad2deg(jitter));

	dist = rad2dist(jitter);
	printf("distance on the unit sphere: %g\n", dist);

	// Find closest neighbours
	dualtree_rangesearch(t->kd_image, t->kd_ref,
	                     RANGESEARCH_NO_LIMIT, dist,
	                     dtrs_dist2_callback,   // specify callback as the above func
	                     dtrs_match_callback, t,
	                     NULL, NULL);

	kdtree_free(t->kd_image);
	kdtree_free(t->kd_ref);
	t->kd_image = NULL;
	t->kd_ref = NULL;
	free(data_image);
	free(data_ref);

	printf("correspondences=%d\n", dl_size(t->dist2));

	// find objs with multiple correspondences.
	/*{
	  int* nci;
	  int* ncr;
	  int i;
	  nci = calloc(t->n, sizeof(int));
	  ncr = calloc(t->n_ref, sizeof(int));
	  for (i=0; i<il_size(t->image); i++) {
	  nci[il_get(t->image, i)]++;
	  ncr[il_get(t->ref, i)]++;
	  }
	  for (i=0; i<t->n; i++) {
	  if (nci[i] > 1) {
	  printf("Image object %i matched %i reference objs.\n",
	  i, nci[i]);
	  }
	  }
	  for (i=0; i<t->n_ref; i++) {
	  if (ncr[i] > 1) {
	  printf("Reference object %i matched %i image objs.\n",
	  i, ncr[i]);
	  }
	  }
	  free(nci);
	  free(ncr);
	  }*/

}


kdtree_t* cached_kd = NULL;
int cached_kd_hp = 0;

void get_reference_stars(tweak_t* t) // use healpix technology to go get ref stars if they aren't passed from outside
{
	double ra_mean = t->a_bar;
	double dec_mean = t->d_bar;
	double radius = t->radius;
	double radius_factor;
	double xyz[3];
	kdtree_qres_t* kq;

	int hp = radecdegtohealpix(ra_mean, dec_mean, t->Nside);
	kdtree_t* kd;
	if (cached_kd_hp != hp || cached_kd == NULL)
	{
		char buf[1000];
		snprintf(buf, 1000, t->hppath, hp);
		printf("opening %s\n", buf);
		kd = kdtree_fits_read(buf, NULL);
		printf("success\n");
		assert(kd);
		cached_kd_hp = hp;
		cached_kd = kd;
	} else
	{
		kd = cached_kd;
	}

	radecdeg2xyzarr(ra_mean, dec_mean, xyz);
	//radec2xyzarr(deg2rad(158.70829), deg2rad(51.919442), xyz);

	// Fudge radius factor because if the shift is really big, then we
	// can't actually find the correct astrometry.
	radius_factor = 1.3;
	kq = kdtree_rangesearch(kd, xyz, radius * radius * radius_factor);
	printf("Did range search got %u stars\n", kq->nres);

	// No stars? That's bad. Run away.
	if (!kq->nres)
	{
		printf("Bad news. tweak_internal: Got no stars in get_reference_stars\n");
		return ;
	}
	tweak_push_ref_xyz(t, kq->results.d, kq->nres);

	kdtree_free_query(kq);
}

// in arcseconds^2 on the sky (chi-sq)
double figure_of_merit(tweak_t* t, double *rmsX, double *rmsY)
{
	// works on the sky
	double sqerr = 0.0;
//	double sqerr_included = 0.0;
	int i;
	for (i = 0; i < il_size(t->image); i++) { // t->image is a list of source objs with current ref correspondences
		double a, d;
		double xyzpt[3];
		double xyzpt_ref[3];
		double xyzerr[3];
		sip_pixelxy2radec(t->sip, t->x[il_get(t->image, i)],
		                  t->y[il_get(t->image, i)], &a, &d);

		// xref and yref should be intermediate WC's not image x and y!
		radecdeg2xyzarr(a, d, xyzpt);
		radecdeg2xyzarr(t->a_ref[il_get(t->ref, i)],   // t->ref is a list of ref objs with current source correspn.
		                t->d_ref[il_get(t->ref, i)], xyzpt_ref);

		xyzerr[0] = xyzpt[0] - xyzpt_ref[0];
		xyzerr[1] = xyzpt[1] - xyzpt_ref[1];
		xyzerr[2] = xyzpt[2] - xyzpt_ref[2];
		if (il_get(t->included, i)) 
			sqerr += xyzerr[0] * xyzerr[0] + xyzerr[1] * xyzerr[1] + xyzerr[2] * xyzerr[2];
//			sqerr_included += xyzerr[0] * xyzerr[0] + xyzerr[1] * xyzerr[1] + xyzerr[2] * xyzerr[2];
//			*rmsX += xyzerr[0] * xyzerr[0]
//			*rmsY += xyzerr[1] * xyzerr[1] + xyzerr[2] * xyzerr[2];
	}
	return rad2arcsec(1)*rad2arcsec(1)*sqerr;
}

double correspondences_rms_arcsec(tweak_t* t, int weighted) {
	double err2 = 0.0;
	int i;
	double totalweight = 0.0;
	for (i=0; i<il_size(t->image); i++) {
		double xyzpt[3];
		double xyzpt_ref[3];
		double weight;

		if (!il_get(t->included, i))
			continue;

		if (weighted && t->weight) {
			weight = dl_get(t->weight, i);
		} else {
			weight = 1.0;
		}
		totalweight += weight;

		sip_pixelxy2xyzarr(t->sip,
						   t->x[il_get(t->image, i)],
						   t->y[il_get(t->image, i)],
						   xyzpt);

		radecdeg2xyzarr(t->a_ref[il_get(t->ref, i)],
		                t->d_ref[il_get(t->ref, i)],
						xyzpt_ref);

		err2 += weight * distsq(xyzpt, xyzpt_ref, 3);
	}
	return distsq2arcsec( err2 / totalweight );
}

// in pixels^2 in the image
double figure_of_merit2(tweak_t* t)
{
	// works on the pixel coordinates
	double sqerr = 0.0;
	int i;
	for (i = 0; i < il_size(t->image); i++) {
		double x, y, dx, dy;
		sip_radec2pixelxy(t->sip, t->a_ref[il_get(t->ref, i)], t->d_ref[il_get(t->ref, i)], &x, &y);
		dx = t->x[il_get(t->image, i)] - x;
		dy = t->y[il_get(t->image, i)] - y;
		sqerr += dx * dx + dy * dy;
	}
	return 3600*3600*sqerr*fabs(sip_det_cd(t->sip)); // convert this to units of arcseconds^2, sketchy
}

// I apologize for the rampant copying and pasting of the polynomial calcs...
void invert_sip_polynomial(tweak_t* t)
{
	// basic idea: lay down a grid in image, for each gridpoint, push through the polynomial
	// to get yourself into warped image coordinate (but not yet lifted onto the sky)
	// then, using the set of warped gridpoints as inputs, fit back to their original grid locations as targets

	int inv_sip_order, ngrid, inv_sip_coeffs, stride;
	double *A, *A2, *b, *b2;
	double maxu, maxv, minu, minv;
	int i, gu, gv;

	assert(t->sip->a_order == t->sip->b_order);
	assert(t->sip->ap_order == t->sip->bp_order);

	inv_sip_order = t->sip->ap_order;

	ngrid = 10 * (t->sip->ap_order + 1);
	printf("tweak inversion using %u gridpoints\n", ngrid);

	// The SIP coefficients form a order x order upper triangular matrix missing
	// the 0,0 element. We limit ourselves to a order 10 SIP distortion.
	// That's why the computation of the number of SIP terms is calculated by
	// Gauss's arithmetic sum: sip_terms = (sip_order+1)*(sip_order+2)/2
	// The in the end, we drop the p^0q^0 term by integrating it into crpix)
	inv_sip_coeffs = (inv_sip_order + 1) * (inv_sip_order + 2) / 2; // upper triangle

	stride = ngrid * ngrid; // Number of rows
	A = malloc(inv_sip_coeffs * stride * sizeof(double));
	b = malloc(2 * stride * sizeof(double));
	A2 = malloc(inv_sip_coeffs * stride * sizeof(double));
	b2 = malloc(2 * stride * sizeof(double));
	assert(A);
	assert(b);
	assert(A2);
	assert(b2);

	// Rearranging formula (4), (5), and (6) from the SIP paper gives the
	// following equations:
	//
	//   +----------------------- Linear pixel coordinates in PIXELS
	//   |                        before SIP correction
	//   |                   +--- Intermediate world coordinates in DEGREES
	//   |                   |
	//   v                   v
	//                  -1
	//   U = [CD11 CD12]   * x
	//   V   [CD21 CD22]     y
	//
	//   +---------------- PIXEL distortion delta from telescope to
	//   |                 linear coordinates
	//   |    +----------- Linear PIXEL coordinates before SIP correction
	//   |    |       +--- Polynomial U,V terms in powers of PIXELS
	//   v    v       v
	//
	//   -f(u1,v1) =  p11 p12 p13 p14 p15 ... * ap1
	//   -f(u2,v2) =  p21 p22 p23 p24 p25 ...   ap2
	//   ...
	//
	//   -g(u1,v1) =  p11 p12 p13 p14 p15 ... * bp1
	//   -g(u2,v2) =  p21 p22 p23 p24 p25 ...   bp2
	//   ...
	//
	// which recovers the A and B's.

	// Find image boundaries
	minu = minv = 1e100;
	maxu = maxv = -1e100;

	for (i = 0; i < t->n; i++) {
		minu = dblmin(minu, t->x[i] - t->sip->wcstan.crpix[0]);
		minv = dblmin(minv, t->y[i] - t->sip->wcstan.crpix[1]);
		maxu = dblmax(maxu, t->x[i] - t->sip->wcstan.crpix[0]);
		maxv = dblmax(maxv, t->y[i] - t->sip->wcstan.crpix[1]);
	}
	printf("maxu=%lf, minu=%lf\n", maxu, minu);
	printf("maxv=%lf, minv=%lf\n", maxv, minv);

	// Fill A in column-major order for fortran dgelsd
	// We just make a big grid and hope for the best
	i = 0;
	for (gu = 0; gu < ngrid; gu++) {
		for (gv = 0; gv < ngrid; gv++) {

			// Calculate grid position in original image pixels
			double u = (gu * (maxu - minu) / ngrid) + minu; // now in pixels
			double v = (gv * (maxv - minv) / ngrid) + minv;  // now in pixels

			double U, V, fuv, guv;
			int p, q, j;
			sip_calc_distortion(t->sip, u, v, &U, &V); // computes U=u+f(u,v) and V=v+g(u,v)
			fuv = U - u;
			guv = V - v;

			// Calculate polynomial terms but this time for inverse
			// j = 0;
			/*
			Maybe we want to explicitly set the (0,0) term to zero...:
			*/
			A[i + stride*0] = 0;
			j = 1;
			for (p = 0; p <= inv_sip_order; p++)
				for (q = 0; q <= inv_sip_order; q++)
					if (p + q <= inv_sip_order && !(p == 0 && q == 0)) {
						// we're skipping the (0, 0) term.
						//assert(j < (inv_sip_coeffs-1));
						assert(j < inv_sip_coeffs);
						A[i + stride*j] = pow(U, (double)p) * pow(V, (double)q);
						j++;
					}
			//assert(j == (inv_sip_coeffs-1));
			assert(j == inv_sip_coeffs);

			b[i] = -fuv;
			b[i + stride] = -guv;
			i++;
		}
	}

	// Save for sanity check
	memcpy(A2, A, sizeof(double)*stride*inv_sip_coeffs);
	memcpy(b2, b, sizeof(double)*stride*2);

	// Allocate work areas and answers
	{
		integer N = inv_sip_coeffs;          // nr cols of A
		doublereal S[N];      // min(N,M); is singular vals of A in dec order
		doublereal RCOND = -1.; // used to determine effective rank of A; -1
		// means use machine precision
		integer NRHS = 2;       // number of right hand sides (one for x and y)
		integer rank;         // output effective rank of A
		integer lwork = 1000 * 1000; /// FIXME ???
		doublereal* work = malloc(lwork * sizeof(double));
		integer *iwork = malloc(lwork * sizeof(long int));
		integer info;
		int p, q, j;
		double chisq;
		integer str = stride;
		integer lda = stride;
		integer ldb = stride;
//		printf("INVERSE DGELSD---- ldb=%d\n", ldb);
		dgelsd_(&str, &N, &NRHS, A, &lda, b, &ldb, S, &RCOND, &rank, work,
		        &lwork, iwork, &info); // make the jump to lightspeed
		stride = str;
		free(work);
		free(iwork);

		// Extract the inverted SIP coefficients
		//j = 0;
		j = 1;
		for (p = 0; p <= inv_sip_order; p++)
			for (q = 0; q <= inv_sip_order; q++)
				if (p + q <= inv_sip_order && !(p == 0 && q == 0))
				{
					assert(j < inv_sip_coeffs);
					t->sip->ap[p][q] = b[j]; // dgelsd in its wisdom, stores its answer by overwriting your input b (nice!)
					t->sip->bp[p][q] = b[stride + j];
					j++;
				}
		assert(j == inv_sip_coeffs);

		// Calculate chi2 for sanity
		chisq = 0;
		for (i = 0; i < stride; i++)
		{
			double sum = 0;
			int j2;
			for (j2 = 0; j2 < inv_sip_coeffs; j2++)
				sum += A2[i + stride * j2] * b[j2];
			chisq += (sum - b2[i]) * (sum - b2[i]);
			sum = 0;
			for (j2 = 0; j2 < inv_sip_coeffs; j2++)
				sum += A2[i + stride * j2] * b[j2 + stride];
			chisq += (sum - b2[i + stride]) * (sum - b2[i + stride]);
		}
		printf("sip_invert_chisq=%lf\n", chisq);
		printf("sip_invert_chisq/%d=%lf\n", ngrid*ngrid, (chisq / ngrid) / ngrid);
		printf("sip_invert_sqrt(chisq/%d=%lf)\n", ngrid*ngrid, sqrt((chisq / ngrid) / ngrid));
	}

	free(A);
	free(b);
	free(A2);
	free(b2);
}



// FIXME: adapt this function to take as input the correspondences to use VVVVV
//    wic is World Intermediate Coordinates, either along ra or dec
//       i.e. canonical image coordinates
//    wic_corr is the list of corresponding indexes for wip
//    pix_corr is the list of corresponding indexes for pixels
//    pix is raw image pixels (either u or v)
//    siporder is the sip order up to MAXORDER (defined in sip.h)
//    the correspondences are passed so that we can stick RANSAC around the whole
//    thing for better estimation.

#define set(A, i, j)  ((A)[(i) + ((stride)*(j))])
#define get(A, i, j)  ((A)[(i) + ((stride)*(j))])

// Run a polynomial tweak
void do_sip_tweak(tweak_t* t) // bad name for this function
{
	int sip_order, sip_coeffs, stride;
	double *UVP, *UVP2, *B, *B2;
	double *X;
	double xyzcrval[3];
	double cdinv[2][2];
	double sx, sy, sU, sV, su, sv;
	double chisq;
	sip_t* swcs;
	int M, N, R;
	int i, j, p, q, order;

	// a_order and b_order should be the same!
	assert(t->sip->a_order == t->sip->b_order);
	sip_order = t->sip->a_order;

	// We need at least the linear terms to compute CD.
	if (sip_order < 1)
		sip_order = 1;

	// The SIP coefficients form an (order x order) upper triangular matrix missing
	// the 0,0 element. We limit ourselves to an order 10 SIP distortion.
	// That's why the number of SIP terms is calculated by
	// Gauss's arithmetic sum: sip_terms = (sip_order+1)*(sip_order+2)/2
	// Then in the end, we drop the p^0q^0 term by integrating it into crpix)
	sip_coeffs = (sip_order + 1) * (sip_order + 2) / 2; // upper triangle

	/* calculate how many points to use based on t->include */
	stride = 0;
	for (i = 0; i < il_size(t->included); i++)
	{
		if (il_get(t->included, i))
			stride++;
	}
	assert(il_size(t->included) == il_size(t->image));
	//	stride = il_size(t->image); // number of rows

	M = stride;
	N = sip_coeffs;
	R = 2;

	assert(M >= N);

	UVP  = malloc(N * M * sizeof(double));
	B    = malloc(M * R * sizeof(double));
	UVP2 = malloc(N * M * sizeof(double));
	B2   = malloc(M * R * sizeof(double));
	assert(UVP);
	assert(B);
	assert(UVP2);
	assert(B2);

	//printf("sqerr=%le [arcsec^2]\n", figure_of_merit(t,NULL,NULL));
	printf("do_sip_tweak starting.\n");
	sip_print_to(t->sip, stdout);
	//	fprintf(stderr,"sqerrxy=%le\n", figure_of_merit2(t));

	printf("RMS error of correspondences: %g arcsec\n",
		   correspondences_rms_arcsec(t, 0));
	printf("Weighted RMS error of correspondences: %g arcsec\n",
		   correspondences_rms_arcsec(t, 1));

	/*
	*  We use a clever trick to estimate CD, A, and B terms in two
	*  seperated least squares fits, then finding A and B by multiplying
	*  the found parameters by CD inverse.
	* 
	*  Rearranging the SIP equations (see sip.h) we get the following
	*  matrix operation to compute x and y in world intermediate
	*  coordinates, which is convienently written in a way which allows
	*  least squares estimation of CD and terms related to A and B.
	* 
	*  First use the x's to find the first set of parametetrs
	* 
	*     +--------------------- Intermediate world coordinates in DEGREES
	*     |          +--------- Pixel coordinates u and v in PIXELS
	*     |          |     +--- Polynomial u,v terms in powers of PIXELS
	*     v          v     v
	*   ( x1 )   ( 1 u1 v1 p1 )   (sx              )
	*   ( x2 ) = ( 1 u2 v2 p2 ) * (cd11            ) : cd11 is a scalar, degrees per pixel
	*   ( x3 )   ( 1 u3 v3 p3 )   (cd12            ) : cd12 is a scalar, degrees per pixel
	*   ( ...)   (   ...    )     (cd11*A + cd12*B ) : cd11*A and cs12*B are mixture of SIP terms (A,B) and CD matrix (cd11,cd12)
	* 
	*  Then find cd21 and cd22 with the y's
	* 
	*   ( y1 )   ( 1 u1 v1 p1 )   (sy              )
	*   ( y2 ) = ( 1 u2 v2 p2 ) * (cd21            ) : scalar, degrees per pixel
	*   ( y3 )   ( 1 u3 v3 p3 )   (cd22            ) : scalar, degrees per pixel
	*   ( ...)   (   ...    )     (cd21*A + cd22*B ) : mixture of SIP terms (A,B) and CD matrix (cd21,cd22)
	* 
	*  These are both standard least squares problems which we solve by
	*  netlib's dgelsd. i.e. min_{cd,A,B} || x - [1,u,v,p]*[s;cd;cdA+cdB]||^2 with
	*  x reference, cd,A,B unrolled parameters.
	* 
	*  after the call to dgelsd, we get back (for x) a vector of optimal
	*    [sx;cd11;cd12; cd11*A + cd12*B]
	*  Now we can pull out sx, cd11 and cd12 from the beginning of this vector,
	*  and call the rest of the vector [cd11*A] + [cd12*B];
	*  similarly for the y fit, we get back a vector of optimal
	*    [sy;cd21;cd22; cd21*A + cd22*B]
	*  once we have all those we can figure out A and B as follows
	*                   -1
	*    A' = [cd11 cd12]    *  [cd11*A' + cd12*B']
	*    B'   [cd21 cd22]       [cd21*A' + cd22*B']
	* 
	*  which recovers the A and B's.
	*
	*/

	/*
	* 
	*  DGESLD allows simultaneous fits ("multiple right hand sides") as follows.
	* 
	*  Normally you want to solve:
	* 
	*     min || b[M-by-1] - A[M-by-N] x[N-by-1] ||_2
	* 
	*  With multiple right-hand sides, this becomes:
	* 
	*     min || B[M-by-R] - A[M-by-N] X[N-by-R] ||_2
	* 
	*  We are solving the "x" and "y" values as two different "right-hand sides",
	*  so:
	* 
	*  R = 2
	*  M = the number of correspondences.
	*  N = the number of SIP terms.
	*
	* And we want an overdetermined system, so M >= N.
	* 
	* 
	*  Since the name "A" is already used to refer to one set of SIP
	*  coefficients, we use the variable "UVP" instead:
	*
	*           [ 1  u1   v1  u1^2  u1v1  v1^2  ... ]
	*    UVP =  [ 1  u2   v2  u2^2  u2v2  v2^2  ... ]
	*           [           ......                  ]
	*
	*  where [pI] are the set of SIP polynomial terms at (uI,vI).
	*  
	*        [ x1   y1 ]
	*    B = [ x2   y2 ]
	*        [   ...   ]
	* 
	*  And the answer X is:
	*        [ sx                  sy                ]
	*        [ cd11                cd21              ]
	*    X = [ cd12                cd22              ]
	*        [ [cd11*A + cd12*B]   [cd21*A + cd22*B] ]
	* 
	*  (where A and B are actually tall vectors)
	*
	*
	*
	*  We are using Fortran DGELSD to solve this equation, so the matrices have
	*  to be in crazy Fortran order.
	*
	* DGELSD takes A and B as inputs, and trashes A and puts the answer in B.
	* (Isn't Fortran great.)  It also takes as arguments LDA and LDB, which are
	* the "leading dimensions" of A and B.  LDA >= M, and LDB >= max(M, N).
	*
	* For us, LDA = M and LDB = M.
	*
	* Because Fortran stores its matrices sideways, 
	*
	*     A(r,c) is at  A + c*LDA + r
	*
	* So for the input/output matrix B (which has size M-by-R on input and
	* N-by-R on output) it sticks the output in
	*
	*     X([0:M-1], [0:R-1])  ->   B + [0:R-1]*LDB + [0:M-1]
	*
	*/


	// fill in the UVP matrix, stride in this case is the number of correspondences
	radecdeg2xyzarr(t->sip->wcstan.crval[0], t->sip->wcstan.crval[1], xyzcrval);
	int row;
	double totalweight = 0.0;
	i = -1;
	for (row = 0; row < il_size(t->included); row++)
	{
		int refi;
		double x, y;
		double xyzpt[3];
		double weight = 1.0;
		double u;
		double v;

		if (!il_get(t->included, row)) {
			continue;
		}
		i++;

		assert(i >= 0);
		assert(i < M);

		u = t->x[il_get(t->image, i)] - t->sip->wcstan.crpix[0];
		v = t->y[il_get(t->image, i)] - t->sip->wcstan.crpix[1];

		if (t->weighted_fit) {
			//double dist2 = dl_get(t->dist2, i);
			//exp(-dist2 / (2.0 * dist2sigma));
			weight = dl_get(t->weight, i);
			assert(weight >= 0.0);
			assert(weight <= 1.0);
			totalweight += weight;
		}

		j = 0;
		for (order=0; order<=sip_order; order++) {
			for (q=0; q<=order; q++) {
				p = order - q;

				assert(j >= 0);
				assert(j < N);
				assert(p >= 0);
				assert(q >= 0);
				assert(p + q <= sip_order);

				set(UVP, i, j) = weight * pow(u, (double)p) * pow(v, (double)q);
				j++;
			}
		}
		assert(j == N);

		//  p q
		// (0,0) = 1     <- order 0
		// (1,0) = u     <- order 1
		// (0,1) = v
		// (2,0) = u^2   <- order 2
		// (1,1) = uv
		// (0,2) = v^2
		// ...

		// The shift - aka (0,0) - SIP coefficient must be 1.
		assert(get(UVP, i, 0) == 1.0 * weight);
		// The linear terms.
		//assert(get(UVP, i, 1) == u * weight);
		//assert(get(UVP, i, 2) == v * weight);
		assert(fabs(get(UVP, i, 1) - u * weight) < 1e-12);
		assert(fabs(get(UVP, i, 2) - v * weight) < 1e-12);

		// B contains Intermediate World Coordinates (in degrees)
		refi = il_get(t->ref, i);
		radecdeg2xyzarr(t->a_ref[refi], t->d_ref[refi], xyzpt);
		star_coords(xyzpt, xyzcrval, &y, &x); // tangent-plane projection
		set(B, i, 0) = weight * rad2deg(x);
		set(B, i, 1) = weight * rad2deg(y);
	}
	assert(i + 1 == M);

	if (t->weighted_fit)
		printf("Total weight: %g\n", totalweight);

	// Save UVP and B for computing chisq
	memcpy(UVP2, UVP, M * N * sizeof(double));
	memcpy(B2,   B,   M * R * sizeof(double));

	if (0) {
		int r, c;
		printf("uvp=array([\n");
		for (r=0; r<M; r++) {
			printf("[");
			for (c=0; c<N; c++) {
				printf("%g,", get(UVP, r, c));
			}
			printf("],\n");
		}
		printf("])\n");

		printf("b=array([\n");
		for (r=0; r<M; r++) {
			printf("[");
			// (c<R looks wrong but it isn't.)
			for (c=0; c<R; c++) {
				printf("%g,", get(B, r, c));
			}
			printf("],\n");
		}
		printf("])\n");
	}

	{
		integer NN = N;
		integer MM = M;
		integer LDA = M;
		integer LDB = M;
		// number of right hand sides.
		integer NRHS = R;
		// min(N,M); is singular vals of UVP in descending order
		doublereal S[NN];
		// used to determine effective rank of UVP; -1 means use machine precision
		doublereal RCOND = -1.;
		// output: effective rank of UVP
		integer rank;
		/// FIXME ???
		integer lwork = 1000 * 1000;
		doublereal *work = malloc(lwork * sizeof(doublereal));
		integer   *iwork = malloc(lwork * sizeof(integer));
		integer info;
		dgelsd_(&MM, &NN, &NRHS, UVP, &LDA, B, &LDB, S, &RCOND, &rank, work,
		        &lwork, iwork, &info); // punch it chewey
		free(work);
		free(iwork);
	}

	// dgelsd trashes UVP and B, and places the answer in B.
	// We rename it X for clarity.
	X = B;

	// Print rms(AX - B)
	{
		int i,j,k;
		double rms=0;
		for (j=0; j<M; j++) {
			for (k=0; k<2; k++) {
				double acc = 0.0;
				for (i=0; i<N; i++)
					acc += get(UVP2, j, i) * get(X, i, k);
				acc -= get(B2, j, k);
				rms += square(acc);
			}
		}
		rms = sqrt(rms / (double)(M*2));

		printf("rms(AX-B) = %g\n", rms);
	}

	// Do it again, only more different.
	{
		gsl_matrix* mA;
		gsl_matrix* QR;
		gsl_vector* tau;
		gsl_vector* b1;
		gsl_vector* b2;
		gsl_vector* resid1;
		gsl_vector* resid2;
		gsl_vector *x1, *x2;
		int ret;

		int i,j,k;
		double rms=0;
		double rmsB=0;
		double rmsX = 0;

		mA = gsl_matrix_alloc(M, N);
		b1 = gsl_vector_alloc(M);
		b2 = gsl_vector_alloc(M);
		x1 = gsl_vector_alloc(N);
		x2 = gsl_vector_alloc(N);
		resid1 = gsl_vector_alloc(M);
		resid2 = gsl_vector_alloc(M);

		for (i=0; i<M; i++)
			for (j=0; j<N; j++)
				gsl_matrix_set(mA, i, j, get(UVP2, i, j));

		for (i=0; i<M; i++) {
			gsl_vector_set(b1, i, get(B2, i, 0));
			gsl_vector_set(b2, i, get(B2, i, 1));
		}

		tau = gsl_vector_alloc(imin(M, N));

		ret = gsl_linalg_QR_decomp(mA, tau);
		// mA,tau now contains a packed version of Q,R.
		QR = mA;

		ret = gsl_linalg_QR_lssolve(QR, tau, b1, x1, resid1);
		ret = gsl_linalg_QR_lssolve(QR, tau, b2, x2, resid2);

		// Print rms(AX - B)
		for (j=0; j<M; j++) {
			for (k=0; k<2; k++) {
				double acc = 0.0;
				for (i=0; i<N; i++)
					acc += get(UVP2, j, i) * gsl_vector_get((k==0 ? x1 : x2), i); //get(X, i, k);
				acc -= get(B2, j, k);
				rms += square(acc);
			}
		}
		rms = sqrt(rms / (double)(M*2));

		for (j=0; j<M; j++) {
			rmsB += square(gsl_vector_get(resid1, j));
			rmsB += square(gsl_vector_get(resid2, j));
		}
		rmsB = sqrt(rmsB / (double)(M*2));

		for (j=0; j<N; j++) {
			rmsX += square(get(X, j, 0) - gsl_vector_get(x1, j));
			rmsX += square(get(X, j, 1) - gsl_vector_get(x2, j));
		}
		rmsX = sqrt(rmsX / (double)(N*2));

		printf("gsl rms(AX-B) = %g\n", rms);
		printf("gsl rmsB      = %g\n", rmsB);
		printf("rms(X_fortran - X_gsl) = %g\n", rmsX);

		gsl_vector_free(tau);
		gsl_matrix_free(mA);
		gsl_vector_free(b1);
		gsl_vector_free(b2);
		gsl_vector_free(x1);
		gsl_vector_free(x2);
		gsl_vector_free(resid1);
		gsl_vector_free(resid2);
	}


	// Row 0 of X are the shift (p=0, q=0) terms.
	// Row 1 of X are the terms that multiply "u".
	// Row 2 of X are the terms that multiply "v".

	// Grab CD.
	t->sip->wcstan.cd[0][0] = get(X, 1, 0);
	t->sip->wcstan.cd[1][0] = get(X, 1, 1);
	t->sip->wcstan.cd[0][1] = get(X, 2, 0);
	t->sip->wcstan.cd[1][1] = get(X, 2, 1);

	// Compute inv(CD)
	i = invert_2by2(t->sip->wcstan.cd, cdinv);
	assert(i == 0);

	// Grab the shift.
	sx = get(X, 0, 0);
	sy = get(X, 0, 1);
	sU =
		cdinv[0][0] * sx +
		cdinv[0][1] * sy;
	sV =
		cdinv[1][0] * sx +
		cdinv[1][1] * sy;

	// Extract the SIP coefficients.
	//  (this includes the 0 and 1 order terms, which we later overwrite)
	j = 0;
	for (order=0; order<=sip_order; order++) {
		for (q=0; q<=order; q++) {
			p = order - q;
			assert(j >= 0);
			assert(j < N);
			assert(p >= 0);
			assert(q >= 0);
			assert(p + q <= sip_order);

			t->sip->a[p][q] =
				cdinv[0][0] * get(X, j, 0) +
				cdinv[0][1] * get(X, j, 1);

			t->sip->b[p][q] =
				cdinv[1][0] * get(X, j, 0) +
				cdinv[1][1] * get(X, j, 1);
			j++;
		}
	}
	assert(j == N);

	// We have already dealt with the shift and linear terms, so zero them out
	// in the SIP coefficient matrix.
	t->sip->a_order = sip_order;
	t->sip->b_order = sip_order;
	t->sip->a[0][0] = 0.0;
	t->sip->b[0][0] = 0.0;
	t->sip->a[0][1] = 0.0;
	t->sip->a[1][0] = 0.0;
	t->sip->b[0][1] = 0.0;
	t->sip->b[1][0] = 0.0;

	invert_sip_polynomial(t);

	//	sU = get(X, 2, 0);
	//	sV = get(X, 2, 1);
	sip_calc_inv_distortion(t->sip, sU, sV, &su, &sv);
	//	su *= -1;
	//	sv *= -1;
	//printf("sU=%g, su=%g, sV=%g, sv=%g\n", sU, su, sV, sv);
	//printf("before cdinv b0=%g, b1=%g\n", get(b, 2, 0), get(b, 2, 1));
	//printf("BEFORE crval=(%.12g,%.12g)\n", t->sip->wcstan.crval[0], t->sip->wcstan.crval[1]);

	printf("sx = %g, sy = %g\n", sx, sy);
	printf("sU = %g, sV = %g\n", sU, sV);
	printf("su = %g, sv = %g\n", su, sv);

	/*
	  printf("Before applying shift:\n");
	  sip_print_to(t->sip, stdout);
	  printf("RMS error of correspondences: %g arcsec\n",
	  correspondences_rms_arcsec(t));
	*/

	swcs = wcs_shift(t->sip, -su, -sv);
	memcpy(t->sip, swcs, sizeof(sip_t));
	sip_free(swcs);

	//sip_free(t->sip);
	//t->sip = swcs;

	/*
	  t->sip->wcstan.crpix[0] -= su;
	  t->sip->wcstan.crpix[1] -= sv;
	*/

	printf("After applying shift:\n");
	sip_print_to(t->sip, stdout);

	/*
	  printf("shiftxun=%le, shiftyun=%le\n", sU, sV);
	  printf("shiftx=%le, shifty=%le\n", su, sv);
	  printf("sqerr=%le\n", figure_of_merit(t,NULL,NULL));
	*/

	// this data is now wrong
	tweak_clear_image_ad(t);
	tweak_clear_ref_xy(t);
	tweak_clear_image_xyz(t);

	// recalc based on new SIP
	tweak_go_to(t, TWEAK_HAS_IMAGE_AD);
	tweak_go_to(t, TWEAK_HAS_REF_XY);

	/*
	  printf("+++++++++++++++++++++++++++++++++++++\n");
	  printf("RMS=%lf [arcsec on sky]\n", sqrt(figure_of_merit(t,NULL,NULL) / stride));
	  printf("+++++++++++///////////+++++++++++++++\n");
	  //	fprintf(stderr,"sqerrxy=%le\n", figure_of_merit2(t));
	  */

	printf("RMS error of correspondences: %g arcsec\n",
		   correspondences_rms_arcsec(t, 0));
	printf("Weighted RMS error of correspondences: %g arcsec\n",
		   correspondences_rms_arcsec(t, 1));

	if (0) {
		// Calculate chi2 for sanity
		chisq = 0;
		for (i=0; i<M; i++) {
			int k;
			for (k=0; k<R; k++) {
				double sum = 0;
				int j;
				for (j=0; j<N; j++)
					sum += get(UVP2, i, j) * get(X, j, k);
				chisq += square(sum - get(B2, i, k));
			}
		}
		//	fprintf(stderr,"sqerrxy=%le (CHISQ matrix)\n", chisq);
		{
			int i,j,k;
			double rmsx=0, rmsy=0;
			printf("ax_b=array([");
			for (j=0; j<M; j++) {
				printf("[");
				for (k=0; k<2; k++) {
					double acc = 0.0;
					for (i=0; i<N; i++)
						acc += get(UVP2, j, i) * get(X, i, k);
					acc -= get(B2, j, k);
					printf("%g,", acc);
					
					if (k == 0)
						rmsx += square(acc);
					else
						rmsy += square(acc);
				}
				printf("],\n");
			}
			printf("])\n");
			
			rmsx = sqrt(rmsx / (double)M);
			rmsy = sqrt(rmsy / (double)M);

			printf("dotprod RMSX=%g [arcsec]\n", deg2arcsec(rmsx));
			printf("dotprod RMSY=%g [arcsec]\n", deg2arcsec(rmsy));
		}
	}


	//	t->sip->wcstan.cd[0][0] = tmptan.cd[0][0];
	//	t->sip->wcstan.cd[0][1] = tmptan.cd[0][1];
	//	t->sip->wcstan.cd[1][0] = tmptan.cd[1][0];
	//	t->sip->wcstan.cd[1][1] = tmptan.cd[1][1];
	//	t->sip->wcstan.crval[0] = tmptan.crval[0];
	//	t->sip->wcstan.crval[1] = tmptan.crval[1];
	//	t->sip->wcstan.crpix[0] = tmptan.crpix[0];
	//	t->sip->wcstan.crpix[1] = tmptan.crpix[1];

	free(UVP);
	free(B);
	free(UVP2);
	free(B2);
}

#undef set
#undef get

// RANSAC from Wikipedia:
// Given:
//     data - a set of observed data points
//     model - a model that can be fitted to data points
//     n - the minimum number of data values required to fit the model
//     k - the maximum number of iterations allowed in the algorithm
//     t - a threshold value for determining when a data point fits a model
//     d - the number of close data values required to assert that a model fits well to data
// Return:
//     bestfit - model parameters which best fit the data (or nil if no good model is found)
// iterations = 0
// bestfit = nil
// besterr = something really large
// while iterations < k {
//     maybeinliers = n randomly selected values from data
//     maybemodel = model parameters fitted to maybeinliers
//     alsoinliers = empty set
//     for every point in data not in maybeinliers {
//         if point fits maybemodel with an error smaller than t
//              add point to alsoinliers
//     }
//     if the number of elements in alsoinliers is > d {
//         % this implies that we may have found a good model
//         % now test how good it is
//         bettermodel = model parameters fitted to all points in maybeinliers and alsoinliers
//         thiserr = a measure of how well model fits these points
//         if thiserr < besterr {
//             bestfit = bettermodel
//             besterr = thiserr
//         }
//     }
//     increment iterations
// }
// return bestfit

void do_ransac(tweak_t* t)
{
	int iterations = 0;
	int maxiter = 500;

	sip_t wcs_try, wcs_best;
	memcpy(&wcs_try, t->sip, sizeof(sip_t));
	memcpy(&wcs_best, t->sip, sizeof(sip_t));

	double besterr = 100000000000000.;
	int sorder = t->sip->a_order;
	int num_free_coeffs = sorder*(sorder+1) + 4 + 2; // CD and CRVAL
	int min_data_points = num_free_coeffs/2 + 5;
//	min_data_points *= 2;
//	min_data_points *= 2;
	printf("/--------------------\n");
	printf("&&&&&&&&&&& mindatapts=%d\n", min_data_points);
	printf("\\-------------------\n");
	int set_size = il_size(t->image);
	assert( il_size(t->image) == il_size(t->ref) );
	il* maybeinliers = il_new(30);
//	il* alsoinliers = il_new(4);

	// we need to prevent pairing any reference star to multiple image
	// stars, or multiple reference stars to single image stars
	il* used_ref_sources = il_new(t->n_ref);
	il* used_image_sources = il_new(t->n);

	int i;
	for (i=0; i<t->n_ref; i++) 
		il_append(used_ref_sources, 0);
	for (i=0; i<t->n; i++) 
		il_append(used_image_sources, 0);

	while (iterations++ < maxiter) {
//		assert(t->ref);
		printf("++++++++++ ITERATION %d\n", iterations);

		// select n random pairs to use for the fit
		il_remove_all(maybeinliers);
		for (i=0; i<t->n_ref; i++) 
			il_set(used_ref_sources, i, 0);
		for (i=0; i<t->n; i++) 
			il_set(used_image_sources, i, 0);
		while (il_size(maybeinliers) < min_data_points) {
			int r = rand()/(double)RAND_MAX * set_size;
//			printf("eeeeeeeeeeeeeeeee %d\n", r);
			// check to see if either star in this pairing is
			// already taken before adding this pairing
			int ref_ind = il_get(t->ref, r);
			int image_ind = il_get(t->image, r);
			if (!il_get(used_ref_sources, ref_ind) &&
			    !il_get(used_image_sources, image_ind)) {
				il_insert_unique_ascending(maybeinliers, r);
				il_set(used_ref_sources, ref_ind, 1);
				il_set(used_image_sources, image_ind, 1);
			}
		}
		for (i=0; i<il_size(t->included); i++) 
			il_set(t->included, i, 0);
		for (i=0; i<il_size(maybeinliers); i++) 
			il_set(t->included, il_get(maybeinliers,i), 1);
		
		// now do a fit with our random sample selection
		t->state &= ~TWEAK_HAS_LINEAR_CD;
		tweak_go_to(t, TWEAK_HAS_LINEAR_CD);

		// this data is now wrong
		tweak_clear_image_ad(t);
		tweak_clear_ref_xy(t);
		tweak_clear_image_xyz(t);

		// recalc based on new SIP
		tweak_go_to(t, TWEAK_HAS_IMAGE_AD);
		tweak_go_to(t, TWEAK_HAS_REF_XY);
		tweak_go_to(t, TWEAK_HAS_IMAGE_XYZ);

		// rms arcsec
		double thiserr = sqrt(figure_of_merit(t,NULL,NULL) / il_size(t->ref));
		if (thiserr < besterr) {
			besterr = thiserr;
			tweak_dump_ascii(t);
		}

		/*
		// now find other samples which do well under the model fit by
		// the random sample set.
		il_remove_all(alsoinliers);
		for (i=0; i<il_size(t->included); i++) {
			if (il_get(t->included, i))
				continue;
			double thresh = 2.e-04; // FIXME mystery parameter
			double image_xyz[3];
			double ref_xyz[3];
			int ref_ind = il_get(t->ref, i);
			int image_ind = il_get(t->image, i);
			double a,d;
			pixelxy2radec(t->sip, t->x[image_ind],t->x[image_ind], &a,&d);
			radecdeg2xyzarr(a,d,image_xyz);
			radecdeg2xyzarr(t->a_ref[ref_ind],t->d_ref[ref_ind],ref_xyz);
			double dx = ref_xyz[0] - image_xyz[0];
			double dy = ref_xyz[1] - image_xyz[1];
			double dz = ref_xyz[2] - image_xyz[2];
			double err = dx*dx+dy*dy+dz*dz;
			if (sqrt(err) < thresh)
				il_append(alsoinliers, i);
		}

		// if we found a good number of points which are really close,
		// then fit both our random sample and the other close points
		if (10 < il_size(alsoinliers)) { // FIXME mystery parameter

			printf("found extra samples %d\n", il_size(alsoinliers));
			for (i=0; i<il_size(alsoinliers); i++) 
				il_set(t->included, il_get(alsoinliers,i), 1);
			
			// FIT AGAIN
			// FIXME put tweak here
			if (t->err < besterr) {
				memcpy(&wcs_best, t->sip, sizeof(sip_t));
				besterr = t->err;
				printf("new best error %le\n", besterr);
			}
		}
		printf("error=%le besterror=%le\n", t->err, besterr);
		*/
	}
	printf("==============================\n");
	printf("==============================\n");
	printf("besterr = %g \n", besterr);
	printf("==============================\n");
	printf("==============================\n");
}

// Really what we want is some sort of fancy dependency system... DTDS!
// Duct-tape dependencey system (DTDS)
#define done(x) t->state |= x; return x;
#define want(x) \
	if (flag == x && t->state & x) \
		return x; \
	else if (flag == x)
#define ensure(x) \
	if (!(t->state & x)) { \
		return tweak_advance_to(t, x); \
	}

unsigned int tweak_advance_to(tweak_t* t, unsigned int flag)
{
	//	fprintf(stderr,"WANT: ");
	//	tweak_print_the_state(flag);
	//	fprintf(stderr,"\n");
	want(TWEAK_HAS_IMAGE_AD) {
		//printf("////++++-////\n");
		int jj;
		ensure(TWEAK_HAS_SIP);
		ensure(TWEAK_HAS_IMAGE_XY);

		printf("Satisfying TWEAK_HAS_IMAGE_AD\n");

		// Convert to ra dec
		assert(!t->a);
		assert(!t->d);
		t->a = malloc(sizeof(double) * t->n);
		t->d = malloc(sizeof(double) * t->n);
		for (jj = 0; jj < t->n; jj++) {
			sip_pixelxy2radec(t->sip, t->x[jj], t->y[jj], t->a + jj, t->d + jj);
			//			printf("i=%4d, x=%10g, y=%10g ==> a=%10g, d=%10g\n", jj, t->x[jj], t->y[jj], t->a[jj], t->d[jj]);
		}

		done(TWEAK_HAS_IMAGE_AD);
	}

	want(TWEAK_HAS_REF_AD) {
		ensure(TWEAK_HAS_REF_XYZ);

		// FIXME
		printf("Satisfying TWEAK_HAS_REF_AD\n");
		printf("FIXME!\n");

		done(TWEAK_HAS_REF_AD);
	}

	want(TWEAK_HAS_REF_XY) {
		int jj;
		ensure(TWEAK_HAS_REF_AD);

		//tweak_clear_ref_xy(t);

		printf("Satisfying TWEAK_HAS_REF_XY\n");
		assert(t->state & TWEAK_HAS_REF_AD);
		assert(t->n_ref);
		assert(!t->x_ref);
		assert(!t->y_ref);
		t->x_ref = malloc(sizeof(double) * t->n_ref);
		t->y_ref = malloc(sizeof(double) * t->n_ref);
		for (jj = 0; jj < t->n_ref; jj++) {
			sip_radec2pixelxy(t->sip, t->a_ref[jj], t->d_ref[jj],
			                  t->x_ref + jj, t->y_ref + jj);
			//fprintf(stderr,"ref star %04d: %g,%g\n",jj,t->x_ref[jj],t->y_ref[jj]);
		}

		done(TWEAK_HAS_REF_XY);
	}

	want(TWEAK_HAS_AD_BAR_AND_R) {
		ensure(TWEAK_HAS_IMAGE_AD);

		printf("Satisfying TWEAK_HAS_AD_BAR_AND_R\n");

		assert(t->state & TWEAK_HAS_IMAGE_AD);
		get_center_and_radius(t->a, t->d, t->n,
		                      &t->a_bar, &t->d_bar, &t->radius);
		printf("a_bar=%lf [deg], d_bar=%lf [deg], radius=%lf [arcmin]\n",
		        t->a_bar, t->d_bar, rad2arcmin(t->radius));

		done(TWEAK_HAS_AD_BAR_AND_R);
	}

	want(TWEAK_HAS_IMAGE_XYZ) {
		int i;
		ensure(TWEAK_HAS_IMAGE_AD);

		printf("Satisfying TWEAK_HAS_IMAGE_XYZ\n");
		assert(!t->xyz);

		t->xyz = malloc(3 * t->n * sizeof(double));
		for (i = 0; i < t->n; i++) {
			radecdeg2xyzarr(t->a[i], t->d[i], t->xyz + 3*i);
		}
		done(TWEAK_HAS_IMAGE_XYZ);
	}

	want(TWEAK_HAS_REF_XYZ) {
		if (t->state & TWEAK_HAS_HEALPIX_PATH) {
			ensure(TWEAK_HAS_AD_BAR_AND_R);
			printf("Satisfying TWEAK_HAS_REF_XYZ\n");
			get_reference_stars(t);
		} else if (t->state & TWEAK_HAS_REF_AD) {
			tweak_ref_find_xyz_from_ad(t);
		} else {
			ensure(TWEAK_HAS_REF_AD);

			printf("Satisfying TWEAK_HAS_REF_XYZ\n");
			printf("FIXME!\n");

			// try a conversion
			assert(0);
		}

		done(TWEAK_HAS_REF_XYZ);
	}

	want(TWEAK_HAS_COARSLY_SHIFTED) {
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_IMAGE_XY);

		printf("Satisfying TWEAK_HAS_COARSLY_SHIFTED\n");

		get_dydx_range(t->x, t->y, t->n,
		               t->x_ref, t->y_ref, t->n_ref,
		               &t->mindx, &t->mindy, &t->maxdx, &t->maxdy);

		do_entire_shift_operation(t, 1.0);
		tweak_clear_image_ad(t);
		tweak_clear_ref_xy(t);

		done(TWEAK_HAS_COARSLY_SHIFTED);
	}

	want(TWEAK_HAS_FINELY_SHIFTED) {
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_IMAGE_XY);
		ensure(TWEAK_HAS_COARSLY_SHIFTED);

		printf("Satisfying TWEAK_HAS_FINELY_SHIFTED\n");

		// Shrink size of hough box
		do_entire_shift_operation(t, 0.3);
		tweak_clear_image_ad(t);
		tweak_clear_ref_xy(t);

		done(TWEAK_HAS_FINELY_SHIFTED);
	}

	want(TWEAK_HAS_REALLY_FINELY_SHIFTED) {
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_IMAGE_XY);
		ensure(TWEAK_HAS_FINELY_SHIFTED);

		printf("Satisfying TWEAK_HAS_REALLY_FINELY_SHIFTED\n");

		// Shrink size of hough box
		do_entire_shift_operation(t, 0.03);
		tweak_clear_image_ad(t);
		tweak_clear_ref_xy(t);

		done(TWEAK_HAS_REALLY_FINELY_SHIFTED);
	}

	want(TWEAK_HAS_CORRESPONDENCES) {
		ensure(TWEAK_HAS_REF_XYZ);
		ensure(TWEAK_HAS_IMAGE_XYZ);

		printf("Satisfying TWEAK_HAS_CORRESPONDENCES\n");

		t->jitterd2 = arcsec2distsq(t->jitter);
		find_correspondences(t, 6.0 * arcsec2rad(t->jitter));

		done(TWEAK_HAS_CORRESPONDENCES);
	}

	want(TWEAK_HAS_LINEAR_CD) {
		ensure(TWEAK_HAS_SIP);
		ensure(TWEAK_HAS_REALLY_FINELY_SHIFTED);
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_REF_AD);
		ensure(TWEAK_HAS_IMAGE_XY);
		ensure(TWEAK_HAS_CORRESPONDENCES);

		printf("Satisfying TWEAK_HAS_LINEAR_CD\n");

		do_sip_tweak(t);

		tweak_clear_on_sip_change(t);

		done(TWEAK_HAS_LINEAR_CD);
	}

	want(TWEAK_HAS_RUN_RANSAC_OPT) {
		ensure(TWEAK_HAS_CORRESPONDENCES);
		do_ransac(t);

		done(TWEAK_HAS_RUN_RANSAC_OPT);
	}

	printf("die for dependence: ");
	tweak_print_the_state(flag);
	printf("\n");
	assert(0);
	return -1;
}

void tweak_push_wcs_tan(tweak_t* t, tan_t* wcs)
{
	if (!t->sip) {
		t->sip = sip_create();
	}
	memcpy(&(t->sip->wcstan), wcs, sizeof(tan_t));
	t->state |= TWEAK_HAS_SIP;
}

void tweak_go_to(tweak_t* t, unsigned int dest_state)
{
	while (! (t->state & dest_state))
		tweak_advance_to(t, dest_state);
}

#define SAFE_FREE(xx) if ((xx)) {free((xx)); xx = NULL;}
void tweak_clear(tweak_t* t)
{
	// FIXME
	if (cached_kd)
		kdtree_fits_close(cached_kd);

	if (!t)
		return ;
	SAFE_FREE(t->a);
	SAFE_FREE(t->d);
	SAFE_FREE(t->x);
	SAFE_FREE(t->y);
	SAFE_FREE(t->xyz);
	SAFE_FREE(t->a_ref);
	SAFE_FREE(t->d_ref);
	SAFE_FREE(t->x_ref);
	SAFE_FREE(t->y_ref);
	SAFE_FREE(t->xyz_ref);
	if (t->sip) {
		sip_free(t->sip);
		t->sip = NULL;
	}
	il_free(t->image);
	il_free(t->ref);
	dl_free(t->dist2);
	if (t->weight)
		dl_free(t->weight);
	il_free(t->maybeinliers);
	il_free(t->bestinliers);
	il_free(t->included);
	t->image = NULL;
	t->ref = NULL;
	t->dist2 = NULL;
	t->weight = NULL;
	t->maybeinliers = NULL;
	t->bestinliers = NULL;
	t->included = NULL;
	kdtree_free(t->kd_image);
	kdtree_free(t->kd_ref);
	SAFE_FREE(t->hppath);
}

void tweak_free(tweak_t* t)
{
	tweak_clear(t);
	free(t);
}
