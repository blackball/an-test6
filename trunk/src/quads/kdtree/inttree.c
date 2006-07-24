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
#define INTKDTREE_MAX_RESULTS 1000

/* Most macros operate on a variable kdtree_t *kd, assumed to exist. */
/* x is a node index, d is a dimension, and n is a point index */
#define INTNODE_SIZE      (sizeof(intkdtree_node_t) + sizeof(real)*kd->ndim*2)

#define NODE(x)        ((intkdtree_node_t*) (((void*)kd->tree) + INTNODE_SIZE*(x)))
#define PARENT(x)      NODE(((x)-1)/2)
#define CHILD_NEG(x)   NODE(2*(x) + 1)
#define CHILD_POS(x)   NODE(2*(x) + 2)
#define ISLEAF(x)      ((2*(x)+1) >= kd->nnodes)
#define COORD(n,d)     ((real*)(kd->data + kd->ndim*(n) + (d)))

/*****************************************************************************/
/* Building routines                                                         */
/*****************************************************************************/

#define REAL2FIXED(x) ((unsigned int) round( (double) ((((x)-kd->minval)/(kd->maxval - kd->minval))/kd->delta) ) )

/* nlevels is the number of levels in the tree. a 3-node tree has 2 levels. */
intkdtree_t *intkdtree_build(real *data, int ndata, int ndim, int nlevel,
                             real minval, real maxval)
{
	printf("BUILD %d\n", ndata);
	int i;
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
	 * stack. At finish, it contains the r pointers for the bottom nodes. */
	for (i = 0; i < ninterior; i++) {
		real hi[D];
		real lo[D];
		int j;
		unsigned int d;
		unsigned int left, right;
		unsigned int firstinlevel = 0;
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
			//printf ("new level %d\n", level);
			firstinlevel = 1; /* not used yet */
			(void) firstinlevel;
		}

		/* Since we're not storing the L pointers, we have to infer L */
		if (i == (1<<level)-1) {
			left = 0;
		} else {
			left = kd->lr[i-1] + 1;
		}
		right = kd->lr[i];
		assert(left <= right);

		//printf("l %d r %d\n", left, right);

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

		/* Pivot the data at the median */
		m = kdtree_quickselect_partition(data, kd->perm, left, right, D, dim);

		for (j=0;j<N*D;j++) {
			//printf("%4.0f ",  data[j]);
		}//printf("\n");

		/* Encode split dimension and value. Last 2 bits are dim */
		unsigned int s = REAL2FIXED(data[D*m+d]);
		unsigned int rem = s  & 0x3;
		/* if we're close to the next value, push up */
		switch(rem) {
			/* case 0: no rounding needed */
			case 1: s--; break; /* get rid of trailing 1 */
			case 2: s++;
			case 3: s++;
		}
		assert((s & 0x3) == 0);
		printf("s*d %f split %f error: %f\n", s*delta*kd->maxval, data[D*m+d], s*delta*kd->maxval - data[D*m+d]);
		kd->tree[i].split = s | dim;

		unsigned int spl = kd->tree[i].split & 0x3;
		assert(spl ==dim);

		for(j=0; j<nbottom; j++) {
			//printf("%3i ", kd->lr[j]);
		}
		//printf("\n");

		if (level == nlevel - 2) {
			int c = 2*i - ninterior;
			/* FIXME asserts!!!!! */
			kd->lr[c+1] = m-1;
			kd->lr[c+2] = right;
		} else {

			/* not yet fancy last level */
			/* FIXME need some asserts here... corner cases? */
			kd->lr[2*i+1] = m-1;
			kd->lr[2*i+2] = right;
		}
	}
	int j;
	for(j=0; j<nbottom; j++) {
		//printf("%3i ", kd->lr[j]);
	}
	//printf("\n");
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
	unsigned int umd = REAL2FIXED(maxdist);

	unsigned int zzz = 0; /* lazy programming */
	unsigned int pti[kd->ndim];
	unsigned int j;

	for (j=0; j<kd->ndim; j++)
		pti[j] = REAL2FIXED(pt[j]);

	stack[zzz++] = 0;

	while (zzz) {

		unsigned int i = stack[--zzz]; /* root beer */

		printf("popped %d\n", i);

		assert(i < kd->nnodes);

		if (i < kd->ninterior) {
			unsigned int info = kd->tree[i].split;
			unsigned int spl = info & 0x3;
			unsigned int loc = info & 0xfffffffc;

			assert(spl < kd->ndim);
			printf("spl %d loc %f\n", spl, kd->delta*loc*kd->maxval);

			if (pti[spl] <= loc) {
				stack[zzz++] = 2*i+1;
				if (pti[spl] + umd >= loc)
					stack[zzz++] = 2*i+2;
			} else {
				stack[zzz++] = 2*i+2;
				if (loc + umd <= pti[spl] )
					stack[zzz++] = 2*i+1;
			}
		} else {
			/* children */

			unsigned int lrind = i - kd->ninterior;
			unsigned int l, r = kd->lr[lrind];
			unsigned int k;
			if (lrind) 
				l = kd->lr[lrind-1]+1;
			else
				l = 0;

			assert(l <= r);

			for (j=l; j<=r; j++) {
				real dsqd = 0.0;
				for (k=0; k < kd->ndim; k++) {
					real delta = pt[k] - (*COORD(j, k));
					dsqd += delta * delta;
				}
				if (dsqd < maxdistsqd) {
					printf("found point close enough dist %f\n",sqrt(dsqd));
					results_sqd[res->nres] = dsqd;
					results_inds[res->nres] = kd->perm[j];
					memcpy(results + res->nres*kd->ndim, COORD(j, 0), sizeof(real)*kd->ndim);
					res->nres++;
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
intkdtree_qres_t *intkdtree_rangesearch(intkdtree_t *kd, real *pt, real maxdistsquared)
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
