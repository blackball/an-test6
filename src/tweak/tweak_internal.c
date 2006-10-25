#include "tweak_internal.h"

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
			assert (iy >=0);
			assert (ix >=0);
			assert (iy*hsz+ ix < hsz*hsz);
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
	fprintf(stderr, "get_shift: mindx=%lf, maxdx=%lf, mindy=%lf, maxdy=%lf\n", mindx, maxdx, mindy, maxdy,);
	fprintf(stderr, "get_shift: xs=%lf, ys=%lf\n", *xshift, *yshift);

	static char c = '1';
	static char fn[] = "houghN.fits";
	fn[5] = c++;
//	ezwriteimage(fn, TINT, hough, hsz, hsz);
}

tweak_t* tweak_new()
{
	tweak_t* t = malloc(sizeof(tweak_t));

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
	if (t->flags & TWEAK_HAS_CORRESPONDENCES) {
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
		t->flags &= !TWEAK_HAS_CORRESPONDENCES;
	} else {
		assert(!image);
		assert(!ref);
		assert(!dist2);
	}
}

void tweak_clear_ref_xy(tweak_t* t)
{
	if (t->flags & TWEAK_HAS_REF_XY) {
		assert(t->x_ref);
		free(t->x_ref);
		assert(t->y_ref);
		free(t->y_ref);
		t->flags &= !TWEAK_HAS_REF_XY;

	} else {
		assert(!t->x_ref);
		assert(!t->y_ref);
	}
}

