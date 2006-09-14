#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "kdtree.h"
#include "kdtree_internal.h"
#include "kdtree_macros.h"

/*
  Expects the following to be defined:

  -   REAL         (must be preprocessor symbol)
  -   real         the input data type (typedef or preprocessor)
  -   KDTYPE       (preprocessor)
  -   kdtype       the kdtree data type
  -   REAL_MIN              real's min value (as a "real")
  -   REAL_MAX              real's max value (as a "real")
  -   REAL2KDTYPE(kd, d, r)   converts a "real" (in dimension "d") to a "kdtype" for the bounding box.
  -   KDTYPE_INTEGER          is kdtype an integer type? (0/1)
  -   KDTYPE_MIN              kdtype's min value (as a "kdtype")
  -   KDTYPE_MAX              kdtype's max value (as a "kdtype")
  x -   KDT_INFTY    large "real"; (also -KDT_INFTY must work)
  x -   LOW_HR (kd, D, i)       returns a "kdtype*" to the lower hyperrectangle corner, for a D-dimensional tree.
  x -   HIGH_HR(kd, D, i)       returns a "kdtype*" to the upper hyperrectangle corner
*/

#define LOW_HR( kd, D, i) ((kd)->bb.KDTYPE_VAR + (2*(i)*(D)))
#define HIGH_HR(kd, D, i) ((kd)->bb.KDTYPE_VAR + ((2*(i)+1)*(D)))

#define KD_SPLIT(kd, i) ((kd)->split.KDTYPE_VAR + (i))

#define KD_DATA(kd, D, i) ((kd)->data.REAL_VAR + (2*(D)*(i)))

#define KD_PERM(kd, i) ((kd)->perm ? (kd)->perm[i] : i)

#if defined(KD_DIM)
#define DIMENSION   (KD_DIM)
#else
#define DIMENSION   (kd->ndim)
#endif

#define SIZEOF_PT  (sizeof(real)*DIMENSION)

// compatibility macros
#define COMPAT_NODE_SIZE      (sizeof(kdtree_node_t) + (SIZEOF_PT * 2))
//#define COMPAT_HIGH_HR(kd, i)  ((real*)(((char*)(kd)->nodes)
#define COMPAT_HIGH_HR(kd, i)  ((kdtype*)(((char*)(kd)->nodes) \
										  + COMPAT_NODE_SIZE*(i) \
										  + sizeof(kdtree_node_t) \
										  + SIZEOF_PT))
//#define COMPAT_LOW_HR(kd, i)   ((real*) (((char*)(kd)->nodes)
#define COMPAT_LOW_HR(kd, i)   ((kdtype*) (((char*)(kd)->nodes) \
										   + COMPAT_NODE_SIZE*(i) \
										   + sizeof(kdtree_node_t)))


// FIXME: kdtree_qres_t -> results: real or kdtype ?

static inline real dist2(real* p1, real* p2, int d) {
	int i;
	real d2 = 0.0;
#if defined(KD_DIM)
	d = KD_DIM;
#endif
	for (i=0; i<d; i++)
		d2 += (p1[i] - p2[i]) * (p1[i] - p2[i]);
	return d2;
}

static inline real dist2_bailout(real* p1, real* p2, int d, real maxd2) {
	int i;
	real d2 = 0.0;
#if defined(KD_DIM)
	d = KD_DIM;
#endif
	for (i=0; i<d; i++) {
		d2 += (p1[i] - p2[i]) * (p1[i] - p2[i]);
		if (d2 > maxd2)
			return -1.0;
	}
	return d2;
}

static inline real dist2_exceeds(real* p1, real* p2, int d, real maxd2) {
	int i;
	real d2 = 0.0;
#if defined(KD_DIM)
	d = KD_DIM;
#endif
	for (i=0; i<d; i++) {
		d2 += (p1[i] - p2[i]) * (p1[i] - p2[i]);
		if (d2 > maxd2)
			return 1;
	}
	return 0;
}

static bool kdtree_bb_point_mindist2_exceeds(real* bblow, real* bbhigh,
											 real* point, int dim, real maxd2)
{
	real d2 = 0.0;
	real delta;
	int i;
#if defined(KD_DIM)
	dim = KD_DIM;
#endif
	for (i = 0; i < dim; i++) {
		real lo, hi;
		lo = bblow[i];
		hi = bbhigh[i];
		if (point[i] < lo)
			delta = lo - point[i];
		else if (point[i] > hi)
			delta = point[i] - hi;
		else
			continue;
		d2 += delta * delta;
		if (d2 > maxd2)
			return TRUE;
	}
	return FALSE;
}

