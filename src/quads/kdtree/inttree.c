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

#ifdef AMNSLOW

real* kdqsort_arr;
int kdqsort_D;

real bounds[INTKDTREE_MAX_DIM];

int kdqsort_compare(const void* v1, const void* v2)
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

int kdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d)
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

#endif

#define GET(x) (arr[(x)*D+d])
#define ELEM_SWAP(il, ir) { \
        if ((il) != (ir)) { \
          tmpperm  = parr[il]; \
          assert(tmpperm != -1); \
          parr[il] = parr[ir]; \
          parr[ir] = tmpperm; \
          memcpy(tmpdata,    arr+(il)*D, D*sizeof(real)); \
          memcpy(arr+(il)*D, arr+(ir)*D, D*sizeof(real)); \
          memcpy(arr+(ir)*D, tmpdata,    D*sizeof(real)); } \
        }

int kdtree_quickselect_partition(real *arr, unsigned int *parr, int l, int r,
                                 int D, int d)
{
	real* tmpdata = alloca(D * sizeof(real));
	real medval;
	int tmpperm = -1, i;

	int low, high;
	int median;
	int middle, ll, hh;

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
		assert(GET(i) >= medval);
		//assert(GET(i) > medval);
	}

	return median + 1;
}
#undef ELEM_SWAP
#undef GET

/* If the root node is level 0, then maxlevel is the level at which there may
 * not be enough points to keep the tree complete (i.e. last level) */
intkdtree_t *intkdtree_build(real *data, int ndata, int ndim, int maxlevel,
                             real minval, real maxval)
{
	int i;
	intkdtree_t *kd;
	int nnodes, nbottom;
	int level = 0, dim=-1, m;
	int lnext;
	unsigned int N = ndata;
	unsigned int D = ndim;

	assert(maxlevel > 0);
	assert(D <= INTKDTREE_MAX_DIM);
	assert(minval < maxval);

	/* Parameters checking */
	if (!data || !N || !D)
		return NULL;
	/* Make sure we have enough data */
	if ((1 << maxlevel) - 1 > N)
		return NULL;

	/* Set the tree fields */
	kd = malloc(sizeof(intkdtree_t));
	nnodes = (1 << maxlevel) - 1;
	nbottom = 1 << (maxlevel - 1); /* number of nodes at the bottom level */
	kd->ndata = N;
	kd->ndim = D;
	kd->nnodes = nnodes;
	kd->nbottom = nbottom;
	kd->data = data;
	kd->minval = minval;
	kd->maxval = maxval;

	/* delta size for this tree... for now same for all dimensions */
	kd->delta = (maxval - minval) / (real) (unsigned int) -1;

	/* perm stores the permutation indexes. This gets shuffled around during
	 * sorts to keep track of the original index. */
	kd->perm = malloc(sizeof(unsigned int) * N);
	for (i = 0;i < N;i++)
		kd->perm[i] = i;
	assert(kd->perm);
	kd->tree = malloc(sizeof(intkdtree_node_t) * (nnodes));
	assert(kd->tree);
	kd->lr = calloc(2*sizeof(unsigned int) * (nbottom), 1);
	assert(kd->lr);

	/* Use the lr array as a stack while building. Yummy memory reuse. */
	kd->lr[0] = 0;
	kd->lr[1] = N - 1;

	lnext = 1;
	level = 0;

	/* And in one shot, make the kdtree. Because in inttree the lr pointers
	 * are only stored for the bottom layer, we use the lr array as a
	 * stack. At finish, it contains the lr pointers for the bottom nodes. */
	for (i = 0; i < nnodes; i++) {
		real hi[D];
		real lo[D];
		int j, d;
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
			printf ("new level %d\n", level);
		}

		if (level == maxlevel - 1) {
			/* Get left and right from front of array */
			assert( i-((1<<(maxlevel-1))-1) < nnodes);
			left = kd->lr[2*(i-((1<<(maxlevel-1))-1))];
			right = kd->lr[2*(i-((1<<(maxlevel-1))-1)) + 1];
			assert(left <= right);
		} else {
			/* Access linerally from front */
			left = kd->lr[2*i];
			right = kd->lr[2*i+1];
			assert(left <= right);
		}

		/* Find the bounding-box for this node. */
		for (d=0; d<D; d++) {
			hi[d] = -KDT_INFTY;
			lo[d] = KDT_INFTY;
		}
		// Haha, how sick is this? Take advantage of the way data is stored.
		pdata = kd->data + left * D;
		for (j=left; j<=right; j++) {
			for (d=0; d<D; d++) {
				if (*pdata > hi[d]) hi[d] = *pdata;
				if (*pdata < lo[d]) lo[d] = *pdata;
				pdata++;
			}
		}

		/* Decide which dimension to split along: we use the dimension with
		   largest range. */
		maxrange = -1.0;
		for (d=0; d<D; d++)
			if ((hi[d] - lo[d]) > maxrange) {
				maxrange = hi[d] - lo[d];
				dim = d;
			}

		/* Pivot the data at the median */
		m = kdtree_quickselect_partition(data, kd->perm, left, right, D, dim);

		/* Encode split dimension and split value */
		kd->nodes[i] = 

		for(j=0; j<nbottom; j++) {
			printf("%3i ", kd->lr[j]);
		}
		printf("\n");

		/* Only do child operations if we're not the last layer */
		if (level == maxlevel - 2) {

			assert(2*i + 1 < nnodes);
			assert(2*i + 2 < nnodes);

			int ntop = nnodes - nbottom;
			int children = 2*(i - ntop);

			kd->lr[2*children+0] = left;
			kd->lr[2*children+1] = m - 1;
			kd->lr[2*children+3] = m;
			kd->lr[2*children+4] = right;

		} else if (level < maxlevel - 1) {
			assert(2*i + 1 < nnodes);
			assert(2*i + 2 < nnodes);

			kd->lr[2*(2*i+1) + 0] = left;
			kd->lr[2*(2*i+1) + 1] = m - 1;
			kd->lr[2*(2*i+2) + 0] = m;
			kd->lr[2*(2*i+2) + 1] = right;
		}
	}
	return kd;
}

