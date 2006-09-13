#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "kdtree.h"
#include "kdtree_internal.h"
#include "kdtree_macros.h"

/*
  Expects the following to be defined:

  -   REAL         (must be preprocessor symbol)
  -   real         the input data type (typedef or preprocessor)
  -   KDTYPE       (preprocessor)
  -   kdtype       the kdtree data type
  -   KDT_INFTY    large "real"; (also -KDT_INFTY must work)
  -   REAL2KDTYPE(kd, d, r)   converts a "real" (in dimension "d") to a "kdtype" for the bounding box.
  -   KDTYPE_INTEGER          is kdtype an integer type? (0/1)
  -   KDTYPE_MAX              if kdtype is integral, its max value (as an int)
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

/* args:
   
-   bb: bounding-box or split-dim?
*/
kdtree_t* KDMANGLE(kdtree_build, REAL, KDTYPE)
	 (real* data, int N, int D, int maxlevel, bool bbtree, bool copydata) {
	real* pdata;
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
		kd->bb.any = malloc(kd->nnodes * sizeof(kdtype));
		assert(kd->bb.any);
	} else {
		kd->split.any = malloc(kd->nnodes * sizeof(kdtype));
		assert(kd->split.any);
	}

#if KDTYPE_INTEGER
	kd->minval = malloc(D * sizeof(double));
	kd->maxval = malloc(D * sizeof(double));
	kd->scale  = malloc(D * sizeof(double));
	for (d=0; d<D; d++) {
		kd->minval[d] =  KDT_INFTY;
		kd->maxval[d] = -KDT_INFTY;
	}
	pdata = kd->data.any;
	for (i=0; i<N; i++) {
		for (d=0; d<D; d++) {
			if (*pdata > kd->maxval[d]) kd->maxval[d] = *pdata;
			if (*pdata < kd->minval[d]) kd->minval[d] = *pdata;
			pdata++;
		}
	}
	for (d=0; d<D; d++) {
		kd->scale[d] = (double)KDTYPE_MAX / (kd->maxval[d] - kd->minval[d]);
	}
#endif

	/* ??? */
	if (copydata) {
		kdtype* pd;
		kd->data.any = malloc(N * D * sizeof(kdtype));
		pd = kd->data.any;
		pdata = data;
		for (i=0; i<N; i++) {
			for (d=0; d<D; d++) {
				*pd = REAL2KDTYPE(kd, d, *pdata);
				pd++;
				pdata++;
			}
		}
	}

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
		unsigned int xx;
		unsigned int s;
		real qsplit;
		unsigned int spl;
		unsigned int c;
		int dim;
		int m;
		real hi[D], lo[D];

		/* Sanity */
		/*
		  assert(NODE(i) == PARENT(2*i + 1));
		  assert(NODE(i) == PARENT(2*i + 2));
		  if (i && i % 2)
		  assert(NODE(i) == CHILD_NEG((i - 1) / 2));
		  else if (i)
		  assert(NODE(i) == CHILD_POS((i - 1) / 2));
		*/

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
		for (d=0; d<D; d++) {
			hi[d] = -KDT_INFTY;
			lo[d] =  KDT_INFTY;
		}
		/* (since data is stored lexicographically we can just iterate through it) */
		/* (avoid doing kd->data[NODE(i)*D + d] many times; just ++ the pointer) */

		/** ??? Do we need special floor/ceiling REAL2KDTYPE routines to ensure
			that the bounding box we store actually bounds the original real types? */

		pdata = kd->data.any + left * D;
		for (j=left; j<=right; j++) {
			for (d=0; d<D; d++) {
				if (*pdata > hi[d]) hi[d] = *pdata;
				if (*pdata < lo[d]) lo[d] = *pdata;
				pdata++;
			}
		}

		if (bbtree) {
			for (d=0; d<D; d++) {
				(LOW_HR (kd, D, i))[d] = REAL2KDTYPE(kd, d, lo[d]);
				(HIGH_HR(kd, D, i))[d] = REAL2KDTYPE(kd, d, hi[d]);
			}
		}

		/* Split along dimension with largest range */
		maxrange = -1.0;
		for (d=0; d<D; d++)
			if ((hi[d] - lo[d]) > maxrange) {
				maxrange = hi[d] - lo[d];
				dim = d;
			}
		d = dim;
		assert (d < D);

		/* PIVOT */

		/* Pivot the data at the median */
		//m = KDFUNC(kdtree_quickselect_partition)(data, kd->perm, NODE(i)->l, NODE(i)->r, D, dim);


		if (!bbtree) {
			/* Encode split dimension and value. Last 2 bits are the split
			 * dimension. The split location is the upper 30 bits. */
		}


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