static bool kdtree_bb_point_maxdist2_exceeds(real* bblow, real* bbhigh,
											 real* point, int dim, real maxd2)
{
	real d2 = 0.0;
	real delta1, delta2;
	int i;
#if defined(KD_DIM)
	dim = KD_DIM;
#endif
	for (i = 0; i < dim; i++) {
		real lo, hi;
		lo = bblow[i];
		hi = bbhigh[i];
		delta1 = (point[i] - lo) * (point[i] - lo);
		delta2 = (point[i] - hi) * (point[i] - hi);
		if (delta1 > delta2)
			d2 += delta1;
		else
			d2 += delta2;
		if (d2 > maxd2)
			return TRUE;
	}
	return FALSE;
}

/*
  real kdtree_bb_mindist2_exceeds(real* bblow1, real* bbhigh1,
  real* bblow2, real* bbhigh2, int dim,
  real maxd2) {
  real d2 = 0.0;
  real delta;
  int i;
  #if defined(KD_DIM)
  dim = KD_DIM;
  #endif
  for (i = 0; i < dim; i++) {
  real alo, ahi, blo, bhi;
  alo = bblow1 [i];
  blo = bblow2 [i];
  ahi = bbhigh1[i];
  bhi = bbhigh2[i];
  if (ahi < blo) {
  delta = blo - ahi;
  d2 += delta * delta;
  if (d2 > maxd2)
  return 1;
  } else if (bhi < alo) {
  delta = alo - bhi;
  d2 += delta * delta;
  if (d2 > maxd2)
  return 1;
  } // else delta = 0;
  }
  return 0;
  }

  real kdtree_bb_maxdist2_exceeds(real* bblow1, real* bbhigh1,
  real* bblow2, real* bbhigh2, int dim,
  real maxd2)
  {
  real d2 = 0.0;
  real delta;
  int i;
  #if defined(KD_DIM)
  dim = KD_DIM;
  #endif
  for (i = 0; i < dim; i++) {
  real alo, ahi, blo, bhi;
  real delta2;
  alo = bblow1 [i];
  ahi = bbhigh1[i];
  blo = bblow2 [i];
  bhi = bbhigh2[i];
  delta  = bhi - alo;
  delta2 = ahi - blo;
  if (delta2 > delta)
  delta = delta2;
  d2 += delta * delta;
  if (d2 > maxd2)
  return 1;
  }
  return 0;
  }
*/

/* Sorts results by kq->sdists */
static int kdtree_qsort_results(kdtree_qres_t *kq, int D)
{
	int beg[KDTREE_MAX_RESULTS], end[KDTREE_MAX_RESULTS], i = 0, j, L, R;
	static real piv_vec[KDTREE_MAX_DIM];
	unsigned int piv_perm;
	real piv;

	beg[0] = 0;
	end[0] = kq->nres - 1;
	while (i >= 0) {
		L = beg[i];
		R = end[i];
		if (L < R) {
			piv = kq->sdists[L];
			for (j = 0;j < D;j++)
				piv_vec[j] = kq->results.REAL_VAR[D * L + j];
			piv_perm = kq->inds[L];
			if (i == KDTREE_MAX_RESULTS - 1) /* Sanity */
				assert(0);
			while (L < R) {
				while (kq->sdists[R] >= piv && L < R)
					R--;
				if (L < R) {
					for (j = 0;j < D;j++)
						kq->results.REAL_VAR[D*L + j] = kq->results.REAL_VAR[D * R + j];
					kq->inds[L] = kq->inds[R];
					kq->sdists[L] = kq->sdists[R];
					L++;
				}
				while (kq->sdists[L] <= piv && L < R)
					L++;
				if (L < R) {
					for (j = 0;j < D;j++)
						kq->results.REAL_VAR[D*R + j] = kq->results.REAL_VAR[D * L + j];
					kq->inds[R] = kq->inds[L];
					kq->sdists[R] = kq->sdists[L];
					R--;
				}
			}
			for (j = 0;j < D;j++)
				kq->results.REAL_VAR[D*L + j] = piv_vec[j];
			kq->inds[L] = piv_perm;
			kq->sdists[L] = piv;
			beg[i + 1] = L + 1;
			end[i + 1] = end[i];
			end[i++] = L;
		} else
			i--;
	}
	return 1;
}

