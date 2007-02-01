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
#include <assert.h>
#include "libwcs/fitsfile.h"
#include "libwcs/wcs.h"
#include "starutil.h"
#include "fitsio.h"
#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "dualtree_rangesearch.h"
#include "healpix.h"
#include "ezfits.h"
#include "sip.h"
#include "sip_util.h"
#include "bl.h"
#include "lm.h"
#include "tweak_internal.h"


int get_xy(fitsfile* fptr, int hdu, float **x, float **y, int *n)
{
	// Find this extension in fptr (which should be open to the xylist)
	int nhdus, hdutype;
	int status=0;

	if (fits_get_num_hdus(fptr, &nhdus, &status)) {
		fits_report_error(stderr, status);
		exit(-1);
	}
	printf("Found %d HDU's\n",nhdus);

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

void get_xy_data(tweak_t* t, fitsfile* xyfptr, int hdu)
{
	// Extract XY from FITS table as floats
	int jj;
	float *xf, *yf;
	get_xy(xyfptr, hdu, &xf, &yf, &t->n);

	// Convert to doubles FIXME cfitsio may be able to do this
	t->x = malloc(sizeof(double)*t->n);
	t->y = malloc(sizeof(double)*t->n);
	for (jj=0; jj<t->n; jj++) {
		t->x[jj] = xf[jj];
		t->y[jj] = yf[jj];
	}
	t->state |= TWEAK_HAS_IMAGE_XY;
}

// Grabs the data we need for tweak from various sources:
// XY locations in pixel space
// AD locations corresponding to sources
// estimated field center / radius
// XY reference locations
// AD reference locations
#if 0
int get_tweak_data(tweak_t* t, fitsfile* xyfptr, char* hppat, int hdu)
{
	// Extract XY from FITS table as floats
	int jj;
	float *xf, *yf;
	get_xy(xyfptr, hdu, &xf, &yf, &t->n);

	// Convert to doubles FIXME cfitsio may be able to do this
	t->x = malloc(sizeof(double)*t->n);
	t->y = malloc(sizeof(double)*t->n);
	for (jj=0; jj<t->n; jj++) {
		t->x[jj] = xf[jj];
		t->y[jj] = yf[jj];
	}

	// Convert to ra dec
//	t->a = malloc(sizeof(double)*t->n);
//	t->d = malloc(sizeof(double)*t->n);
//	for (jj=0; jj<t->n; jj++) {
//		pixelxy2radec(t->sip, t->x[jj], t->y[jj], t->a+jj, t->d+jj);
//		if (jj < 30) 
//			printf("::: %lf %lf\n", t->a[jj], t->d[jj]);
//	}

//	ezscatter("scatter_image.fits", t->x,t->y,t->a,t->d,t->n);

	// Find field center/radius
//	get_center_and_radius(t->a, t->d, t->n,
//	                      &t->a_bar, &t->d_bar, &t->radius);
//	fprintf(stderr, "abar=%f, dbar=%f, radius in rad=%f\n",
//	                 t->a_bar, t->d_bar, t->radius);

	while( TWEAK_HAS_AD_BAR_AND_R !=
			tweak_advance_to(t, TWEAK_HAS_AD_BAR_AND_R));

	// Get the reference stars from our catalog
	get_reference_stars(t->a_bar, t->d_bar, t->radius,
			    &t->a_ref, &t->d_ref, &t->n_ref, hppat);
	if (!t->n_ref)  {
		// No reference stars? This is bad.
		fprintf(stderr, "No reference stars; failing\n");
		free(t->a);
		free(t->d);
		free(t->x);
		free(t->y);
		return 0;
	}

	// Shift runs in XY; project reference stars
	t->x_ref = malloc(sizeof(double)*t->n_ref);
	t->y_ref = malloc(sizeof(double)*t->n_ref);
	for (jj=0; jj<t->n_ref; jj++) {
		radec2pixelxy(t->sip, t->a_ref[jj], t->d_ref[jj],
		              t->x_ref+jj, t->y_ref+jj);
	}
	return 1;
}
#endif

void free_extraneous(tweak_t* t) 
{
	// don't free x and y because we don't compute them
	if (t->a) free(t->a);
	if (t->d) free(t->d);
	if (t->a_ref) free(t->a_ref);
	if (t->d_ref) free(t->d_ref);
	if (t->x_ref) free(t->x_ref);
	if (t->y_ref) free(t->y_ref);
}


void dump_data(tweak_t* t)
{
	static char s = '1';
	char fn[] = "scatter_imageN.fits";
	fn[13] = s;
	ezscatter(fn, t->x,t->y,t->a,t->d,t->n);

	char fn2[] = "scatter_ref__N.fits";
	fn2[13] = s;
	ezscatter(fn2, t->x_ref,t->y_ref,t->a_ref,t->d_ref,t->n_ref);

	if (t->image) {
		double *a, *d, *x, *y;
		a = malloc(sizeof(double)*il_size(t->image));
		d = malloc(sizeof(double)*il_size(t->image));
		x = malloc(sizeof(double)*il_size(t->image));
		y = malloc(sizeof(double)*il_size(t->image));

		double *a_ref, *d_ref, *x_ref, *y_ref;
		a_ref = malloc(sizeof(double)*il_size(t->image));
		d_ref = malloc(sizeof(double)*il_size(t->image));
		x_ref = malloc(sizeof(double)*il_size(t->image));
		y_ref = malloc(sizeof(double)*il_size(t->image));

		double *dx, *dy;
		dx = malloc(sizeof(double)*il_size(t->image));
		dy = malloc(sizeof(double)*il_size(t->image));
		
		int i;
		for (i=0; i<il_size(t->image); i++) {
			int im_ind = il_get(t->image, i);
			x[i] = t->x[im_ind];
			y[i] = t->y[im_ind];
			sip_pixelxy2radec(t->sip, x[i],y[i], a+i,d+i);
//			a[i] = t->a[im_ind];
//			d[i] = t->d[im_ind];

			int ref_ind = il_get(t->ref, i);
			a_ref[i] = t->a_ref[ref_ind];
			d_ref[i] = t->d_ref[ref_ind];
			// Must recompute ref xy's
//			radec2pixelxy(t->sip, a_ref[i], d_ref[i], 
//					x_ref +i, y_ref+i);
			x_ref[i] = t->x_ref[ref_ind];
			y_ref[i] = t->y_ref[ref_ind];

			dx[i] = x_ref[i] - x[i];
			dy[i] = y_ref[i] - y[i];
		}
		char fn3[] = "scatter_corIMN.fits";
		char fn4[] = "scatter_corREN.fits";
		char fn5[] = "scatter_corVEN.fits";
		fn3[13] = s;
		fn4[13] = s;
		fn5[13] = s;
		ezscatter(fn3, x,y,a,d,il_size(t->image));
		ezscatter(fn4, x_ref,y_ref,a_ref,d_ref,il_size(t->image));
		ezscatter(fn5, x, y, dx, dy, il_size(t->image));
		free(a);
		free(d);
		free(x);
		free(y);
		free(a_ref);
		free(d_ref);
		free(x_ref);
		free(y_ref);
	}
	s++;
}

// Our parameter vector is 'p' for LM. The breakdown of parameters into the
// parameter vector goes in the following order:
//
// OPT_CRVAL       CRVAL0
//                 CRVAL1
//               
// OPT_CRPIX       CRPIX0
//                 CRPIX1
//               
// OPT_CD          CD1_1
//                 CD1_2
//                 CD2_1
//                 CD2_2
//                 
// OPT_SIP         A_1_2         Note that A_1_1 is not present; it is 0
//                 A_1_3
//                 A_1_4
//                 A_1_5
//                 A_1_a_order   All A matrix terms listed in row-major order;
//                               same for B matrix. All terms are such that
//                               i+j <= a_order. Below-cross-diagonal terms
//                               are implicitly zero.
//                 B_1_1
//                 B_1_2         ... and so on

void unpack_params(sip_t* sip, double *pp, int opt_flags) 
{
	assert(pp);
	assert(sip);
	if (opt_flags & OPT_CRVAL) {
		sip->crval[0] = *pp++;
		sip->crval[1] = *pp++;
	}
	if (opt_flags & OPT_CRPIX) {
		sip->crpix[0] = *pp++;
		sip->crpix[1] = *pp++;
	}
	if (opt_flags & OPT_CD) {
		sip->cd[0][0] = *pp++;
		sip->cd[0][1] = *pp++;
		sip->cd[1][0] = *pp++;
		sip->cd[1][1] = *pp++;
	}
	if (opt_flags & OPT_SIP) {
		int p, q;
		for (p=0; p<sip->a_order; p++)
			for (q=0; q<sip->a_order; q++)
				if (p+q <= sip->a_order && !(p==0&&q==0))
					 sip->a[p][q] = *pp++;
		for (p=0; p<sip->b_order; p++)
			for (q=0; q<sip->b_order; q++)
				if (p+q <= sip->b_order && !(p==0&&q==0))
					 sip->b[p][q] = *pp++;
	}
	if (opt_flags & OPT_SIP_INVERSE) {
		int p, q;
		for (p=0; p<sip->ap_order; p++)
			for (q=0; q<sip->ap_order; q++)
				if (p+q <= sip->ap_order && !(p==0&&q==0))
					 sip->ap[p][q] = *pp++;
		for (p=0; p<sip->bp_order; p++)
			for (q=0; q<sip->bp_order; q++)
				if (p+q <= sip->bp_order && !(p==0&&q==0))
					 sip->bp[p][q] = *pp++;
	}
}

int pack_params(sip_t* sip, double *parameters, int opt_flags) 
{
	double* pp = parameters;
	if (opt_flags & OPT_CRVAL) {
		*pp++ = sip->crval[0];
		*pp++ = sip->crval[1];
	}
	if (opt_flags & OPT_CRPIX) {
		*pp++ = sip->crpix[0];
		*pp++ = sip->crpix[1];
	}
	if (opt_flags & OPT_CD) {
		*pp++ = sip->cd[0][0];
		*pp++ = sip->cd[0][1];
		*pp++ = sip->cd[1][0];
		*pp++ = sip->cd[1][1];
	}
	if (opt_flags & OPT_SIP) {
//		printf("Packing SIP||||||||\n");
		int p, q;
		for (p=0; p<sip->a_order; p++)
			for (q=0; q<sip->a_order; q++)
				if (p+q <= sip->a_order && !(p==0&&q==0)) {
//					if (sip->a[p][q] != 0.00) 
//						printf("Found nonzero=%le\n", sip->a[p][q]);
					  *pp++ = sip->a[p][q];
				}
		for (p=0; p<sip->b_order; p++)
			for (q=0; q<sip->b_order; q++)
				if (p+q <= sip->b_order && !(p==0&&q==0))
					  *pp++ = sip->b[p][q];
	}

	if (opt_flags & OPT_SIP_INVERSE) {
		int p, q;
		for (p=0; p<sip->ap_order; p++)
			for (q=0; q<sip->ap_order; q++)
				if (p+q <= sip->ap_order && !(p==0&&q==0))
					  *pp++ = sip->ap[p][q];
		for (p=0; p<sip->bp_order; p++)
			for (q=0; q<sip->bp_order; q++)
				if (p+q <= sip->bp_order && !(p==0&&q==0))
					  *pp++ = sip->bp[p][q];
	}
	return pp - parameters;
}

// RIFE with optimization opportunities
void cost(double *p, double *hx, int m, int n, void *adata)
{
	tweak_t* t = adata;

	sip_t sip;
	memcpy(&sip, t->sip, sizeof(sip_t));
	unpack_params(&sip, p, t->opt_flags);

	// Run the gauntlet.
	double err = 0; // calculate our own sum-squared error
	int i;
	for (i=0; i<il_size(t->image); i++) {
		if (!il_get(t->included, i)) 
			continue;
		double a, d;
		int image_ind = il_get(t->image, i);
		sip_pixelxy2radec(&sip, t->x[image_ind], t->y[image_ind], &a, &d);
		double image_xyz[3];
		radecdeg2xyzarr(a, d, image_xyz);
		*hx++ = image_xyz[0];
		*hx++ = image_xyz[1];
		*hx++ = image_xyz[2];

		double ref_xyz[3];
		int ref_ind = il_get(t->ref, i);
		radecdeg2xyzarr(t->a_ref[ref_ind], t->d_ref[ref_ind], ref_xyz);
		double dx = ref_xyz[0] - image_xyz[0];
		double dy = ref_xyz[1] - image_xyz[1];
		double dz = ref_xyz[2] - image_xyz[2];
		err += dx*dx+dy*dy+dz*dz;
//		printf("dx=%le, dy=%le, dz=%le\n",dx,dy,dz);

	}
//	printf("sqd error=%le\n", err);
}

// Do a fit.
// Requires: correspondences     t->image, t->ref,
//           image sources       t->x,t->y,
//           reference sources   t->a_ref,t->d_ref
//           optimization flags  t->opt_flags
//           initial SIP guess   t->sip
void lm_fit(tweak_t* t)
{
	double params[410];
//	t->opt_flags = OPT_CRVAL | OPT_CRPIX | OPT_CD;
//	t->opt_flags = OPT_CRVAL | OPT_CD;
	t->opt_flags = OPT_CRVAL | OPT_CRPIX | OPT_CD | OPT_SIP;
	t->sip->a_order = 7;
	t->sip->b_order = 7;

//	printf("BEFORE::::::::::\n");
//	print_sip(t->sip);

	//WTF
//	t->sip->cd[0][0] *= 1.41;
//	t->sip->cd[0][1] *= 1.41;
//	t->sip->cd[1][0] *= 1.41;
//	t->sip->cd[1][1] *= 1.41;

//	t->sip->crval[0] *= 1.61;
//	t->sip->crval[1] *= 1.22;

//	t->sip->crpix[0] *= 5.01;
//	t->sip->crpix[1] *= 1.02;
//	t->sip->a[2][2] = 5555;
//	t->sip->b[2][2] = -1111;
//	t->sip->a[1][1] = 44.44;
//	t->sip->b[1][1] = -2.22;


//	print_sip(t->sip);

	int m =  pack_params(t->sip, params, t->opt_flags);
//	printf("REPACKED::::::::::\n");
//	sip_t sip;
//	memcpy(&sip, t->sip, sizeof(sip_t));
//	unpack_params(&sip, params, t->opt_flags);
//	print_sip(&sip);

	// Pack target values
	double *desired = malloc(sizeof(double)*il_size(t->image)*3);
	double *hx = desired;
	int i;
	for (i=0; i<il_size(t->image); i++) {
		if (!il_get(t->included, i)) 
			continue;
		int ref_ind = il_get(t->ref, i);
		double ref_xyz[3];
		radecdeg2xyzarr(t->a_ref[ref_ind], t->d_ref[ref_ind], ref_xyz);
		*hx++ = ref_xyz[0];
		*hx++ = ref_xyz[1];
		*hx++ = ref_xyz[2];
	}

	int n = hx - desired;
//	assert(hx-desired == 3*il_size(t->image));

//	printf("Starting optimization m=%d n=%d!!!!!!!!!!!\n",m,n);
	double info[LM_INFO_SZ];
	int max_iterations = 200;
	double opts[] = { 0.001*LM_INIT_MU,
	                  0.001*LM_STOP_THRESH,
	                  0.001*LM_STOP_THRESH,
		          0.001*LM_STOP_THRESH,
	                  0.001*-LM_DIFF_DELTA};
	dlevmar_dif(cost, params, desired, m, n, max_iterations,
	            opts, info, NULL, NULL, t);

//	printf("initial error^2 = %le\n", info[0]);
//	printf("final   error^2 = %le\n", info[1]);
//	printf("nr iterations   = %lf\n", info[5]);
//	printf("term reason     = %lf\n", info[6]);
//	printf("function evals  = %lf\n", info[7]);

	unpack_params(t->sip, params, t->opt_flags);
	t->err = info[1];
//	memcpy(t->parameters, params, sizeof(double)*t->n);
}

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

void ransac(tweak_t* t)
{
	int iterations = 0;
	int maxiter = 40;

	sip_t wcs_try, wcs_best;
	memcpy(&wcs_try, t->sip, sizeof(sip_t));
	memcpy(&wcs_best, t->sip, sizeof(sip_t));

	double besterr = 100000000000000.;
	int min_data_points = 100;
	int set_size = il_size(t->image);
	il* maybeinliers = il_new(4);
	il* alsoinliers = il_new(4);

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
		lm_fit(t);

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
			sip_pixelxy2radec(t->sip, t->x[image_ind],t->x[image_ind], &a,&d);
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
			lm_fit(t);
			if (t->err < besterr) {
				memcpy(&wcs_best, t->sip, sizeof(sip_t));
				besterr = t->err;
				printf("new best error %le\n", besterr);
			}
		}
		printf("error=%le besterror=%le\n", t->err, besterr);
	}
}


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
	//sip_t* wcs;
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
		tweak_t tweak;
		tweak_init(&tweak);
		// set jitter: 6 arcsec.
		tweak.jitter = 6.0;
		tweak.Nside = Nside;

		// At this point, we have an image. Now get the WCS data
		tweak.sip = load_sip_from_fitsio(fptr);
		if (!tweak.sip) {
			fprintf(stderr, "Problems with WCS info, skipping HDU\n");
			continue;
		}
		print_sip(tweak.sip);
		tweak.state = TWEAK_HAS_SIP;

		// Now get image XY data
		get_xy_data(&tweak, xyfptr, kk);
		tweak_push_hppath(&tweak, hppat);

