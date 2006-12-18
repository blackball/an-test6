#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "tweak_internal.h"
#include "healpix.h"
#include "dualtree_rangesearch.h"
#include "kdtree_fits_io.h"
#include "ezfits.h"

typedef double doublereal;
typedef long int integer;
extern int dgelsd_(integer *m, integer *n, integer *nrhs, doublereal *a,
		integer *lda, doublereal *b, integer *ldb, doublereal *s,
		doublereal *rcond, integer *rank, doublereal *work, integer
		*lwork, integer *iwork, integer *info);

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

void get_dydx_range(double* ximg, double* yimg, int nimg,
                    double* xcat, double* ycat, int ncat, 
                    double *mindx, double *mindy, double *maxdx, double *maxdy)
{
	*maxdx = -1e100;
	*mindx = 1e100;
	*maxdy = -1e100;
	*mindy = 1e100;

	int i, j;
	for (i=0; i<nimg; i++) {
		for (j=0; j<ncat; j++) {
			double dx = ximg[i]-xcat[j];
			double dy = yimg[i]-ycat[j];
			*maxdx = max(dx,*maxdx);
			*maxdy = max(dy,*maxdy);
			*mindx = min(dx,*mindx);
			*mindy = min(dy,*mindy);
		}
	}
}

void get_shift(double* ximg, double* yimg, int nimg,
               double* xcat, double* ycat, int ncat, 
	       double mindx, double mindy, double maxdx, double maxdy,
               double* xshift, double* yshift)
{

	int i, j;

	// hough transform 
	int hsz = 1000; // hough size
	int *hough = malloc(hsz*hsz*sizeof(double));
	for (i=0; i<hsz*hsz; i++)
		hough[i] = 0;

	for (i=0; i<nimg; i++) {
		for (j=0; j<ncat; j++) {
			double dx = ximg[i]-xcat[j];
			double dy = yimg[i]-ycat[j];
			int hszi = hsz-1;
			int iy = hszi*( (dy-mindy)/(maxdy-mindy) );
			int ix = hszi*( (dx-mindx)/(maxdx-mindx) );

			// check to make sure the point is in the box
//			if ( !(iy >=0) || !(ix >=0) || !(iy*hsz+ ix < hsz*hsz))
//			if ( !(iy >=0) || !(ix >=0) || !(iy*hsz+ ix < hsz*hsz) ||
//			if ( iy < 0 || ix < 0 || iy > hsz || ix > hsz )
//				continue;
//			assert (iy >=0);
//			assert (ix >=0);
//			assert (iy*hsz+ ix < hsz*hsz);
			if (0 < iy && iy < hsz+1 &&
					0 < ix && ix < hsz+1) {
				// approx gauss
				// FIMXE use a better approx: see hogg ticket
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

	FILE* ff = fopen("hough.dat", "w");
	fwrite(hough, sizeof(double), hsz*hsz, ff);
	fclose(ff);


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
	fprintf(stderr, "get_shift: mindx=%lf, maxdx=%lf, mindy=%lf, maxdy=%lf\n", mindx, maxdx, mindy, maxdy);
	fprintf(stderr, "get_shift: xs=%lf, ys=%lf\n", *xshift, *yshift);

	static char c = '1';
	static char fn[] = "houghN.fits";
	fn[5] = c++;
	ezwriteimage(fn, TINT, hough, hsz, hsz);
}

// Take shift in image plane and do a switcharoo to make the wcs something
// better
sip_t* wcs_shift(sip_t* wcs, double xs, double ys)
{
	sip_t* swcs = malloc(sizeof(sip_t));
	memcpy(swcs, wcs, sizeof(sip_t));

	// Save
	double crpix0 = wcs->crpix[0];
	double crpix1 = wcs->crpix[1];

	wcs->crpix[0] += xs;
	wcs->crpix[1] += ys;

	// now reproject the old crpix[xy] into swcs
	double nxref, nyref;
	pixelxy2radec(wcs, crpix0, crpix1, &nxref, &nyref);

	swcs->crval[0] = nxref;
	swcs->crval[1] = nyref;

	// Restore
	wcs->crpix[0] = crpix0;
	wcs->crpix[1] = crpix1;

	return swcs;
}

sip_t* do_entire_shift_operation(tweak_t* t, double rho)
{
	get_shift(t->x, t->y, t->n,
		  t->x_ref, t->y_ref, t->n_ref,
		  rho*t->mindx, rho*t->mindy, rho*t->maxdx, rho*t->maxdy,
		  &t->xs, &t->ys);
	sip_t* swcs = wcs_shift(t->sip, t->xs, t->ys);
	free(t->sip);
	t->sip = swcs;
	printf("xshift=%lf, yshift=%lf\n", t->xs, t->ys);
	return NULL;
}

void tweak_init(tweak_t* t)
{

	t->n = 0;
	t->a = t->d = t->x = t->y = NULL;
	t->xyz = NULL;

	t->a_bar = t->d_bar = t->radius = 0.0;

	t->n_ref = 0;
	t->a_ref = t->d_ref = t->x_ref = t->y_ref = NULL;
	t->xyz_ref = NULL;

	// FIXME maybe these should be allocated at the outset
	t->image = t->ref = t->dist2 = NULL;

	t->maybeinliers = t->bestinliers = t->included = NULL;
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
	if (state & TWEAK_HAS_SIP            ) printf("TWEAK_HAS_SIP, ");
	if (state & TWEAK_HAS_IMAGE_XY       ) printf("TWEAK_HAS_IMAGE_XY, ");
	if (state & TWEAK_HAS_IMAGE_XYZ      ) printf("TWEAK_HAS_IMAGE_XYZ, ");
	if (state & TWEAK_HAS_IMAGE_AD       ) printf("TWEAK_HAS_IMAGE_AD, ");
	if (state & TWEAK_HAS_REF_XY         ) printf("TWEAK_HAS_REF_XY, ");
	if (state & TWEAK_HAS_REF_XYZ        ) printf("TWEAK_HAS_REF_XYZ, ");
	if (state & TWEAK_HAS_REF_AD         ) printf("TWEAK_HAS_REF_AD, ");
	if (state & TWEAK_HAS_AD_BAR_AND_R   ) printf("TWEAK_HAS_AD_BAR_AND_R, ");
	if (state & TWEAK_HAS_CORRESPONDENCES) printf("TWEAK_HAS_CORRESPONDENCES, ");
	if (state & TWEAK_HAS_RUN_OPT        ) printf("TWEAK_HAS_RUN_OPT, ");
	if (state & TWEAK_HAS_RUN_RANSAC_OPT ) printf("TWEAK_HAS_RUN_RANSAC_OPT, ");
	if (state & TWEAK_HAS_COARSLY_SHIFTED) printf("TWEAK_HAS_COARSLY_SHIFTED, ");
	if (state & TWEAK_HAS_FINELY_SHIFTED ) printf("TWEAK_HAS_FINELY_SHIFTED, ");
	if (state & TWEAK_HAS_REALLY_FINELY_SHIFTED ) printf("TWEAK_HAS_REALLY_FINELY_SHIFTED, ");
	if (state & TWEAK_HAS_HEALPIX_PATH   ) printf("TWEAK_HAS_HEALPIX_PATH, ");
	if (state & TWEAK_HAS_LINEAR_CD      ) printf("TWEAK_HAS_LINEAR_CD, ");
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
		USE_FILE("corr_im");
		cor_im = fopen(fn, "w");
		USE_FILE("corr_ref");
		cor_ref = fopen(fn, "w");
		USE_FILE("corr_delta");
		cor_delta = fopen(fn, "w");
		int i;
		for (i=0; i<il_size(t->image); i++) {
			double a,d;
			int im_ind = il_get(t->image, i);
			pixelxy2radec(t->sip, t->x[im_ind],t->y[im_ind], &a,&d);
			tweak_print4_fp(cor_im, t->x[im_ind], t->y[im_ind],
					a, d);

			int ref_ind = il_get(t->ref, i);
			tweak_print4_fp(cor_ref,
					t->x_ref[ref_ind], t->y_ref[ref_ind],
					t->a_ref[ref_ind], t->d_ref[ref_ind]);

			tweak_print4_fp(cor_delta,
					t->x[im_ind], t->y[im_ind],
					t->x_ref[ref_ind] - t->x[im_ind],
					t->y_ref[ref_ind] - t->y[im_ind]);
		}
		fclose(cor_im);
		fclose(cor_ref);
		fclose(cor_delta);
	}
	dump_nr++;
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
}

void tweak_clear_ref_xy(tweak_t* t)
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

void tweak_clear_ref_ad(tweak_t* t)
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

void tweak_clear_image_ad(tweak_t* t)
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

void tweak_push_ref_xyz(tweak_t* t, double* xyz, int n)
{
	tweak_clear_ref_ad(t);

	assert(xyz);
	assert(n);

	assert(!t->xyz_ref); // no leaky
	t->xyz_ref = malloc(sizeof(double)*3*n);
	memcpy(t->xyz_ref, xyz, 3*n*sizeof(double));

	double *ra = malloc(sizeof(double)*n);
	double *dec = malloc(sizeof(double)*n);
	assert(ra);
	assert(dec);

	int i;
	for (i=0; i<n; i++) {
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

void tweak_push_image_xy(tweak_t* t, double* x, double *y, int n)
{
	tweak_clear_image_xy(t);

	assert(n);

	t->x = malloc(sizeof(double)*n);
	t->y = malloc(sizeof(double)*n);
	memcpy(t->x, x, sizeof(double)*n);
	memcpy(t->y, y, sizeof(double)*n);

	t->state |= TWEAK_HAS_IMAGE_XY;
}

void tweak_push_hppath(tweak_t* t, char* hppath)
{
	t->hppath = strdup(hppath);
	t->state |= TWEAK_HAS_HEALPIX_PATH;
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

//	printf("found new one!: dx=%lf, dy=%lf, dist=%lf\n", dx,dy,sqrt(dx*dx+dy*dy));
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

void find_correspondences(tweak_t* t)
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

	double jitter = 2.0 /3600.0 * 180 / M_PI / 10.0/10.0/10.0;
	printf("jitter=%lf\n",jitter);

	// Find closest neighbours
	dualtree_rangesearch(t->kd_image, t->kd_ref,
	                     RANGESEARCH_NO_LIMIT, jitter, // This min/max dist is in radians
	                     dtrs_dist2_callback,
	                     dtrs_match_callback, t,
	                     NULL, NULL);

	printf("im=%d\n", il_size(t->image)); 
	printf("ref=%d\n", il_size(t->ref)); 
	printf("dist2=%d\n", il_size(t->dist2)); 
//	dl_print(t->dist2);
}


kdtree_t* cached_kd = NULL;
int cached_kd_hp = 0;

void get_reference_stars(tweak_t* t)
{
	double ra_mean = t->a_bar;
	double dec_mean = t->d_bar;
	double radius = t->radius;

	// FIXME magical 9 constant == an_cat hp res NSide
	int hp = radectohealpix(deg2rad(ra_mean), deg2rad(dec_mean), 9); 
	kdtree_t* kd;
	if (cached_kd_hp != hp || cached_kd == NULL) {
		char buf[1000];
		snprintf(buf,1000, t->hppath, hp);
		fprintf(stderr, "opening %s\n",buf);
		kd = kdtree_fits_read(buf, NULL);
		fprintf(stderr, "success\n");
		assert(kd);
		cached_kd_hp = hp;
		cached_kd = kd;
	} else {
		kd = cached_kd;
	}

	double xyz[3];
	radec2xyzarr(deg2rad(ra_mean), deg2rad(dec_mean), xyz);
	//radec2xyzarr(deg2rad(158.70829), deg2rad(51.919442), xyz);

	// Fudge radius factor because if the shift is really big, then we
	// can't actually find the correct astrometry.
	double radius_factor = 1.3;
	kdtree_qres_t* kq = kdtree_rangesearch(kd, xyz,
			radius*radius*radius_factor);
	fprintf(stderr, "Did range search got %u stars\n", kq->nres);

	// No stars? That's bad. Run away.
	if (!kq->nres)
		return;

	tweak_push_ref_xyz(t, kq->results.d, kq->nres);

	kdtree_free_query(kq);
}

// Run a linear tweak; only changes CD matrix
void do_linear_tweak(tweak_t* t)
{

	integer stride = 2*il_size(t->image);
	double* A = malloc(4*stride*sizeof(double));
	double* b = malloc(stride*sizeof(double));
	assert(A);
	assert(b);

	// Fill A in column-major order for fortran dgelsd
	// x1   u1 v1 0  0    cd11
	// y1 = 0  0  u1 v1 * cd12
	// x2   u2 v2 0  0    cd21
	// y2   0  0  u2 v2   cd22
	// ...
	// where we minimize cd, and x,y are from refs in intermediate world
	// coordinates. i.e. min_b || b - Ax||^2 with b refs, x unrolled cd
	int i;
	for (i=0; i<stride/2; i++) {
		A[2*i+0 + stride*0] = deg2rad(t->x[il_get(t->image, i)] - t->sip->crpix[0]);
		A[2*i+0 + stride*1] = deg2rad(t->y[il_get(t->image, i)] - t->sip->crpix[1]);
		A[2*i+1 + stride*2] = deg2rad(t->x[il_get(t->image, i)] - t->sip->crpix[0]);
		A[2*i+1 + stride*3] = deg2rad(t->y[il_get(t->image, i)] - t->sip->crpix[1]);

		A[2*i+1 + stride*0] = A[2*i+1 + stride*1] = A[2*i+0 + stride*2] =
			A[2*i+0 + stride*3] = 0.0;

		b[2*i+0] = deg2rad(t->x_ref[il_get(t->ref, i)] - t->sip->crpix[0]);
		b[2*i+1] = deg2rad(t->y_ref[il_get(t->ref, i)] - t->sip->crpix[1]);
	}

	// allocate work areas and answers
	
	doublereal S[4];
	doublereal RCOND=-1.; // nr right hand sides
	integer N=4; // nr cols of A
	integer NRHS=1; // nr right hand sides
	integer rank;
	integer lwork = 1000*1000;
	doublereal* work = malloc(lwork*sizeof(double));
	integer *iwork = malloc(lwork*sizeof(long int));
	integer info;
	dgelsd_(&stride, &N, &NRHS, A, &stride, b, &stride, S, &RCOND, &rank, work,
			&lwork, iwork, &info);

	t->sip->cd[0][0] = b[0]; // b is replaced with CD 
	t->sip->cd[0][1] = b[1];
	t->sip->cd[1][0] = b[2];
	t->sip->cd[1][1] = b[3];
}


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
	printf("WANT: ");
	tweak_print_the_state(flag);
	printf("\n");
	want(TWEAK_HAS_IMAGE_AD) {
		ensure(TWEAK_HAS_SIP);
		ensure(TWEAK_HAS_IMAGE_XY);

		// Convert to ra dec
		assert(!t->a);
		assert(!t->d);
		t->a = malloc(sizeof(double)*t->n);
		t->d = malloc(sizeof(double)*t->n);
		int jj;
		for (jj=0; jj<t->n; jj++) {
			pixelxy2radec(t->sip, t->x[jj], t->y[jj], t->a+jj, t->d+jj);
		}

		done(TWEAK_HAS_IMAGE_AD);
	}

	want(TWEAK_HAS_REF_AD) {
		ensure(TWEAK_HAS_REF_XYZ);

		// FIXME

		done(TWEAK_HAS_REF_AD);
	}
			
	want(TWEAK_HAS_REF_XY) {
		ensure(TWEAK_HAS_REF_AD);

		//tweak_clear_ref_xy(t);

		assert(t->state & TWEAK_HAS_REF_AD);
		assert(t->n_ref);
		assert(!t->x_ref);
		assert(!t->y_ref);
		t->x_ref = malloc(sizeof(double)*t->n_ref);
		t->y_ref = malloc(sizeof(double)*t->n_ref);
		int jj;
		for (jj=0; jj<t->n_ref; jj++) {
			radec2pixelxy(t->sip, t->a_ref[jj], t->d_ref[jj],
				      t->x_ref+jj, t->y_ref+jj);
		}

		done(TWEAK_HAS_REF_XY);
	}

	want(TWEAK_HAS_AD_BAR_AND_R) {
		ensure(TWEAK_HAS_IMAGE_AD);

		assert(t->state & TWEAK_HAS_IMAGE_AD);
		get_center_and_radius(t->a, t->d, t->n, 
		                      &t->a_bar, &t->d_bar, &t->radius);

		done(TWEAK_HAS_AD_BAR_AND_R);
	}

	want(TWEAK_HAS_IMAGE_XYZ) {
		ensure(TWEAK_HAS_IMAGE_AD);

		assert(!t->xyz);

		t->xyz = malloc(3*t->n*sizeof(double));
		int i;
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
			get_reference_stars(t);
		} else {
			ensure(TWEAK_HAS_REF_AD);

			// try a conversion
			assert(0);
		}

		done(TWEAK_HAS_REF_XYZ);
	}

	want(TWEAK_HAS_COARSLY_SHIFTED) {
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_IMAGE_XY);

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

		// Shrink size of hough box
		do_entire_shift_operation(t, 0.03);
		tweak_clear_image_ad(t);
		tweak_clear_ref_xy(t);

		done(TWEAK_HAS_REALLY_FINELY_SHIFTED);
	}

	want(TWEAK_HAS_CORRESPONDENCES) {
		ensure(TWEAK_HAS_REF_XYZ);
		ensure(TWEAK_HAS_IMAGE_XYZ);

		find_correspondences(t);

		done(TWEAK_HAS_CORRESPONDENCES);
	}

	want(TWEAK_HAS_LINEAR_CD) {
		ensure(TWEAK_HAS_SIP);
		ensure(TWEAK_HAS_REALLY_FINELY_SHIFTED);
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_REF_AD);
		ensure(TWEAK_HAS_IMAGE_XY);
		ensure(TWEAK_HAS_CORRESPONDENCES);

		do_linear_tweak(t);

		done(TWEAK_HAS_LINEAR_CD);
	}

	printf("die for dependence: "); 
	tweak_print_the_state(flag);
	printf("\n"); 
	assert(0);
}

void tweak_clear(tweak_t* t)
{
	// FIXME
	if (cached_kd)
		kdtree_fits_close(cached_kd);

	// FIXME this should free stuff
	tweak_init(t);
}
