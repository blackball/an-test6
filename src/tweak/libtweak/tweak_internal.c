#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

// debug
#include <netinet/in.h>

#include "tweak_internal.h"
#include "healpix.h"
#include "dualtree_rangesearch.h"
#include "kdtree_fits_io.h"

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
//  - Put CD inverse into it's own function
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
	if (x < y) return y;
	return x;
}
double dblmin(double x, double y)
{
	if (x < y) return x;
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

	for (i=0; i<nimg; i++) {
		for (j=0; j<ncat; j++) {
			double dx = ximg[i]-xcat[j];
			double dy = yimg[i]-ycat[j];
			*maxdx = dblmax(dx,*maxdx);
			*maxdy = dblmax(dy,*maxdy);
			*mindx = dblmin(dx,*mindx);
			*mindy = dblmin(dy,*mindy);
		}
	}
}

void get_shift(double* ximg, double* yimg, int nimg,
               double* xcat, double* ycat, int ncat, 
	       double mindx, double mindy, double maxdx, double maxdy,
               double* xshift, double* yshift)
{

	int i, j;
	int themax,themaxind,ys,xs;

	// hough transform 
	int hsz = 1000; // hough histogram size (per side)
	int *hough = calloc(hsz*hsz, sizeof(int)); // allocate bins
	int kern[] = {0,  2,  3, 2,  0, // approximate gaussian smoother
					  2,  7, 12, 7,  2,      // should be KERNEL_SIZE x KERNEL_SIZE
					  3, 12, 20, 12, 3,
					  2,  7, 12, 7,  2,
					  0,  2,  3, 2,  0};

	for (i=0; i<nimg; i++) {    // loop over all pairs of source-catalog objs
		for (j=0; j<ncat; j++) {
			double dx = ximg[i]-xcat[j];
			double dy = yimg[i]-ycat[j];
			int hszi = hsz-1;
			int iy = hszi*( (dy-mindy)/(maxdy-mindy) ); // compute deltay using implicit floor
			int ix = hszi*( (dx-mindx)/(maxdx-mindx) ); // compute deltax using implicit floor

			// check to make sure the point is in the box
			if (KERNEL_MARG <= iy && iy < hsz-KERNEL_MARG &&
				 KERNEL_MARG <= ix && ix < hsz-KERNEL_MARG) {
				int kx, ky;
				for (ky=-2; ky<=2; ky++)
					for (kx=-2; kx<=2; kx++)
						hough[(iy-ky)*hsz + (ix-kx)] += kern[(ky+2)*5+(kx+2)];
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
	for (i=0; i<hsz*hsz; i++) {
	  if (themax < hough[i]) { // find peak in hough
			themaxind = i;
			themax = hough[i];
		}
	}

	ys = themaxind/hsz; // where is this best index in x,y space?
	xs = themaxind%hsz;

	fprintf(stderr,"xshsz = %d, yshsz=%d\n",xs,ys); // FIXME logging

	*yshift = ((double)(themaxind/hsz)/(double)hsz)*(maxdy-mindy)+mindy;
	*xshift = ((double)(themaxind % hsz)/(double)hsz)*(maxdx-mindx)+mindx;
	fprintf(stderr,"get_shift: mindx=%lf, maxdx=%lf, mindy=%lf, maxdy=%lf\n", mindx, maxdx, mindy, maxdy);
	fprintf(stderr,"get_shift: xs=%lf, ys=%lf\n", *xshift, *yshift);

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
   double crpix0,crpix1,crval0,crval1;
	double nxref, nyref,theta,sintheta,costheta;
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
	theta = -deg2rad(nxref-crval0); // deltaRA = new minus old RA; 
	theta *= sin(deg2rad(nyref));  // multiply by the sin of the NEW Dec; at equator this correctly evals to zero
	sintheta = sin(theta);
	costheta = cos(theta);
	//fprintf(stderr,"wcs_shift: crval0: %g->%g   crval1:%g->%g\ntheta=%g   (sintheta,costheta)=(%g,%g)\n",
	//	  crval0,nxref,crval1,nyref,theta,sintheta,costheta);

	// Restore
	wcs->wcstan.crpix[0] = crpix0; // restore old crpix
	wcs->wcstan.crpix[1] = crpix1;

   // Fix the CD matrix since "northwards" has changed due to moving RA
	newCD[0][0] = costheta* swcs->wcstan.cd[0][0] - sintheta*swcs->wcstan.cd[0][1];
	newCD[0][1] = sintheta* swcs->wcstan.cd[0][0] + costheta*swcs->wcstan.cd[0][1];
	newCD[1][0] = costheta* swcs->wcstan.cd[1][0] - sintheta*swcs->wcstan.cd[1][1];
	newCD[1][1] = sintheta* swcs->wcstan.cd[1][0] + costheta*swcs->wcstan.cd[1][1];
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
	fprintf(stderr, "xshift=%lf, yshift=%lf\n", t->xs, t->ys);
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
	for (i=0; i<n; i++) 
		if (!z && !w)
			tweak_print2_fp(f, x[i], y[i]); 
		else
			tweak_print4_fp(f, x[i], y[i], z[i], w[i]); 
	fclose(f);
}

void tweak_print_the_state(unsigned int state)
{
	if (state & TWEAK_HAS_SIP            ) fprintf(stderr,"TWEAK_HAS_SIP, ");
	if (state & TWEAK_HAS_IMAGE_XY       ) fprintf(stderr,"TWEAK_HAS_IMAGE_XY, ");
	if (state & TWEAK_HAS_IMAGE_XYZ      ) fprintf(stderr,"TWEAK_HAS_IMAGE_XYZ, ");
	if (state & TWEAK_HAS_IMAGE_AD       ) fprintf(stderr,"TWEAK_HAS_IMAGE_AD, ");
	if (state & TWEAK_HAS_REF_XY         ) fprintf(stderr,"TWEAK_HAS_REF_XY, ");
	if (state & TWEAK_HAS_REF_XYZ        ) fprintf(stderr,"TWEAK_HAS_REF_XYZ, ");
	if (state & TWEAK_HAS_REF_AD         ) fprintf(stderr,"TWEAK_HAS_REF_AD, ");
	if (state & TWEAK_HAS_AD_BAR_AND_R   ) fprintf(stderr,"TWEAK_HAS_AD_BAR_AND_R, ");
	if (state & TWEAK_HAS_CORRESPONDENCES) fprintf(stderr,"TWEAK_HAS_CORRESPONDENCES, ");
	if (state & TWEAK_HAS_RUN_OPT        ) fprintf(stderr,"TWEAK_HAS_RUN_OPT, ");
	if (state & TWEAK_HAS_RUN_RANSAC_OPT ) fprintf(stderr,"TWEAK_HAS_RUN_RANSAC_OPT, ");
	if (state & TWEAK_HAS_COARSLY_SHIFTED) fprintf(stderr,"TWEAK_HAS_COARSLY_SHIFTED, ");
	if (state & TWEAK_HAS_FINELY_SHIFTED ) fprintf(stderr,"TWEAK_HAS_FINELY_SHIFTED, ");
	if (state & TWEAK_HAS_REALLY_FINELY_SHIFTED ) fprintf(stderr,"TWEAK_HAS_REALLY_FINELY_SHIFTED, ");
	if (state & TWEAK_HAS_HEALPIX_PATH   ) fprintf(stderr,"TWEAK_HAS_HEALPIX_PATH, ");
	if (state & TWEAK_HAS_LINEAR_CD      ) fprintf(stderr,"TWEAK_HAS_LINEAR_CD, ");
}

void tweak_print_state(tweak_t* t)
{
	tweak_print_the_state(t->state);
}

#define BUFSZ 100
#define USE_FILE(x) snprintf(fn, BUFSZ, x "_%p_%d.dat", (void*)t->state, dump_nr)
void tweak_dump_ascii(tweak_t* t)
{
	static int dump_nr = 0;
	char fn[BUFSZ];
	FILE* cor_im; // correspondences
	FILE* cor_ref;
	FILE* cor_delta;

	USE_FILE("scatter_image");
	tweak_print4(fn, t->x,t->y,t->a,t->d,t->n);

	if (t->state & TWEAK_HAS_REF_XY && 
			t->state & TWEAK_HAS_REF_AD) {
		USE_FILE("scatter_ref");
		tweak_print4(fn, t->x_ref,t->y_ref,t->a_ref,t->d_ref,t->n_ref);
	}

	if (t->image) {
		int i;
		USE_FILE("corr_im");
		cor_im = fopen(fn, "w");
		USE_FILE("corr_ref");
		cor_ref = fopen(fn, "w");
		USE_FILE("corr_delta");
		cor_delta = fopen(fn, "w");
		for (i=0; i<il_size(t->image); i++) {
			double a,d;
			int im_ind = il_get(t->image, i);
			sip_pixelxy2radec(t->sip, t->x[im_ind],t->y[im_ind], &a,&d);
			tweak_print4_fp(cor_im, t->x[im_ind], t->y[im_ind],
					a, d);

			if (t->state & TWEAK_HAS_REF_XY) {
				int ref_ind = il_get(t->ref, i);
				tweak_print4_fp(cor_ref,
						t->x_ref[ref_ind], t->y_ref[ref_ind],
						t->a_ref[ref_ind], t->d_ref[ref_ind]);

				tweak_print4_fp(cor_delta,
						t->x[im_ind], t->y[im_ind],
						t->x_ref[ref_ind] - t->x[im_ind],
						t->y_ref[ref_ind] - t->y[im_ind]);
			}
		}
		fclose(cor_im);
		fclose(cor_ref);
		fclose(cor_delta);
	}
	fprintf(stderr,"dump=%d\n", dump_nr);
	dump_nr++;
}
void get_center_and_radius(double* ra, double* dec, int n,
                          double* ra_mean, double* dec_mean, double* radius)
{
	double* xyz = malloc(3*n*sizeof(double));
	double xyz_mean[3] = {0,0,0};
	double norm=0;
	double maxdist2 = 0;
	int maxind = -1;
	int i, j;

	for (i=0; i<n; i++) {
		radec2xyzarr(deg2rad(ra[i]),
		             deg2rad(dec[i]),
		             xyz + 3*i);
	}

	for (i=0; i<n; i++)  // dumb average
		for (j=0; j<3; j++) 
			xyz_mean[j] += xyz[3*i+j];

	for (j=0; j<3; j++)  // reproject onto sphere
		norm += xyz_mean[j]*xyz_mean[j];
	norm = sqrt(norm);

	for (j=0; j<3; j++) 
		xyz_mean[j] /= norm;

	for (i=0; i<n; i++) { // find largest distance from average
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
		t->image = NULL;
		t->ref = NULL;
		t->dist2 = NULL;
		t->state &= ~TWEAK_HAS_CORRESPONDENCES;
	} else {
		assert(!t->image);
		assert(!t->ref);
		assert(!t->dist2);
	}
	assert(!t->image);
	assert(!t->ref);
	assert(!t->dist2);
}

void tweak_clear_on_sip_change(tweak_t* t)
{
	tweak_clear_correspondences(t);
	tweak_clear_image_ad(t);
	tweak_clear_ref_xy(t);
	tweak_clear_image_xyz(t);

}

void tweak_clear_ref_xy(tweak_t* t)  // ref_xy are the catalog star positions in image coordinates
{
	if (t->state & TWEAK_HAS_REF_XY) {
		assert(t->x_ref);
		free(t->x_ref);
		assert(t->y_ref);
		t->x_ref = NULL;
		free(t->y_ref);
		t->y_ref = NULL;
		t->state &= ~TWEAK_HAS_REF_XY;

	} else {
		assert(!t->x_ref);
		assert(!t->y_ref);
	}
}

void tweak_clear_ref_ad(tweak_t* t) // radec of catalog stars
{
	if (t->state & TWEAK_HAS_REF_AD) {
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

	} else {
		assert(!t->a_ref);
		assert(!t->d_ref);
	}
}

void tweak_clear_image_ad(tweak_t* t) // source (image) objs in ra,dec according to current tweak
{
	if (t->state & TWEAK_HAS_IMAGE_AD) {
		assert(t->a);
		free(t->a);
		t->a = NULL;
		assert(t->d);
		free(t->d);
		t->d = NULL;

		t->state &= ~TWEAK_HAS_IMAGE_AD;

	} else {
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

void tweak_push_ref_xyz(tweak_t* t, double* xyz, int n) // tell us (from outside tweak) where the catalog stars are
{
   double *ra,*dec;
	int i;

	tweak_clear_ref_ad(t);

	assert(xyz);
	assert(n);

	assert(!t->xyz_ref); // no leaky
	t->xyz_ref = malloc(sizeof(double)*3*n);
	memcpy(t->xyz_ref, xyz, 3*n*sizeof(double));

	ra = malloc(sizeof(double)*n);
	dec = malloc(sizeof(double)*n);
	assert(ra);
	assert(dec);

	for (i=0; i<n; i++) { // fill em up
		double *pt = xyz+3*i;
		ra[i] = rad2deg(xy2ra(pt[0],pt[1]));
		dec[i] = rad2deg(z2dec(pt[2]));
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

	t->x = malloc(sizeof(double)*n);
	t->y = malloc(sizeof(double)*n);
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

void tweak_skip_shift(tweak_t* t) {
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
}

// Dualtree rangesearch callback for distance calc. this should be integrated
// with real dualtree rangesearch.
double dtrs_dist2_callback(void* p1, void* p2, int D)
{
	double accum = 0;
	double* pp1=p1, *pp2=p2;
	int i;
	for (i=0; i<D; i++) {
		double delta = pp1[i]-pp2[i];
		accum += delta*delta;
	}
	return accum;
}

// The jitter is in radians
void find_correspondences(tweak_t* t, double jitter)  // actually call the dualtree
{
	double* data_image = malloc(sizeof(double)*t->n*3);
	double* data_ref = malloc(sizeof(double)*t->n_ref*3);

	assert(t->state & TWEAK_HAS_IMAGE_XYZ);
	assert(t->state & TWEAK_HAS_REF_XYZ);
	tweak_clear_correspondences(t);

	memcpy(data_image, t->xyz,     3*t->n*sizeof(double));
	memcpy(data_ref,   t->xyz_ref, 3*t->n_ref*sizeof(double));

	t->kd_image = kdtree_build(NULL, data_image, t->n, 3, 4, KDTT_DOUBLE,
	                           KD_BUILD_BBOX);

	t->kd_ref = kdtree_build(NULL, data_ref, t->n_ref, 3, 4, KDTT_DOUBLE,
                                 KD_BUILD_BBOX);

	// Storage for correspondences
	t->image = il_new(600);
	t->ref = il_new(600);
	t->dist2 = dl_new(600);
	t->included = il_new(600);

	fprintf(stderr,"jitter=%g arcsec / %g arcmin / %g deg\n",
			rad2arcsec(jitter), rad2arcmin(jitter), rad2deg(jitter));

	// Find closest neighbours
	dualtree_rangesearch(t->kd_image, t->kd_ref,
	                     RANGESEARCH_NO_LIMIT, jitter, // This min/max dist is in radians
	                     dtrs_dist2_callback,  // specify callback as the above func
	                     dtrs_match_callback, t,
	                     NULL, NULL);

	kdtree_free(t->kd_image);
	kdtree_free(t->kd_ref);
	t->kd_image = NULL;
	t->kd_ref = NULL;
	free(data_image);
	free(data_ref);

	fprintf(stderr,"correspondences=%d\n", il_size(t->dist2)); 
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

	int hp = radectohealpix(deg2rad(ra_mean), deg2rad(dec_mean), t->Nside); 
	kdtree_t* kd;
	if (cached_kd_hp != hp || cached_kd == NULL) {
		char buf[1000];
		snprintf(buf,1000, t->hppath, hp);
		fprintf(stderr,"opening %s\n",buf);
		kd = kdtree_fits_read(buf, NULL);
		fprintf(stderr,"success\n");
		assert(kd);
		cached_kd_hp = hp;
		cached_kd = kd;
	} else {
		kd = cached_kd;
	}

	radec2xyzarr(deg2rad(ra_mean), deg2rad(dec_mean), xyz);
	//radec2xyzarr(deg2rad(158.70829), deg2rad(51.919442), xyz);

	// Fudge radius factor because if the shift is really big, then we
	// can't actually find the correct astrometry.
	radius_factor = 1.3;
	kq = kdtree_rangesearch(kd, xyz,radius*radius*radius_factor);
	fprintf(stderr,"Did range search got %u stars\n", kq->nres);

	// No stars? That's bad. Run away.
	if (!kq->nres) {
	  fprintf(stderr,"Bad news. tweak_internal: Got no stars in get_reference_stars\n");
		return;
	}
	tweak_push_ref_xyz(t, kq->results.d, kq->nres);

	kdtree_free_query(kq);
}

// in arcseconds^2 on the sky (chi-sq)
double figure_of_merit(tweak_t* t) 
{

	// works on the sky
	double sqerr = 0.0;
	int i;
	for (i=0; i<il_size(t->image); i++) { // t->image is a list of source objs with current ref correspondences
		double a,d;
		double xyzpt[3];
		double xyzpt_ref[3];
		double xyzerr[3];
		sip_pixelxy2radec(t->sip, t->x[il_get(t->image, i)],
				t->y[il_get(t->image, i)], &a, &d);
		// xref and yref should be intermediate WC's not image x and y!
		radecdeg2xyzarr(a, d, xyzpt);
		radecdeg2xyzarr(t->a_ref[il_get(t->ref, i)],  // t->ref is a list of ref objs with current source correspn.
				t->d_ref[il_get(t->ref, i)], xyzpt_ref);

		xyzerr[0] = xyzpt[0]-xyzpt_ref[0];
		xyzerr[1] = xyzpt[1]-xyzpt_ref[1];
		xyzerr[2] = xyzpt[2]-xyzpt_ref[2];
		sqerr += xyzerr[0]*xyzerr[0] + xyzerr[1]*xyzerr[1] + xyzerr[2]*xyzerr[2];  
	}
	return rad2arcsec(1)*rad2arcsec(1)*sqerr;

}

// in pixels^2 in the image
double figure_of_merit2(tweak_t* t) 
{
	// works on the pixel coordinates
	double sqerr = 0.0;
	int i;
	for (i=0; i<il_size(t->image); i++) {
		double x,y,dx,dy;
		sip_radec2pixelxy(t->sip, t->a_ref[il_get(t->ref, i)], t->d_ref[il_get(t->ref, i)], &x, &y);
		dx = t->x[il_get(t->image, i)] - x;
		dy = t->y[il_get(t->image, i)] - y;
		sqerr += dx*dx + dy*dy;
	}
	return 3600*3600*sqerr*fabs(sip_det_cd(t->sip)); // convert this to units of arcseconds^2, sketchy
}

// I apologize for the rampant copying and pasting of the polynomial calcs...
void invert_sip_polynomial(tweak_t* t)
{
  // basic idea: lay down a grid in image, for each gridpoint, push through the polynomial 
  // to get yourself into warped image coordinate (but not yet lifted onto the sky)
  // then, using the set of warped gridpoints as inputs, fit back to their original grid locations as targets

   int inv_sip_order,ngrid,inv_sip_coeffs,stride;
	double *A,*A2,*b,*b2;
	double maxu,maxv,minu,minv;
	int i,gu, gv;

	assert(t->sip->a_order == t->sip->b_order);
	assert(t->sip->ap_order == t->sip->bp_order);

	inv_sip_order = t->sip->ap_order;

	ngrid = 10*(t->sip->ap_order+1);
	fprintf(stderr,"tweak inversion using %u gridpoints\n",ngrid);

	// The SIP coefficients form a order x order upper triangular matrix missing
	// the 0,0 element. We limit ourselves to a order 10 SIP distortion. 
	// That's why the computation of the number of SIP terms is calculated by
	// Gauss's arithmetic sum: sip_terms = (sip_order+1)*(sip_order+2)/2
	// The in the end, we drop the p^0q^0 term by integrating it into crpix)
	inv_sip_coeffs = (inv_sip_order+1)*(inv_sip_order+2)/2; // upper triangle

	stride = ngrid*ngrid; // Number of rows
	A  = malloc(inv_sip_coeffs*stride*sizeof(double));
	b  = malloc(2*stride*sizeof(double));
	A2 = malloc(inv_sip_coeffs*stride*sizeof(double));
	b2 = malloc(2*stride*sizeof(double));
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
	minu = minv =  1e100;
	maxu = maxv = -1e100;

	for (i=0; i<t->n; i++) {
		minu = dblmin(minu, t->x[i] - t->sip->wcstan.crpix[0]);
		minv = dblmin(minv, t->y[i] - t->sip->wcstan.crpix[1]);
		maxu = dblmax(maxu, t->x[i] - t->sip->wcstan.crpix[0]);
		maxv = dblmax(maxv, t->y[i] - t->sip->wcstan.crpix[1]);
	}
	fprintf(stderr,"maxu=%lf, minu=%lf\n", maxu, minu);
	fprintf(stderr,"maxv=%lf, minv=%lf\n", maxv, minv);

	// Fill A in column-major order for fortran dgelsd
	// We just make a big grid and hope for the best
	i = 0;
	for (gu=0; gu<ngrid; gu++) { 
	  for (gv=0; gv<ngrid; gv++) {

		 // Calculate grid position in original image pixels
		 double u = (gu * (maxu-minu) / ngrid) + minu; // now in pixels
		 double v = (gv * (maxv-minv) / ngrid) + minv;  // now in pixels

		 double U,V,fuv,guv;
		 int p,q,j;
		 sip_calc_distortion(t->sip, u, v, &U, &V); // computes U=u+f(u,v) and V=v+g(u,v)
		 fuv = U-u;
		 guv = V-v;

		 // Calculate polynomial terms but this time for inverse
		 // j = 0;
		 /*
			Maybe we want to explicitly set the (0,0) term to zero...:
		 */
		 A[i + stride*0] = 0;
		 j = 1;
		 for (p=0; p<=inv_sip_order; p++)
			for (q=0; q<=inv_sip_order; q++)
			  if (p+q <= inv_sip_order && !(p==0&&q==0)) {
				 // we're skipping the (0, 0) term.
				 //assert(j < (inv_sip_coeffs-1));
				 assert(j < inv_sip_coeffs);
				 A[i + stride*j] = pow(U,(double)p)*pow(V,(double)q);
				 j++;
			  }
		 //assert(j == (inv_sip_coeffs-1));
		 assert(j == inv_sip_coeffs);

			b[i] = -fuv;
			b[i+stride] = -guv;
			i++;
		}
	}

	// Save for sanity check
	memcpy(A2,A,sizeof(double)*stride*inv_sip_coeffs);
	memcpy(b2,b,sizeof(double)*stride*2);

	// Allocate work areas and answers
   {
	integer N=inv_sip_coeffs;          // nr cols of A
	doublereal S[N];      // min(N,M); is singular vals of A in dec order
	doublereal RCOND=-1.; // used to determine effective rank of A; -1
	                      // means use machine precision
	integer NRHS=2;       // number of right hand sides (one for x and y)
	integer rank;         // output effective rank of A
	integer lwork = 1000*1000; /// FIXME ???
	doublereal* work = malloc(lwork*sizeof(double));
	integer *iwork = malloc(lwork*sizeof(long int));
	integer info;
	int p, q, j;
	double chisq;
	integer str = stride;
	integer lda = stride;
	integer ldb = stride;
	dgelsd_(&str, &N, &NRHS, A, &lda, b, &ldb, S, &RCOND, &rank, work,
			  &lwork, iwork, &info); // make the jump to lightspeed
	stride = str;
	free(work);
	free(iwork);

	// Extract the inverted SIP coefficients
	//j = 0;
	j = 1;
	for (p=0; p<=inv_sip_order; p++)
	  for (q=0; q<=inv_sip_order; q++)
		 if (p+q <= inv_sip_order && !(p==0&&q==0)) {
			assert(j < inv_sip_coeffs);
			t->sip->ap[p][q] = b[j]; // dgelsd in its wisdom, stores its answer by overwriting your input b (nice!)
			t->sip->bp[p][q] = b[stride+j];
			j++;
		 }
	assert(j == inv_sip_coeffs);

	// Calculate chi2 for sanity
	chisq=0;
	for(i=0; i<stride; i++) {
	  double sum=0;
	  int j2;
	  for(j2=0; j2<inv_sip_coeffs; j2++) 
		 sum += A2[i+stride*j2]*b[j2];
	  chisq += (sum-b2[i])*(sum-b2[i]);
	  sum=0;
	  for(j2=0; j2<inv_sip_coeffs; j2++) 
		 sum += A2[i+stride*j2]*b[j2+stride];
	  chisq += (sum-b2[i+stride])*(sum-b2[i+stride]);
	}
	fprintf(stderr,"sip_invert_chisq=%lf\n",chisq);
	fprintf(stderr,"sip_invert_chisq/%d=%lf\n",ngrid*ngrid,(chisq/ngrid)/ngrid);
	fprintf(stderr,"sip_invert_sqrt(chisq/%d=%lf)\n",ngrid*ngrid,sqrt((chisq/ngrid)/ngrid));
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

// Run a polynomial tweak
void do_linear_tweak(tweak_t* t) // bad name for this function
{
   int sip_order,sip_coeffs,stride;
   double *UVP,*UVP2,*b,*b2;
	double xyzcrval[3];
	double cdi[2][2];
   double inv_det;
	double sU, sV,su,sv;
	double chisq;
	sip_t* swcs;
   int i;
	// a_order and b_order should be the same!
	assert(t->sip->a_order == t->sip->b_order);
	sip_order = t->sip->a_order;
	// The SIP coefficients form an (order x order) upper triangular matrix missing
	// the 0,0 element. We limit ourselves to an order 10 SIP distortion. 
	// That's why the number of SIP terms is calculated by
	// Gauss's arithmetic sum: sip_terms = (sip_order+1)*(sip_order+2)/2
	// Then in the end, we drop the p^0q^0 term by integrating it into crpix)
	sip_coeffs = (sip_order+1)*(sip_order+2)/2; // upper triangle

	stride = il_size(t->image); // number of rows
	UVP = malloc((2+sip_coeffs)*stride*sizeof(double));
	b = malloc(2*stride*sizeof(double));
	UVP2 = malloc((2+sip_coeffs)*stride*sizeof(double));
	b2 = malloc(2*stride*sizeof(double));
	assert(UVP);
	assert(b);
	assert(UVP2);
	assert(b2);

	fprintf(stderr,"sqerr=%le [arcsec^2]\n", figure_of_merit(t));
	sip_print(t->sip);
	//	fprintf(stderr,"sqerrxy=%le\n", figure_of_merit2(t));

	// We use a clever trick to estimate CD, A, and B terms in two
	// seperated least squares fits, then finding A and B by multiplying
	// the found parameters by CD inverse.
	//
	// Rearranging the SIP equations (see sip.h) we get the following
	// matrix operation to compute x and y in world intermediate
	// coordinates, which is convienently written in a way which allows
	// least squares estimation of CD and terms related to A and B.
	//
	// First use the x's to find the first set of parametetrs
	//
	//   +-------------- Intermediate world coordinates in DEGREES
	//   |    +--------- Pixel coordinates u and v in PIXELS
	//   |    |     +--- Polynomial u,v terms in powers of PIXELS
	//   v    v     v
	//   x1 = u1 v1 p1 * cd11            : cd11 is a scalar, degrees per pixel
	//   x2   u2 v2 p2   cd12            : cd12 is a scalar, degrees per pixel
	//   x3   u3 v3 p3   cd11*A + cd12*B : cd11*A and cs12*B are mixture of SIP terms (A,B) and CD matrix (cd11,cd12)
	//   ...
	// 
	// Then find cd21 and cd22 with the y's
	//
	//   y1 = u1 v1 p1 * cd21            : scalar, degrees per pixel   
	//   y2   u2 v2 p2   cd22            : scalar, degrees per pixel   
	//   y3   u3 v3 p3   cd21*A + cd22*B : mixture of SIP terms and CD 
	//   ...
	//
	// These are both standard least squares problems which we solve by
	// netlib's dgelsd. i.e. min_{cd,A,B} || x - [u,v,p]*[cd;cdA+cdB]||^2 with x reference, cd,A,B
	// unrolled parameters.
	//
   // after the call to dgelsd, we get back (for x) a vector of optimal [cd11;cd12; cd11*A + cd12*B]
   // Now we can pull out cd11 and cd12 from the beginning of this vector, 
	// and call the rest of the vector [cd11*A] + [cd12*B];
   // similarly for the y fit, we get back a vector of optimal [cd21;cd22; cd21*A + cd22*B]
   // once we have all those we can figure out A and B as follows
	//                  -1
	//   A' = [cd11 cd12]    *  [cd11*A' + cd12*B']
	//   B'   [cd21 cd22]       [cd21*A' + cd22*B']
	//
	// which recovers the A and B's.


	// fill in the UVP matrix, stride in this case is the number of correspondences
	radecdeg2xyzarr(t->sip->wcstan.crval[0], t->sip->wcstan.crval[1], xyzcrval);
	for (i=0; i<stride; i++) {
	   int j,p,q;
		int refi;
		double x,y;
		double xyzpt[3];
		double u = t->x[il_get(t->image, i)] - t->sip->wcstan.crpix[0];
		double v = t->y[il_get(t->image, i)] - t->sip->wcstan.crpix[1];

		UVP[i + stride*0] = u; // first two cols are special
		UVP[i + stride*1] = v;

		// Poly terms for SIP.  Note that this includes the constant
		// shift parameter (SIP term 0,0) which we extract and apply to
		// crpix (and don't include in SIP terms)
		j = 2;  // we already filled [i+stride*0] with u and [i*stride*1] with v
		for (p=0; p<=sip_order; p++)
			for (q=0; q<=sip_order; q++)
				if (p+q <= sip_order) {
					assert(2 <= j);
					assert(j < 2+sip_coeffs);
					if (p+q != 1) 
						UVP[i + stride*j] = pow(u,(double)p) * pow(v,(double)q);
					else
						// We don't want repeated
						// linear terms
						UVP[i + stride*j] = 0.0;
					j++;
				}
		assert(j == 2+sip_coeffs);

		// DEBUG - Be sure about shift coefficient
		assert(UVP[i + stride*2] == 1.0);

		// xref and yref should be intermediate WC's not image x and y!
		refi = il_get(t->ref, i);
		radecdeg2xyzarr(t->a_ref[refi], t->d_ref[refi], xyzpt);
		star_coords(xyzpt, xyzcrval, &y, &x); // find tangent in intermediate coords

		b[i + stride*0] = rad2deg(x);
		b[i + stride*1] = rad2deg(y);
	}

	// Save UVP, bx, and by for computing chisq
	memcpy(UVP2,UVP,sizeof(double)*stride*(2+sip_coeffs));
	memcpy(b2,b,sizeof(double)*stride*2);

	// Allocate work areas and answers
   {
	integer N=2+sip_coeffs;          // nr cols of UVP
	doublereal S[N];      // min(N,M); is singular vals of UVP in dec order
	doublereal RCOND=-1.; // used to determine effective rank of UVP; -1
	                      // means use machine precision
	integer NRHS=2;       // number of right hand sides (one for x and y)
	integer rank;         // output effective rank of UVP
	integer lwork = 1000*1000; /// FIXME ???
	doublereal* work = malloc(lwork*sizeof(doublereal));
	integer *iwork = malloc(lwork*sizeof(integer));
	integer info;
	integer str = stride;
	integer lda = stride;
	integer ldb = stride;
	dgelsd_(&str, &N, &NRHS, UVP, &lda, b, &ldb, S, &RCOND, &rank, work,
			  &lwork, iwork, &info); // punch it chewey
	free(work);
	free(iwork);
	}

	t->sip->wcstan.cd[0][0] = b[0 + stride*0]; // b is replaced with CD during dgelsd
	t->sip->wcstan.cd[0][1] = b[1 + stride*0];
	t->sip->wcstan.cd[1][0] = b[0 + stride*1];
	t->sip->wcstan.cd[1][1] = b[1 + stride*1];

	// Extract the SIP coefficients
	inv_det = 1.0/sip_det_cd(t->sip);
	cdi[0][0] =  t->sip->wcstan.cd[1][1] * inv_det;
	cdi[0][1] = -t->sip->wcstan.cd[0][1] * inv_det;
	cdi[1][0] = -t->sip->wcstan.cd[1][0] * inv_det;
	cdi[1][1] =  t->sip->wcstan.cd[0][0] * inv_det;

	// This magic *3* is here because we skip the (0, 0) term.
   {
	int p, q, j;
	j = 3;
	for (p=0; p<=sip_order; p++)
		for (q=0; q<=sip_order; q++)
			if (p+q <= sip_order && !(p==0&&q==0)) {
				assert(2 <= j);
				assert(j < 2+sip_coeffs);
				t->sip->a[p][q] =
					cdi[0][0] * b[j + stride*0] +
					cdi[0][1] * b[j + stride*1];
				t->sip->b[p][q] =
					cdi[1][0] * b[j + stride*0] +
					cdi[1][1] * b[j + stride*1];
				j++;
			}
	assert(j == 2+sip_coeffs);
	}
	// Since the linear terms should be zero, and we set the coefficients
	// that way, we don't know what the optimizer set them to. Thus,
	// explicitly set them to zero here.

	t->sip->a_order = sip_order;
	t->sip->b_order = sip_order;
	t->sip->a[0][0] = 0.0; // we are going to put the shift into the wcs
	t->sip->b[0][0] = 0.0;
	t->sip->a[0][1] = 0.0; // we put the linear terms into CD
	t->sip->a[1][0] = 0.0;
	t->sip->b[0][1] = 0.0;
	t->sip->b[1][0] = 0.0;

	invert_sip_polynomial(t);

	// Grab the shift, put it into the wcs
	sU = cdi[0][0]*b[2 + stride*0] + cdi[0][1]*b[2 + stride*1];
	sV = cdi[1][0]*b[2 + stride*0] + cdi[1][1]*b[2 + stride*1];
	sip_calc_inv_distortion(t->sip, sU, sV, &su, &sv);
	swcs = wcs_shift(t->sip, -su, -sv);
	sip_free(t->sip);
	t->sip = swcs;

	fprintf(stderr,"New sip header:\n");
	sip_print(t->sip);
	fprintf(stderr,"shiftxun=%le, shiftyun=%le\n",sU, sV);
	fprintf(stderr,"shiftx=%le, shifty=%le\n",su, sv);
	fprintf(stderr,"sqerr=%le\n", figure_of_merit(t));
	fprintf(stderr,"rms=%lf\n", sqrt(figure_of_merit(t)/stride));
	//	fprintf(stderr,"sqerrxy=%le\n", figure_of_merit2(t));

	// Calculate chi2 for sanity
	chisq=0;
	for(i=0; i<stride; i++) {
		double sum=0;
		int j;
		for(j=0; j<6; j++) 
			sum += UVP2[i+stride*j]*b[j];
		chisq += (sum-b2[i])*(sum-b2[i]);
	}
	//	fprintf(stderr,"sqerrxy=%le (CHISQ matrix)\n", chisq);

	free(UVP);
	free(b);
	free(UVP2);
	free(b2);
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

unsigned int tweak_advance_to(tweak_t* t, unsigned int flag) {
	//	fprintf(stderr,"WANT: ");
	//	tweak_print_the_state(flag);
	//	fprintf(stderr,"\n");
	want(TWEAK_HAS_IMAGE_AD) {
		int jj;
		ensure(TWEAK_HAS_SIP);
		ensure(TWEAK_HAS_IMAGE_XY);

		fprintf(stderr,"Satisfying TWEAK_HAS_IMAGE_AD\n");

		// Convert to ra dec
		assert(!t->a);
		assert(!t->d);
		t->a = malloc(sizeof(double)*t->n);
		t->d = malloc(sizeof(double)*t->n);
		for (jj=0; jj<t->n; jj++) {
			sip_pixelxy2radec(t->sip, t->x[jj], t->y[jj], t->a+jj, t->d+jj);
		}

		done(TWEAK_HAS_IMAGE_AD);
	}

	want(TWEAK_HAS_REF_AD) {
		ensure(TWEAK_HAS_REF_XYZ);

		// FIXME
		fprintf(stderr,"Satisfying TWEAK_HAS_REF_AD\n");
		fprintf(stderr,"FIXME!\n");

		done(TWEAK_HAS_REF_AD);
	}
			
	want(TWEAK_HAS_REF_XY) {
		int jj;
		ensure(TWEAK_HAS_REF_AD);

		//tweak_clear_ref_xy(t);

		fprintf(stderr,"Satisfying TWEAK_HAS_REF_XY\n");
		assert(t->state & TWEAK_HAS_REF_AD);
		assert(t->n_ref);
		assert(!t->x_ref);
		assert(!t->y_ref);
		t->x_ref = malloc(sizeof(double)*t->n_ref);
		t->y_ref = malloc(sizeof(double)*t->n_ref);
		for (jj=0; jj<t->n_ref; jj++) {
			sip_radec2pixelxy(t->sip, t->a_ref[jj], t->d_ref[jj],
				      t->x_ref+jj, t->y_ref+jj);
			//fprintf(stderr,"ref star %04d: %g,%g\n",jj,t->x_ref[jj],t->y_ref[jj]);
		}

		done(TWEAK_HAS_REF_XY);
	}

	want(TWEAK_HAS_AD_BAR_AND_R) {
		ensure(TWEAK_HAS_IMAGE_AD);

		fprintf(stderr,"Satisfying TWEAK_HAS_AD_BAR_AND_R\n");

		assert(t->state & TWEAK_HAS_IMAGE_AD);
		get_center_and_radius(t->a, t->d, t->n, 
		                      &t->a_bar, &t->d_bar, &t->radius);
		fprintf(stderr,"a_bar=%lf [deg], d_bar=%lf [deg], radius=%lf [arcmin]\n",
				t->a_bar, t->d_bar, rad2arcmin(t->radius));

		done(TWEAK_HAS_AD_BAR_AND_R);
	}

	want(TWEAK_HAS_IMAGE_XYZ) {
		int i;
		ensure(TWEAK_HAS_IMAGE_AD);

		fprintf(stderr,"Satisfying TWEAK_HAS_IMAGE_XYZ\n");
		assert(!t->xyz);

		t->xyz = malloc(3*t->n*sizeof(double));
		for (i=0; i<t->n; i++) {
			radec2xyzarr(deg2rad(t->a[i]),
				     deg2rad(t->d[i]),
				     t->xyz + 3*i);
		}
		done(TWEAK_HAS_IMAGE_XYZ);
	}

	want(TWEAK_HAS_REF_XYZ) {
		if (t->state & TWEAK_HAS_HEALPIX_PATH) {
			ensure(TWEAK_HAS_AD_BAR_AND_R);
			fprintf(stderr,"Satisfying TWEAK_HAS_REF_XYZ\n");
			get_reference_stars(t);
		} else {
			ensure(TWEAK_HAS_REF_AD);

			fprintf(stderr,"Satisfying TWEAK_HAS_REF_XYZ\n");
			fprintf(stderr,"FIXME!\n");

			// try a conversion
			assert(0);
		}

		done(TWEAK_HAS_REF_XYZ);
	}

	want(TWEAK_HAS_COARSLY_SHIFTED) {
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_IMAGE_XY);

		fprintf(stderr,"Satisfying TWEAK_HAS_COARSLY_SHIFTED\n");

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

		fprintf(stderr,"Satisfying TWEAK_HAS_FINELY_SHIFTED\n");

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

		fprintf(stderr,"Satisfying TWEAK_HAS_REALLY_FINELY_SHIFTED\n");

		// Shrink size of hough box
		do_entire_shift_operation(t, 0.03);
		tweak_clear_image_ad(t);
		tweak_clear_ref_xy(t);

		done(TWEAK_HAS_REALLY_FINELY_SHIFTED);
	}

	want(TWEAK_HAS_CORRESPONDENCES) {
		ensure(TWEAK_HAS_REF_XYZ);
		ensure(TWEAK_HAS_IMAGE_XYZ);

		fprintf(stderr,"Satisfying TWEAK_HAS_CORRESPONDENCES\n");

		find_correspondences(t, arcsec2rad(t->jitter));

		done(TWEAK_HAS_CORRESPONDENCES);
	}

	want(TWEAK_HAS_LINEAR_CD) {
		ensure(TWEAK_HAS_SIP);
		ensure(TWEAK_HAS_REALLY_FINELY_SHIFTED);
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_REF_AD);
		ensure(TWEAK_HAS_IMAGE_XY);
		ensure(TWEAK_HAS_CORRESPONDENCES);

		fprintf(stderr,"Satisfying TWEAK_HAS_LINEAR_CD\n");

		do_linear_tweak(t);

		tweak_clear_on_sip_change(t);

		done(TWEAK_HAS_LINEAR_CD);
	}

	fprintf(stderr,"die for dependence: "); 
	tweak_print_the_state(flag);
	fprintf(stderr,"\n"); 
	assert(0);
	return -1;
}

void tweak_push_wcs_tan(tweak_t* t, tan_t* wcs) {
	if (!t->sip) {
		t->sip = sip_create();
	}
	memcpy(&(t->sip->wcstan), wcs, sizeof(tan_t));
	t->state |= TWEAK_HAS_SIP;
}

void tweak_go_to(tweak_t* t, unsigned int dest_state)
{
	while(! (t->state & dest_state))
		tweak_advance_to(t, dest_state);
}

#define SAFE_FREE(xx) if ((xx)) {free((xx)); xx = NULL;}
void tweak_clear(tweak_t* t)
{
	// FIXME
	if (cached_kd)
		kdtree_fits_close(cached_kd);

	// FIXME this should free stuff
	if (!t) return;
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
	il_free(t->maybeinliers);
	il_free(t->bestinliers);
	il_free(t->included);
	kdtree_free(t->kd_image);
	kdtree_free(t->kd_ref);
	SAFE_FREE(t->hppath);
}

void tweak_free(tweak_t* t) {
	tweak_clear(t);
	free(t);
}