static
bool resize_results(kdtree_qres_t* res, int newsize, int d,
					bool do_dists) {
	if (do_dists)
		res->sdists  = realloc(res->sdists , newsize * sizeof(real));
	res->results.any = realloc(res->results.any, newsize * d * sizeof(real));
	res->inds    = realloc(res->inds   , newsize * sizeof(unsigned int));
	if (newsize && (!res->results.any || (do_dists && !res->sdists) || !res->inds))
		fprintf(stderr, "Failed to resize kdtree results arrays.\n");
	return TRUE;
}

static
bool add_result(kdtree_qres_t* res, real sdist, unsigned int ind, real* pt,
				int d, int* res_size, bool do_dists) {
	if (do_dists)
		res->sdists[res->nres] = sdist;
	res->inds  [res->nres] = ind;
	memcpy(((char*)res->results.any) + res->nres * d * sizeof(real),
		   pt, sizeof(real) * d);
	res->nres++;
	if (res->nres == *res_size) {
		// enlarge arrays.
		*res_size *= 2;
		return resize_results(res, *res_size, d, do_dists);
	}
	return TRUE;
}

kdtree_qres_t* KDMANGLE(kdtree_rangesearch_options, REAL, KDTYPE)(kdtree_t* kd, REAL* pt, double maxd2, int options)
{
	int nodestack[100];
	int stackpos = 0;
	kdtree_qres_t *res;
	int res_size;
	int* p_res_size = &res_size;
	int D;
	bool do_dists;
	bool do_wholenode_check;

	if (!kd || !pt)
		return NULL;
#if defined(KD_DIM)
	assert(kd->ndim == KD_DIM);
	D = KD_DIM;
#else
	D = kd->ndim;
#endif

	// gotta compute 'em if ya wanna sort 'em!
	if (options & KD_OPTIONS_SORT_DISTS)
		options |= KD_OPTIONS_COMPUTE_DISTS;
	do_dists = options & KD_OPTIONS_COMPUTE_DISTS;
	do_wholenode_check = !(options & KD_OPTIONS_SMALL_RADIUS);

	res = calloc(1, sizeof(kdtree_qres_t));
	if (!res) {
		fprintf(stderr, "Failed to allocate kdtree_qres_t struct.\n");
		return NULL;
	}
	res_size = KDTREE_MAX_RESULTS;
	resize_results(res, res_size, D, do_dists);

	// queue root.
	nodestack[0] = 0;

	while (stackpos >= 0) {
		int nodeid;
		int i;
		int dim;
		int L, R;
		kdtype split;
		kdtype *bblo=NULL, *bbhi=NULL;

		nodeid = nodestack[stackpos];
		stackpos--;

		if (ISLEAF(kd, nodeid)) {
			L = kdtree_left(kd, nodeid);
			R = kdtree_right(kd, nodeid);
			if (do_dists) {
				for (i=L; i<=R; i++) {
					// FIXME benchmark dist2 vs dist2_bailout.
					real dsqd = dist2_bailout(pt, KD_DATA(kd, D, i), D, maxd2);
					if (dsqd == -1.0)
						continue;
					if (!add_result(res, dsqd, KD_PERM(kd, i), KD_DATA(kd, D, i), D, p_res_size, do_dists))
						return NULL;
				}
			} else {
				for (i=L; i<=R; i++) {
					if (dist2_exceeds(pt, KD_DATA(kd, D, i), D, maxd2))
						continue;
					if (!add_result(res, 0.0, KD_PERM(kd, i), KD_DATA(kd, D, i), D, p_res_size, do_dists))
						return NULL;
				}
			}
			continue;
		}

		if (kd->nodes) {
			// compat mode
			bblo = COMPAT_LOW_HR (kd, nodeid);
			bbhi = COMPAT_HIGH_HR(kd, nodeid);

		} else if (kd->bb.any) {
			// bb trees
			bblo = LOW_HR (kd, D, nodeid);
			bbhi = HIGH_HR(kd, D, nodeid);

		} else {
			// split/dim trees
			split = *KD_SPLIT(kd, i);
			if (kd->splitdim)
				dim = kd->splitdim[i];
			else {
#if KDTYPE_INTEGER
				dim = split & kd->dimmask;
				split &= kd->splitmask;
#endif
			}
		}

		if (bblo && bbhi) {
			if (kdtree_bb_point_mindist2_exceeds(bblo, bbhi, pt, D, maxd2))
				continue;

			if (do_wholenode_check &&
				!kdtree_bb_point_maxdist2_exceeds(bblo, bbhi, pt, D, maxd2)) {

				L = kdtree_left(kd, nodeid);
				R = kdtree_right(kd, nodeid);
				if (do_dists) {
					for (i=L; i<=R; i++) {
						real dsqd = dist2(pt, KD_DATA(kd, D, i), D);
						if (!add_result(res, dsqd, KD_PERM(kd, i), KD_DATA(kd, D, i), D, p_res_size, do_dists))
							return NULL;
					}
				} else {
					for (i=L; i<=R; i++)
						if (!add_result(res, 0.0, KD_PERM(kd, i), KD_DATA(kd, D, i), D, p_res_size, do_dists))
							return NULL;
				}
				continue;
			}
		}
		stackpos++;
		nodestack[stackpos] = 2 * nodeid + 1;
		stackpos++;
		nodestack[stackpos] = 2 * nodeid + 2;
	}

	/* Resize result arrays. */
	resize_results(res, res->nres, D, do_dists);

	/* Sort by ascending distance away from target point before returning */
	if (options & KD_OPTIONS_SORT_DISTS)
		kdtree_qsort_results(res, kd->ndim);

	return res;
}