void tweak_clear_ref_ad(tweak_t* t)
{
	if (t->flags & TWEAK_HAS_REF_AD) {
		assert(t->a_ref);
		free(t->a_ref);
		assert(t->d_ref);
		free(t->d_ref);
		t->n_ref = 0;

		tweak_clear_correspondences(t);
		tweak_clear_ref_xy(t);
		t->flags &= !TWEAK_HAS_REF_AD;

	} else {
		assert(!t->a_ref);
		assert(!t->d_ref);
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
	assert(*ra);
	assert(*dec);

	int i;
	for (i=0; i<n; i++) {
		double *pt = xyz+3*i;
		(*ra)[i] = rad2deg(xy2ra(pt[0],pt[1]));
		(*dec)[i] = rad2deg(z2dec(pt[2]));
	}

	t->a_ref = ra;
	t->d_ref = ra;
	t->n_ref = n;

	t->flags |= TWEAK_HAS_REF_AD;
	t->flags |= TWEAK_HAS_REF_XYZ;
}

// DualTree RangeSearch callback. We want to keep track of correspondences.
// Potentially the matching could be many-to-many; we allow this and hope the
// optimizer can take care of it.
void dtrs_match_callback(void* extra, int image_ind, int ref_ind, double dist2)
{
	tweak_t* t = extra;

	image_ind = t->kd_image->perm[image_ind];
	ref_ind = t->kd_ref->perm[ref_ind];

	double dx = t->x[image_ind] - t->x_ref[ref_ind];
	double dy = t->y[image_ind] - t->y_ref[ref_ind];

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

	assert(t->flags & TWEAK_HAS_IMAGE_XYZ);
	assert(t->flags & TWEAK_HAS_REF_XYZ);
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

	// Find closest neighbours
	dualtree_rangesearch(t->kd_image, t->kd_ref,
	                     RANGESEARCH_NO_LIMIT, 0.32, // This min/max dist is in pixels
	                     dtrs_dist2_callback,
	                     dtrs_match_callback, t,
	                     NULL, NULL);

	printf("im=%d\n", il_size(t->image)); 
	printf("ref=%d\n", il_size(t->ref)); 
	printf("dist2=%d\n", il_size(t->dist2)); 
	dl_print(t->dist2);
}



// Duct-tape dependencey system (DTDS)
#define done(x) t->flags |= x; return x;
#define want(x) \
	if (flag == x && t->flags & x) \
		return x; \
	else if (flag == x) 
#define ensure(x) \
	if (!t->flags & x) { \
		return tweak_advance(t, x); \
	}

unsigned int tweak_advance_to(tweak_t* t, unsigned int flag)
{

//	want(TWEAK_HAS_SIP) {
		// problems
//		assert(0);

		// FIXME in the future, we could probably try depending on
		// correspondences and then estimate a SIP TAN header from
		// that.
//	} 

	want(TWEAK_IMAGE_AD) {
		ensure(TWEAK_HAS_SIP);
		ensure(TWEAK_HAS_IMAGE_XY);

		// Convert to ra dec
		assert(!t->a);
		assert(!t->d);
		t->a = malloc(sizeof(double)*t->n);
		t->d = malloc(sizeof(double)*t->n);
		for (jj=0; jj<t->n; jj++) {
			pixelxy2radec(t->sip, t->x[jj], t->y[jj], t->a+jj, t->d+jj);
		}

		done(TWEAK_IMAGE_AD);
	}
			
	want(TWEAK_REF_XY) {
		// FIXME this could be provided by rawstartree provided
		//ensure(TWEAK_HAS_AD_BAR_AND_R);
		ensure(TWEAK_HAS_REF_AD);

		tweak_clear_ref_xy(t);

		assert(t->flags & TWEAK_HAS_REF_AD);
		assert(t->n_ref);
		assert(!t->x_ref);
		assert(!t->y_ref);
		t->x_ref = malloc(sizeof(double)*t->n_ref);
		t->y_ref = malloc(sizeof(double)*t->n_ref);
		for (jj=0; jj<t->n_ref; jj++) {
			radec2pixelxy(t->sip, t->a_ref[jj], t->d_ref[jj],
				      t->x_ref+jj, t->y_ref+jj);
		}

		done(TWEAK_REF_XY);
	}

	want(TWEAK_HAS_AD_BAR_AND_R) {
		ensure(TWEAK_HAS_REF_AD);

		assert(t->flags & TWEAK_HAS_REF_AD);
		get_center_and_radius(t->a_ref, t->d_ref, t->n_ref, 
		                      &t->a_bar, &t->d_bar, &t->radius);

		done(TWEAK_HAS_AD_BAR_AND_R);
	}

	want(TWEAK_HAS_IMAGE_XYZ) {
		ensure(TWEAK_HAS_IMAGE_AD);

		assert(!t->xyz);

		t->xyz = malloc(3*t->n*sizeof(double));
		int i;
		for (i=0; i<n; i++) {
			radec2xyzarr(deg2rad(t->a[i]),
				     deg2rad(t->d[i]),
				     t->xyz + 3*i);
		}
		done(TWEAK_HAS_IMAGE_XYZ);
	}

	want(TWEAK_HAS_REF_XYZ) {
		ensure(TWEAK_HAS_REF_AD);

		// need to fetch from rawstartree here
		assert(0);

		done(TWEAK_HAS_REF_XYZ);
	}

	want(TWEAK_HAS_COARSLY_SHIFTED) {
		ensure(TWEAK_HAS_REF_XY);
		ensure(TWEAK_HAS_IMAGE_XY);

		get_dydx_range(t->x, t->y, t->n,
		               t->x_ref, t->y_ref, t->n_ref,
		               t->mindx, t->mindy, t->maxdx, t->maxdy);

		get_shift(t->x, t->y, t->n,
		          t->x_ref, t->y_ref, t->n_ref,
		          t->mindx, t->mindy, t->maxdx, t->maxdy,
			  &t->xs, &t->ys);


		done(TWEAK_HAS_COARSLY_SHIFTED);
	}

	want(TWEAK_HAS_FINELY_SHIFTED) {
		ensure(TWEAK_HAS_COARSLY_SHIFTED);

		// Shrink size of hough box
		double rho = 0.05;

		get_shift(t->x, t->y, t->n,
		          t->x_ref, t->y_ref, t->n_ref,
		          rho*t->mindx, rho*t->mindy,
			  rho*t->maxdx, rho*t->maxdy,
			  &t->xs, &t->ys);

		done(TWEAK_HAS_COARSLY_SHIFTED);
	}

	want(TWEAK_HAS_CORRESPONDENCES) {
		ensure(TWEAK_HAS_REF_XYZ);
		ensure(TWEAK_HAS_IMAGE_XYZ);

		find_correspondences(t);

		done(TWEAK_HAS_CORRESPONDENCES);
	}

	fprintf(stderr, "die for dependence: flag %p\n", flag);
	assert(0);
}