//		unsigned int dest_state = TWEAK_HAS_LINEAR_CD;
//		dest_state = TWEAK_HAS_IMAGE_AD;
//		while(! (tweak.state & dest_state)) {
//			printf("%p\n", (void*)tweak.state);
//			tweak_print_state(&tweak);
//			printf("\n");
//			tweak_advance_to(&tweak, dest_state);
//			tweak_dump_ascii(&tweak);
//			getchar();
//		}
//		tweak.state &= ~TWEAK_HAS_LINEAR_CD;
		tweak_go_to(&tweak, TWEAK_HAS_LINEAR_CD);
		tweak_go_to(&tweak, TWEAK_HAS_REF_XY);
		tweak_go_to(&tweak, TWEAK_HAS_IMAGE_AD);
		tweak_go_to(&tweak, TWEAK_HAS_CORRESPONDENCES);
		tweak_dump_ascii(&tweak);

		int k;
		for (k=0; k<5; k++) {
			tweak.state &= ~TWEAK_HAS_LINEAR_CD;
			tweak_go_to(&tweak, TWEAK_HAS_LINEAR_CD);
		}
		tweak_go_to(&tweak, TWEAK_HAS_REF_XY);
		tweak_go_to(&tweak, TWEAK_HAS_IMAGE_AD);
		tweak_go_to(&tweak, TWEAK_HAS_CORRESPONDENCES);
		printf("final state: ");
		tweak_print_state(&tweak);
		tweak_dump_ascii(&tweak);
		print_sip(tweak.sip);
		printf("\n");

		exit(1);
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
