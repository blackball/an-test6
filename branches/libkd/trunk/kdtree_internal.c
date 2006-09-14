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

/*
  #define FOURBILLION 4294967296.0
  #define REAL2FIXED(x) ((unsigned int) round( (double) ((((x)-kd->minval)/(kd->maxval - kd->minval))*FOURBILLION ) ) )
  #define REALDELTA2FIXED(x) ((unsigned int) round( (double) (((x)/(kd->maxval - kd->minval))*FOURBILLION ) ) )
  #define FIXED2REAL(i) (kd->minval + (i)*kd->delta)
  #define UINT_MAX 0xffffffff
*/

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
		/*
		  real hi[D], lo[D];
		  real scale[D];
		*/
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

		return KDMANGLE(kdtree_build, KDTYPE, KDTYPE)(mydata, N, D, maxlevel, bbtree, FALSE);
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

	/* ???
	   if (copydata) {
	   kd->data.any = malloc(N * D * sizeof(kdtype));
	   kdata = kd->data.any;
	   rdata = data;
	   for (i=0; i<N; i++) {
	   for (d=0; d<D; d++) {
	   *kdata = REAL2KDTYPE(kd, d, *rdata);
	   kdata++;
	   rdata++;
	   }
	   }
	   }
	*/

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
		kdtype s;
#endif
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

		/** ??? Do we need special floor/ceiling REAL2KDTYPE routines to ensure
			that the bounding box we store actually bounds the original real types? */

		// Gah!
		/*
		  if (copydata) {
		  kdtype klo[D];
		  kdtype khi[D];
		  kdata = kd->data.any + left * D;
		  for (d=0; d<D; d++) {
		  khi[d] = KDTYPE_MIN;
		  klo[d] = KDTYPE_MAX;
		  }
		  for (j=left; j<=right; j++) {
		  for (d=0; d<D; d++) {
		  if (*kdata > khi[d]) khi[d] = *kdata;
		  if (*kdata < klo[d]) klo[d] = *kdata;
		  kdata++;
		  }
		  }
		  if (bbtree) {
		  for (d=0; d<D; d++) {
		  (LOW_HR (kd, D, i))[d] = klo[d];
		  (HIGH_HR(kd, D, i))[d] = khi[d];
		  }
		  }
		  maxrange = -1.0;
		  for (d=0; d<D; d++)
		  if ((khi[d] - klo[d]) > maxrange) {
		  maxrange = khi[d] - klo[d];
		  dim = d;
		  }
		  d = dim;
		  assert (d < D);
		  } else {
		*/
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
		//m = kdtree_qsort(data, kd->perm, left, right, D, dim);
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

		/*
		  #if KDTYPE_INTEGER
		  printf("%s: L=%i, R=%i, m=%i, d[m]=%u, s=%u, qsplit=%u.\n",
		  __FUNCTION__,
		  left, right, m, (unsigned int)data[D*m+d], s, (unsigned int)qsplit);
		  #else
		  printf("%s: L=%i, R=%i, m=%i, d[m]=%g, s=%u, qsplit=%g.\n",
		  __FUNCTION__,
		  left, right, m, (double)data[D*m+d], s, (double)qsplit);
		  #endif
		*/

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
		m = KDFUNC(kdtree_quickselect_partition)(data, kd->perm, left, right, D, dim);
#endif

#if KDTYPE_INTEGER
		if (!bbtree) {
			*(KD_SPLIT(kd, i)) = s | dim;
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