#if KDTYPE_INTEGER

static real* kdqsort_arr;
static int kdqsort_D;

static int kdqsort_compare(const void* v1, const void* v2)
{
	int i1, i2;
	real val1, val2;
	i1 = *((int*)v1);
	i2 = *((int*)v2);
	val1 = kdqsort_arr[i1 * kdqsort_D];
	val2 = kdqsort_arr[i2 * kdqsort_D];
	if (val1 < val2)
		return -1;
	else if (val1 > val2)
		return 1;
	return 0;
}

static int kdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d)
{
	int* permute;
	int i, j, N;
	real* tmparr;
	int* tmpparr;

	N = r - l + 1;
	permute = malloc(N * sizeof(int));
	for (i = 0; i < N; i++)
		permute[i] = i;
	kdqsort_arr = arr + l * D + d;
	kdqsort_D = D;

	qsort(permute, N, sizeof(int), kdqsort_compare);

	tmparr = malloc(N * D * sizeof(real));
	tmpparr = malloc(N * sizeof(int));
	for (i = 0; i < N; i++) {
		int pi = permute[i];
		for (j = 0; j < D; j++) {
			tmparr[i*D + j] = arr[(l + pi) * D + j];
		}
		tmpparr[i] = parr[l + pi];
	}
	memcpy(arr + l*D, tmparr, N*D*sizeof(real));
	memcpy(parr + l, tmpparr, N*sizeof(int));
	free(tmparr);
	free(tmpparr);
	free(permute);
	return 1;
}

#else

#define GET(x) (arr[(x)*D+d])
#if defined(KD_DIM)
#define ELEM_SWAP(il, ir) { \
        if ((il) != (ir)) { \
          tmpperm  = parr[il]; \
          assert(tmpperm != -1); \
          parr[il] = parr[ir]; \
          parr[ir] = tmpperm; \
          { int d; for (d=0; d<D; d++) { \
		  tmpdata[0] = arr[(il)*D+d]; \
		  arr[(il)*D+d] = arr[(ir)*D+d]; \
          arr[(il)*D+d] = tmpdata[0]; }}}}
#else
#define ELEM_SWAP(il, ir) { \
        if ((il) != (ir)) { \
          tmpperm  = parr[il]; \
          assert(tmpperm != -1); \
          parr[il] = parr[ir]; \
          parr[ir] = tmpperm; \
          memcpy(tmpdata,    arr+(il)*D, D*sizeof(real)); \
          memcpy(arr+(il)*D, arr+(ir)*D, D*sizeof(real)); \
          memcpy(arr+(ir)*D, tmpdata,    D*sizeof(real)); }}