void intkdtree_rangesearch_actual(intkdtree_t *kd, real *pt, real maxdistsqd, kdtree_qres_t *res)
{

}

/* Range seach helper */
void intkdtree_rangesearch_actual(intkdtree_t *kd, int nodeid, real *pt, real maxdistsqd, kdtree_qres_t *res)
{
	real smallest_dsqd, largest_dsqd;
	int i, j;
	kdtree_node_t* node;

	node = kdtree_nodeid_to_node(kd, nodeid);
	/* Early exit - FIXME benchmark to see if this actually helps */
	smallest_dsqd = kdtree_node_point_mindist2_bailout(kd, node, pt, maxdistsqd);
	//smallest_dsqd = kdtree_node_point_mindist2(kd, node, pt);
	if (smallest_dsqd > maxdistsqd) {
		return;
	}

	/* FIXME benchmark to see if this helps: if the whole node is within
	   range, grab all its points. */
	/* This is somewhat easy to do via the formula 2^l*i +2^l-1 */
	/*
	largest_dsqd = kdtree_node_point_maxdist2_bailout(kd, node, pt, maxdistsqd);
	if (largest_dsqd <= maxdistsqd) {
		for (i=node->l; i<=node->r; i++) {
			// HACK - some/many users don't care about the dist...
			results_sqd[res->nres] = dist2(kd->data + i * kd->ndim, pt, kd->ndim);
			results_inds[res->nres] = kd->perm[i];
			memcpy(results + res->nres*kd->ndim, COORD(i, 0), sizeof(real)*kd->ndim);
			res->nres++;
			if (res->nres >= KDTREE_MAX_RESULTS) {
				fprintf(stderr, "\nkdtree rangesearch overflow.\n");
				overflow = 1;
				break;
			}
		}
		return;
	}
	*/

	if (ISLEAF(nodeid)) {
		for (i=node->l; i<=node->r; i++) {
			real dsqd = 0.0;
			for (j = 0; j < kd->ndim; j++) {
				real delta = pt[j] - (*COORD(i, j));
				dsqd += delta * delta;
			}
			if (dsqd < maxdistsqd) {
				results_sqd[res->nres] = dsqd;
				results_inds[res->nres] = kd->perm[i];
				memcpy(results + res->nres*kd->ndim, COORD(i, 0), sizeof(real)*kd->ndim);
				res->nres++;
				if (res->nres >= KDTREE_MAX_RESULTS) {
					fprintf(stderr, "\nkdtree rangesearch overflow.\n");
					overflow = 1;
					break;
				}
			}
		}
	} else {
		kdtree_rangesearch_actual(kd, 2*nodeid + 1, pt, maxdistsqd, res);
		if (overflow)
			return;
		kdtree_rangesearch_actual(kd, 2*nodeid + 2, pt, maxdistsqd, res);
	}
}

/* Sorts results by kq->sdists */
int kdtree_qsort_results(kdtree_qres_t *kq, int D)
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
				piv_vec[j] = kq->results[D * L + j];
			piv_perm = kq->inds[L];
			if (i == KDTREE_MAX_RESULTS - 1) /* Sanity */
				assert(0);
			while (L < R) {
				while (kq->sdists[R] >= piv && L < R)
					R--;
				if (L < R) {
					for (j = 0;j < D;j++)
						kq->results[D*L + j] = kq->results[D * R + j];
					kq->inds[L] = kq->inds[R];
					kq->sdists[L] = kq->sdists[R];
					L++;
				}
				while (kq->sdists[L] <= piv && L < R)
					L++;
				if (L < R) {
					for (j = 0;j < D;j++)
						kq->results[D*R + j] = kq->results[D * L + j];
					kq->inds[R] = kq->inds[L];
					kq->sdists[R] = kq->sdists[L];
					R--;
				}
			}
			for (j = 0;j < D;j++)
				kq->results[D*L + j] = piv_vec[j];
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

/* Range seach */
kdtree_qres_t *kdtree_rangesearch(kdtree_t *kd, real *pt, real maxdistsquared)
{
	kdtree_qres_t *res;
	if (!kd || !pt)
		return NULL;
	res = malloc(sizeof(kdtree_qres_t));
	res->nres = 0;
	overflow = 0;

	results = malloc(KDTREE_MAX_RESULTS * kd->ndim * sizeof(real));

	/* Do the real rangesearch */
	kdtree_rangesearch_actual(kd, 0, pt, maxdistsquared, res);

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

	free(results);
	results = NULL;

	return res;
}

