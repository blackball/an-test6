#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "inttree.h"

/*****************************************************************************/
/* Utility macros                                                            */
/*****************************************************************************/
#define INTKDTREE_MAX_DIM 4
#define INTKDTREE_MAX_RESULTS 10000

/* Most macros operate on a variable kdtree_t *kd, assumed to exist. */
/* x is a node index, d is a dimension, and n is a point index */
#define INTNODE_SIZE      (sizeof(intkdtree_node_t) + sizeof(real)*kd->ndim*2)

#define NODE(x)        ((intkdtree_node_t*) (((void*)kd->tree) + INTNODE_SIZE*(x)))
#define PARENT(x)      NODE(((x)-1)/2)
#define CHILD_NEG(x)   NODE(2*(x) + 1)
#define CHILD_POS(x)   NODE(2*(x) + 2)
#define COORD(n,d)     ((real*)(kd->data + kd->ndim*(n) + (d)))

/*****************************************************************************/
/* Building routines                                                         */
/*****************************************************************************/

#define REAL2FIXED(x) ((unsigned int) round( (double) ((((x)-kd->minval)/(kd->maxval - kd->minval))/kd->delta) ) )
#define UINT_MAX 0xffffffff

real round(real);

/* nlevels is the number of levels in the tree. a 3-node tree has 2 levels. */
intkdtree_t *intkdtree_build(real *data, int ndata, int ndim, int nlevel,
                             real minval, real maxval)
{
	printf("BUILD %d\n", ndata);
	int i, j;
	intkdtree_t *kd;
	unsigned int nnodes, nbottom, ninterior;
	unsigned int level = 0, dim=-1, m;
	unsigned int lnext;
	real delta;
	unsigned int N = ndata;
	unsigned int D = ndim;

	assert(nlevel > 0);
	assert(D <= INTKDTREE_MAX_DIM);
	assert(minval < maxval);

	/* Parameters checking */
	if (!data || !N || !D)
		return NULL;
	/* Make sure we have enough data */
	if ((1 << nlevel) - 1 > N)
		return NULL;

	/* Range better be correct FIXME or determined automatically? */
	for (i=0; i<N; i++) {
		for (j=0; j<D; j++) {
			assert(minval <= data[D*i+j]);
			assert(data[D*i+j] <= maxval);
		}
	}

	/* Set the tree fields */
	kd = malloc(sizeof(intkdtree_t));
	nnodes = (1 << nlevel) - 1;
	nbottom = 1 << (nlevel - 1); /* number of nodes at the bottom level */
	ninterior = nbottom-1;       /* number of interior nodes */
	kd->ndata = N;
	kd->ndim = D;
	kd->nnodes = nnodes;
	kd->nbottom = nbottom;
	kd->ninterior = ninterior;
	kd->data = data;
	kd->minval = minval;
	kd->maxval = maxval;

	/* delta size for this tree... for now same for all dimensions */
	kd->delta = delta = (maxval - minval) / 4294967296.0;

	/* perm stores the permutation indexes. This gets shuffled around during
	 * sorts to keep track of the original index. */
	kd->perm = malloc(sizeof(unsigned int) * N);
	for (i = 0;i < N;i++)
		kd->perm[i] = i;
	assert(kd->perm);

	/* Only interior nodes need space in the node tree; others are in lr space */
	kd->tree = malloc(sizeof(intkdtree_node_t) * (ninterior));
	assert(kd->tree);
	kd->lr = calloc(nbottom, sizeof(unsigned int));

	for (i=0; i<nbottom; i++) {
		kd->lr[i] = 1111;
	}
	assert(kd->lr);

	/* Use the lr array as a stack while building. In place in your face! */
	kd->lr[0] = N - 1;

	lnext = 1;
	level = 0;

	/* And in one shot, make the kdtree. Because in inttree the lr pointers
	 * are only stored for the bottom layer, we use the lr array as a
	 * stack. At finish, it contains the r pointers for the bottom nodes.
	 * The l pointer is simply +1 of the previous right pointer, or 0 if we
	 * are at the first element of the lr array. */
	for (i = 0; i < ninterior; i++) {
		real hi[D];
		real lo[D];
		unsigned int j;
		unsigned int d;
		unsigned int left, right;
		real* pdata;
		real maxrange;

		/* Sanity */
		assert(NODE(i) == PARENT(2*i + 1));
		assert(NODE(i) == PARENT(2*i + 2));
		if (i && i % 2)
			assert(NODE(i) == CHILD_NEG((i - 1) / 2));
		else if (i)
			assert(NODE(i) == CHILD_POS((i - 1) / 2));

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
			lo[d] = KDT_INFTY;
		}

		/* Haha, how sick is this? Take advantage of the way data is stored. */
		pdata = kd->data + left * D;
		for (j=left; j<=right; j++) {
			for (d=0; d<D; d++) {
				if (*pdata > hi[d]) hi[d] = *pdata;
				if (*pdata < lo[d]) lo[d] = *pdata;
				pdata++;
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
		m = kdtree_qsort(data, kd->perm, left, right, D, dim);
		m = 1 + (left+right)/2;

		/* Make sure sort works */
		unsigned int xx;
		for(xx=left; xx<=right-1; xx++) { 
			assert(data[D*xx+d] <= data[D*(xx+1)+d]);
		}

		/* Encode split dimension and value. Last 2 bits are the split
		 * dimension. The split location is the upper 30 bits. */
		unsigned int s = REAL2FIXED(data[D*m+d]);
		s = s & 0xfffffffc;
		real qsplit = s*delta*kd->maxval; 
		assert((s & 0x3) == 0);

		/* Play games to make sure we properly partition the data */
		while(m < right && data[D*m+d] < qsplit) m++;
		while(m > left && qsplit <= data[D*(m-1)+d]) m--;
		assert(m-1 >= 0);

		/* Even more sanity */
		for (xx=left; xx<=m-1; xx++)
			assert(data[D*xx+d] < qsplit);
		for (xx=m; xx<=right; xx++)
			assert(qsplit <= data[D*xx+d]);
		assert(left <= m);
		assert(m <= right);

		/* Store split dimension and location */
		kd->tree[i].split = s | dim;

		/* Sanity is good when you do so much low level tweaking */
		unsigned int spl = kd->tree[i].split & 0x3;
		assert(spl == dim);

		/* Store the R pointers for each child */
		unsigned int c = 2*i;
		if (level == nlevel - 2)
			c -= ninterior;
		kd->lr[c+1] = m-1;
		kd->lr[c+2] = right;

		assert(0 <= left);
		assert(left <= m-1);
		assert(m <= right);
	}

	unsigned int xx;
	for(xx=0; xx<nbottom-1; xx++)
		assert(kd->lr[xx] <= kd->lr[xx+1]);

	return kd;
}

#define INTKDTREE_STACK_SIZE 1024
unsigned int stack[INTKDTREE_STACK_SIZE];
real results[INTKDTREE_MAX_RESULTS*INTKDTREE_MAX_DIM];
real results_sqd[INTKDTREE_MAX_RESULTS];
unsigned int results_inds[INTKDTREE_MAX_RESULTS];

void intkdtree_rangesearch_actual(intkdtree_t *kd, real *pt, real maxdistsqd, intkdtree_qres_t *res)
{
	real maxdist = sqrt(maxdistsqd); /* a sqrt per search or a square per iteration? */
	unsigned int radius = REAL2FIXED(maxdist);

	unsigned int zzz = 0; /* lazy programming */
	unsigned int pti[kd->ndim];
	unsigned int j;

	for (j=0; j<kd->ndim; j++)
		pti[j] = REAL2FIXED(pt[j]);

	stack[zzz++] = 0;

	while (zzz) {

		unsigned int i = stack[--zzz]; /* root beer */

		assert(i < kd->nnodes);

		if (i < kd->ninterior) {
			/* Interior nodes */

			unsigned int info = kd->tree[i].split;
			unsigned int spl = info & 0x3;
			unsigned int loc = info & 0xfffffffc;

			assert(spl < kd->ndim);

			if (pti[spl] < loc) {
				assert(pt[spl] < kd->delta*loc*kd->maxval);
				assert(loc >= pti[spl]);
				stack[zzz++] = 2*i+1;
				/* Note that this condition is written
				 * such that integer overflow cannot occur */
				if (loc - pti[spl] <= radius) {
					assert(kd->delta*loc*kd->maxval - pt[spl] > 0.0);
					assert(kd->delta*loc*kd->maxval - pt[spl] <= maxdist);
					stack[zzz++] = 2*i+2;
				}
			} else {
				assert(kd->delta*loc*kd->maxval <= pt[spl]);
				stack[zzz++] = 2*i+2;
				assert(pti[spl] >= loc);
				if (pti[spl] - loc < radius) {
					assert(pt[spl] - kd->delta*loc*kd->maxval >= 0.0);
					assert(pt[spl] - kd->delta*loc*kd->maxval < maxdist);
					stack[zzz++] = 2*i+1;
				}
			}
		} else {
			/* Child nodes */

			unsigned int lrind = i - kd->ninterior;
			unsigned int r = kd->lr[lrind];
			unsigned int l, k;

			/* Figure out our bounds in the data array */
			if (lrind) 
				l = kd->lr[lrind-1]+1;
			else
				l = 0;

			assert(0 <= l);
			assert(l <= r);
			assert(r <= kd->ndata);

			for (j=l; j<=r; j++) {
				real dsqd = 0.0;
				for (k=0; k < kd->ndim; k++) {
					real delta = pt[k] - (*COORD(j, k));
					dsqd += delta * delta;
				}
				if (dsqd < maxdistsqd) {
					results_sqd[res->nres] = dsqd;
					results_inds[res->nres] = kd->perm[j];
					memcpy(results + res->nres*kd->ndim, COORD(j, 0), sizeof(real)*kd->ndim);
					res->nres++;
					/* FIXME maybe put an assert and simply
					 * crash at this point? */
					if (res->nres >= INTKDTREE_MAX_RESULTS) {
						fprintf(stderr, "\nintkdtree rangesearch overflow.\n");
						break;
					}
				}
			}
		}
	}
}

/* Range seach */
int overflow;
intkdtree_qres_t *intkdtree_rangesearch_unsorted(intkdtree_t *kd, real *pt, real maxdistsquared)
{
	kdtree_qres_t *res;
	if (!kd || !pt)
		return NULL;
	res = malloc(sizeof(kdtree_qres_t));
	res->nres = 0;
	overflow = 0;

	/* Do the real rangesearch */
	intkdtree_rangesearch_actual(kd, pt, maxdistsquared, res);

	/* Store actual coordinates of results */
	res->results = malloc(sizeof(real) * kd->ndim * res->nres);
	memcpy(res->results, results, sizeof(real)*kd->ndim*res->nres);

	/* Store squared distances of results */
	res->sdists = malloc(sizeof(real) * res->nres);
	memcpy(res->sdists, results_sqd, sizeof(real)*res->nres);

	/* Store indexes of results */
	res->inds = malloc(sizeof(unsigned int) * res->nres);
	memcpy(res->inds, results_inds, sizeof(unsigned int)*res->nres);

	int j, k;
	for(j=0;j<res->nres; j++) {
		real x = 0;
		for(k=0; k<kd->ndim;k++) {
			real delta = res->results[kd->ndim*j+k]-pt[k];
			x += delta*delta;
		}
		assert(x <= maxdistsquared);
		//printf("delta: %f\n", x);
	}

	return res;
}


intkdtree_qres_t *intkdtree_rangesearch(intkdtree_t *kd, real *pt, real maxdistsquared)
{
	/* Do unsorted search */
	intkdtree_qres_t * res = intkdtree_rangesearch_unsorted(kd, pt, maxdistsquared);

	/* Sort by ascending distance away from target point before returning */
	kdtree_qsort_results(res, kd->ndim);

	return res;
}

void intkdtree_free(intkdtree_t* kd)
{
	free(kd->tree);
	free(kd->lr);
	free(kd->perm);
	free(kd);
}