#endif

static int kdtree_quickselect_partition(real *arr, unsigned int *parr, int l, int r, int D, int d) {
	real* tmpdata = alloca(D * sizeof(real));
	real medval;
	int tmpperm = -1, i;
	int low, high;
	int median;
	int middle, ll, hh;

#if defined(KD_DIM)
	// tell the compiler this is a constant...
	D = KD_DIM;
#endif

	/* sanity is good */
	assert(r >= l);

	/* Find the median; also happens to partition the data */
	low = l;
	high = r;
	median = (low + high) / 2;
	while(1) {
        if (high <= (low+1))
            break;

		/* Find median of low, middle and high items; swap into position low */
		middle = (low + high) / 2;
		if (GET(middle) > GET(high))
			ELEM_SWAP(middle, high);
		if (GET(low) > GET(high))
			ELEM_SWAP(low, high);
		if (GET(middle) > GET(low))
			ELEM_SWAP(middle, low);

		/* Swap low item (now in position middle) into position (low+1) */
		ELEM_SWAP(middle, low + 1) ;

		/* Nibble from each end towards middle, swapping items when stuck */
		ll = low + 1;
		hh = high;
		for (;;) {
			do ll++;
			while (GET(low) > GET(ll)) ;
			do hh--;
			while (GET(hh) > GET(low)) ;

			if (hh < ll)
				break;

			ELEM_SWAP(ll, hh) ;
		}

		/* Swap middle item (in position low) back into correct position */
		ELEM_SWAP(low, hh) ;

		/* Re-set active partition */
		if (hh <= median)
			low = ll;
		if (hh >= median)
			high = hh - 1;
	}

    if (high == low + 1) {  /* Two elements only */
        if (GET(low) > GET(high))
            ELEM_SWAP(low, high);
    }
	medval = GET(median);

	/* check that it worked. */
	for (i = l; i <= median; i++) {
		assert(GET(i) <= medval);
	}
	for (i = median + 1; i <= r; i++) {
		/* Assert contention: i just changed this assert to strict
		 * because for the inttree, i need strict median guarentee
		 * (i.e. all the median values are on one side or the other of
		 * the return value of this function) If this causes problems
		 * let me know --k */
		assert(GET(i) > medval);
		//assert(GET(i) >= medval);
	}

	return median + 1;
}
#undef ELEM_SWAP
#undef GET

#endif

// FIXME: declaration should go elsewhere...
kdtree_t* KDMANGLE(kdtree_build, KDTYPE, KDTYPE)(kdtype* data, int N, int D, int maxlevel, bool bbtree, bool copydata);


/* args:
   
-   bb: bounding-box or split-dim?
*/
kdtree_t* KDMANGLE(kdtree_build, REAL, KDTYPE)
	 (real* data, int N, int D, int maxlevel, bool bbtree, bool copydata) {
	real* rdata;
	kdtype* kdata;
	int i, d;
	int xx;
	kdtree_t* kd;
	int nnodes;
	int lnext, level;

	assert(maxlevel > 0);
	assert(D <= KDTREE_MAX_DIM);
#if defined(KD_DIM)
	assert(D == KD_DIM);
	// let the compiler know that D is a constant...
	D = KD_DIM;
#endif

	/* Parameters checking */
	if (!data || !N || !D)
		return NULL;
	/* Make sure we have enough data */
	if ((1 << maxlevel) - 1 > N)
		return NULL;

	if (copydata) {
		double hi[D], lo[D], scale[D];
		kdtype* mydata;
		kdtree_t temptree;
		kdtree_t* kd = &temptree;

		for (d=0; d<D; d++) {
			hi[d] = REAL_MIN;
			lo[d] = REAL_MAX;
		}
		rdata = data;
		for (i=0; i<N; i++) {
			for (d=0; d<D; d++) {
				if (*rdata > hi[d]) hi[d] = *rdata;
				if (*rdata < lo[d]) lo[d] = *rdata;
				rdata++;
			}
		}
		for (d=0; d<D; d++)
			scale[d] = (double)KDTYPE_MAX / (hi[d] - lo[d]);

		kd->minval = lo;
		kd->maxval = hi;
		kd->scale = scale;

	    mydata = malloc(N * D * sizeof(kdtype));
		kdata = mydata;
		rdata = data;
		for (i=0; i<N; i++) {
			for (d=0; d<D; d++) {
				*kdata = REAL2KDTYPE(kd, d, *rdata);
				kdata++;
				rdata++;
			}
		}

		kd = KDMANGLE(kdtree_build, KDTYPE, KDTYPE)(mydata, N, D, maxlevel, bbtree, FALSE);
		if (kd)
			kd->datacopy = TRUE;
		return kd;
	}

	/* Set the tree fields */
	kd = calloc(1, sizeof(kdtree_t));
	nnodes = (1 << maxlevel) - 1;
	kd->ndata = N;
	kd->ndim = D;
	kd->nnodes = nnodes;
	kd->nbottom = 1 << (maxlevel - 1);
	kd->ninterior = kd->nbottom - 1;
	kd->data.any = data;

	assert(kd->nbottom + kd->ninterior == kd->nnodes);

	/* perm stores the permutation indexes. This gets shuffled around during
	 * sorts to keep track of the original index. */
	kd->perm = malloc(sizeof(unsigned int) * N);
	for (i = 0;i < N;i++)
		kd->perm[i] = i;
	assert(kd->perm);

	kd->lr = malloc(kd->nbottom * sizeof(unsigned int));
	assert(kd->lr);

	if (bbtree) {
		kd->bb.any = malloc(kd->nnodes * 2 * D * sizeof(kdtype));
		assert(kd->bb.any);
	} else {
		kd->split.any = malloc(kd->nnodes * sizeof(kdtype));
		assert(kd->split.any);

#if KDTYPE_INTEGER
#else
		kd->splitdim = malloc(kd->nnodes * sizeof(unsigned char));
#endif

		for (i=0; i<32; i++) {
			kd->dimmask |= (1 << i);
			if (kd->dimmask >= (D-1)) {
				kd->dimbits = i + 1;
				break;
			}
		}
		kd->splitmask = 0xffffffffu - kd->dimmask;
		//printf("D=%i.  dimbits=%i, mask=0x%x, splitmask=0x%x.\n", D, kd->dimbits, kd->dimmask, kd->splitmask);
	}

#if KDTYPE_INTEGER
	kd->minval = malloc(D * sizeof(double));
	kd->maxval = malloc(D * sizeof(double));
	kd->scale  = malloc(D * sizeof(double));
	for (d=0; d<D; d++) {
		kd->minval[d] = REAL_MAX;
		kd->maxval[d] = REAL_MIN;
	}
	rdata = kd->data.any;
	for (i=0; i<N; i++) {
		for (d=0; d<D; d++) {
			if (*rdata > kd->maxval[d]) kd->maxval[d] = *rdata;
			if (*rdata < kd->minval[d]) kd->minval[d] = *rdata;
			rdata++;
		}
	}
	for (d=0; d<D; d++) {
		kd->scale[d] = (double)KDTYPE_MAX / (kd->maxval[d] - kd->minval[d]);
	}
#endif

	/* Use the lr array as a stack while building. In place in your face! */
	kd->lr[0] = N - 1;

	lnext = 1;
	level = 0;

	/* And in one shot, make the kdtree. Because in inttree the lr pointers
	 * are only stored for the bottom layer, we use the lr array as a
	 * stack. At finish, it contains the r pointers for the bottom nodes.
	 * The l pointer is simply +1 of the previous right pointer, or 0 if we
	 * are at the first element of the lr array. */
	for (i = 0; i < kd->ninterior; i++) {
		unsigned int j;
		unsigned int d;
		unsigned int left, right;
		real maxrange;
#if KDTYPE_INTEGER
		real qsplit;
		unsigned int xx;
#endif
		kdtype s;
		unsigned int c;
		int dim;
		int m;
		real hi[D], lo[D];

		/* Have we reached the next level in the tree? */
		if (i == lnext) {
			level++;
			lnext = lnext * 2 + 1;
		}

		/* Since we're not storing the L pointers, we have to infer L */
		if (i == (1<<level)-1) {
			left = 0;
		} else {
			left = kd->lr[i-1] + 1;
		}
		right = kd->lr[i];

		/* More sanity */
		assert(0 <= left);
		assert(left <= right);
		assert(right < N);

		/* Find the bounding-box for this node. */
		/* (since data is stored lexicographically we can just iterate through it) */
		/* (avoid doing kd->data[NODE(i)*D + d] many times; just ++ the pointer) */
		for (d=0; d<D; d++) {
			hi[d] = REAL_MIN;
			lo[d] = REAL_MAX;
		}
		rdata = kd->data.any + left * D;
		for (j=left; j<=right; j++) {
			for (d=0; d<D; d++) {
				if (*rdata > hi[d]) hi[d] = *rdata;
				if (*rdata < lo[d]) lo[d] = *rdata;
				rdata++;
			}
		}
		if (bbtree) {
			for (d=0; d<D; d++) {
				(LOW_HR (kd, D, i))[d] = REAL2KDTYPE(kd, d, lo[d]);
				(HIGH_HR(kd, D, i))[d] = REAL2KDTYPE(kd, d, hi[d]);
			}
		}

		/* Split along dimension with largest range */
		maxrange = REAL_MIN;
		for (d=0; d<D; d++)
			if ((hi[d] - lo[d]) >= maxrange) {
				maxrange = hi[d] - lo[d];
				dim = d;
			}
		d = dim;
		assert (d < D);

#if KDTYPE_INTEGER
		/* Pivot the data at the median */
		/* Because the nature of the inttree is to bin the split
		 * planes, we have to be careful. Here, we MUST sort instead
		 * of merely partitioning, because we may not be able to
		 * properly represent the median as a split plane. Imagine the
		 * following on the real line: 
		 *
		 *    |P P   | P M  | P    |P     |  PP |  ------> X
		 *           1      2
		 * The |'s are possible split positions. If M is selected to
		 * split on, we actually cannot select the split 1 or 2
		 * immediately, because if we selected 2, then M would be on
		 * the wrong side (the medians always go to the right) and we
		 * can't select 1 because then P would be on the wrong side.
		 * So, the solution is to try split 2, and if point M-1 is on
		 * the correct side, great. Otherwise, we have to move shift
		 * point M-1 into the right side and only then chose plane 1. */


		/* FIXME but qsort allocates a 2nd perm array GAH */
		kdtree_qsort(data, kd->perm, left, right, D, dim);
		m = 1 + (left+right)/2;

		/* Make sure sort works */
		for(xx=left; xx<=right-1; xx++) { 
			assert(data[D*xx+d] <= data[D*(xx+1)+d]);
		}

		/* Encode split dimension and value. Last 2 bits are the split
		 * dimension. The split location is the upper 30 bits. */
		s = REAL2KDTYPE(kd, d, data[D*m+d]);
		if (!bbtree) {
			s = s & kd->splitmask;
			assert((s & kd->dimmask) == 0);
		}
		qsplit = KDTYPE2REAL(kd, d, s);

		{
			int xxxxy;
			int xxxx;
			xxxx = m;
			/* Play games to make sure we properly partition the data */
			while(m < right && data[D*m+d] < qsplit) m++;
			xxxxy = m;
			while(left <  m && qsplit <= data[D*(m-1)+d]) m--;
		}

		/* Even more sanity */
		assert(m != 0);
		for (xx=left; m && xx<=m-1; xx++)
			assert(data[D*xx+d] < qsplit);
		for (xx=m; xx<=right; xx++)
			assert(qsplit <= data[D*xx+d]);
		assert(left <= m);
		assert(m <= right);

#else
		/* Pivot the data at the median */
		m = kdtree_quickselect_partition(data, kd->perm, left, right, D, dim);
		s = REAL2KDTYPE(kd, d, data[D*m+d]);
#endif

#if KDTYPE_INTEGER
		if (!bbtree) {
			*(KD_SPLIT(kd, i)) = s | dim;
		}
#else
		if (!bbtree) {
			*(KD_SPLIT(kd, i)) = s;
			kd->splitdim[i] = dim;
		}
#endif

		/* Store the R pointers for each child */
		c = 2*i;
		if (level == maxlevel - 2)
			c -= kd->ninterior;
		kd->lr[c+1] = m-1;
		kd->lr[c+2] = right;

		assert((m == 0 && left == 0) || (left <= m-1));
		assert(m <= right);
	}

	for (xx=0; xx<kd->nbottom-1; xx++)
		assert(kd->lr[xx] <= kd->lr[xx+1]);

	/* do leaf nodes get bounding boxes? */





	return kd;

}





