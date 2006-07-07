#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "kdtree.h"

/*****************************************************************************/
/* Utility macros                                                            */
/*****************************************************************************/
#define KDTREE_MAX_DIM 10
#define KDTREE_MAX_RESULTS 1000

/* Most macros operate on a variable kdtree_t *kd, assumed to exist. */
/* x is a node index, d is a dimension, and n is a point index */
#define NODE_SIZE      (sizeof(kdtree_node_t) + sizeof(real)*kd->ndim*2)

#define LOW_HR(x, d)   ((real*) (((void*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*(d)))
#define HIGH_HR(x, d)  ((real*) (((void*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*((d) \
                                 + kd->ndim)))
#define NODE(x)        ((kdtree_node_t*) (((void*)kd->tree) + NODE_SIZE*(x)))
#define PARENT(x)      NODE(((x)-1)/2)
#define CHILD_NEG(x)   NODE(2*(x) + 1)
#define CHILD_POS(x)   NODE(2*(x) + 2)
#define ISLEAF(x)      ((2*(x)+1) >= kd->nnodes)
#define COORD(n,d)     ((real*)(kd->data + kd->ndim*(n) + (d)))

static inline real dist2(real* p1, real* p2, int d) {
	int i;
	real d2 = 0.0;
	for (i=0; i<d; i++)
		d2 += (p1[i] - p2[i]) * (p1[i] - p2[i]);
	return d2;
}

static inline int dist2_exceeds(real* p1, real* p2, int d, real maxd2) {
	int i;
	real d2 = 0.0;
	for (i=0; i<d; i++) {
		d2 += (p1[i] - p2[i]) * (p1[i] - p2[i]);
		if (d2 > maxd2)
			return 1;
	}
	return 0;
}

/*****************************************************************************/
/* Building routines                                                         */
/*****************************************************************************/

void kdtree_inverse_permutation(kdtree_t* tree, int* invperm) {
	int i;
	if (!tree->perm) {
		for (i=0; i<tree->ndata; i++)
			invperm[i] = i;
	} else {
		for (i=0; i<tree->ndata; i++)
			invperm[tree->perm[i]] = i;
	}
}

int kdtree_compute_levels(int N, int Nleaf) {
    int levels;
	assert(Nleaf);
	levels = (int)rint(log(N / (double)Nleaf) * M_LOG2E);
    if (levels < 1)
        levels = 1;
	return levels;
}

#ifdef AMNSLOW

real* kdqsort_arr;
int kdqsort_D;

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
kdtree_t *kdtree_build(real *data, int N, int D, int maxlevel)
{
	int i;
	kdtree_t *kd;
	int nnodes;
	int level = 0, dim=-1, m;
	int lnext;

	assert(maxlevel > 0);
	assert(D <= KDTREE_MAX_DIM);

	/* Parameters checking */
	if (!data || !N || !D)
		return NULL;
	/* Make sure we have enough data */
	if ((1 << maxlevel) - 1 > N)
		return NULL;

	/* Set the tree fields */
	kd = malloc(sizeof(kdtree_t));
	nnodes = (1 << maxlevel) - 1;
	kd->ndata = N;
	kd->ndim = D;
	kd->nnodes = nnodes;
	kd->data = data;

	/* perm stores the permutation indexes. This gets shuffled around during
	 * sorts to keep track of the original index. */
	kd->perm = malloc(sizeof(unsigned int) * N);
	for (i = 0;i < N;i++)
		kd->perm[i] = i;
	assert(kd->perm);
	kd->tree = malloc(NODE_SIZE * (nnodes));
	assert(kd->tree);

	NODE(0)->l = 0;
	NODE(0)->r = N - 1;

	lnext = 1;
	level = 0;

	/* And in one shot, make the kdtree. Each iteration we set our
	 * children's [l,r] array bounds and pivot our own subset. */
	for (i = 0; i < nnodes; i++) {
		real hi[D];
		real lo[D];
		int j, d;
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

		/* Find the bounding-box for this node. */
		for (d=0; d<D; d++) {
			hi[d] = -KDT_INFTY;
			lo[d] = KDT_INFTY;
		}
		// haha, how sick is this? Take advantage of the way data is stored.
		pdata = kd->data + NODE(i)->l * D;
		for (j=NODE(i)->l; j<=NODE(i)->r; j++) {
			for (d=0; d<D; d++) {
				if (*pdata > hi[d]) hi[d] = *pdata;
				if (*pdata < lo[d]) lo[d] = *pdata;
				pdata++;
			}
		}

		for (d=0; d<D; d++) {
			*LOW_HR(i, d) = lo[d];
			*HIGH_HR(i, d) = hi[d];
		}

		/*
		  for (j=NODE(i)->l; j<=NODE(i)->r; j++) {
		  for (d=0; d<D; d++) {
		  assert(*LOW_HR(i,d) <= *COORD(j, d));
		  assert(*HIGH_HR(i,d) >= *COORD(j, d));
		  }
		  }
		*/

		/* Decide which dimension to split along: we use the dimension with
		   largest range. */
		maxrange = -1.0;
		for (d=0; d<D; d++)
			if ((hi[d] - lo[d]) > maxrange) {
				maxrange = hi[d] - lo[d];
				dim = d;
			}

		/* Pivot the data at the median */
		m = kdtree_quickselect_partition(data, kd->perm, NODE(i)->l, NODE(i)->r, D, dim);

		/* Only do child operations if we're not the last layer */
		if (level < maxlevel - 1) {
			assert(2*i + 1 < nnodes);
			assert(2*i + 2 < nnodes);
			CHILD_NEG(i)->l = NODE(i)->l;
			CHILD_NEG(i)->r = m - 1;
			CHILD_POS(i)->l = m;
			CHILD_POS(i)->r	= NODE(i)->r;
		}
	}
	return kd;
}

/*****************************************************************************/
/* Querying routines                                                         */
/*****************************************************************************/

int kdtree_node_check(kdtree_t* kd, kdtree_node_t* node, int nodeid) {
	int sum, i;
	real dsum;
	real *bblo, *bbhi;
	// scan through the arrays owned by this node
	sum = 0;
	dsum = 0.0;
	bblo = kdtree_get_bb_low(kd, node);
	bbhi = kdtree_get_bb_high(kd, node);

	assert(node->l < kd->ndata);
	assert(node->r < kd->ndata);
	assert(node->l >= 0);
	assert(node->r >= 0);
	assert(node->l <= node->r);

	// if it's the root node, make sure that each value in the permutation
	// array is present exactly once.
	if (!nodeid && kd->perm) {
		unsigned char* counts = calloc(kd->ndata, 1);
		for (i=node->l; i<=node->r; i++)
			counts[kd->perm[i]]++;
		for (i=node->l; i<=node->r; i++)
			assert(counts[i] == 1);
		free(counts);
	}

	if (kd->perm) {
		for (i=node->l; i<=node->r; i++) {
			sum += kd->perm[i];
			assert(kd->perm[i] >= 0);
			assert(kd->perm[i] < kd->ndata);
		}
	}
	if (ISLEAF(nodeid)) {
		for (i=node->l; i<=node->r; i++) {
			// check that the point is within the bounding box.
			assert(kdtree_is_point_in_rect(bblo, bbhi, kd->data + i*kd->ndim, kd->ndim));
		}
	} else {
		// check that the children's bounding box corners are within
		// the parent's bounding box.
		kdtree_node_t* child;
		real* bb;
		child = kdtree_get_child1(kd, node);
		bb = kdtree_get_bb_low(kd, child);
		assert(kdtree_is_point_in_rect(bblo, bbhi, bb, kd->ndim));
		bb = kdtree_get_bb_high(kd, child);
		assert(kdtree_is_point_in_rect(bblo, bbhi, bb, kd->ndim));
		child = kdtree_get_child2(kd, node);
		bb = kdtree_get_bb_low(kd, child);
		assert(kdtree_is_point_in_rect(bblo, bbhi, bb, kd->ndim));
		bb = kdtree_get_bb_high(kd, child);
		assert(kdtree_is_point_in_rect(bblo, bbhi, bb, kd->ndim));
	}
	return 0;
}

int kdtree_check(kdtree_t* kd) {
	int i;
	for (i=0; i<kd->nnodes; i++) {
		if (kdtree_node_check(kd, NODE(i), i))
			return -1;
	}
	return 0;
}

inline
kdtree_node_t* kdtree_get_root(kdtree_t* kd)
{
	return kd->tree;
}

inline
real* kdtree_node_get_point(kdtree_t* tree, kdtree_node_t* node, int ind)
{
	return tree->data + (node->l + ind) * tree->ndim;
}

inline
int kdtree_node_get_index(kdtree_t* tree, kdtree_node_t* node, int ind)
{
	if (!tree->perm)
		return node->l + ind;
	return tree->perm[(node->l + ind)];
}

inline
real* kdtree_get_bb_low(kdtree_t* tree, kdtree_node_t* node)
{
	return (real*)(node + 1);
}

inline
real* kdtree_get_bb_high(kdtree_t* tree, kdtree_node_t* node)
{
	return (real*)((char*)(node + 1) + sizeof(real) * tree->ndim);
}

inline
int kdtree_is_point_in_rect(real* bblo, real* bbhi, real* point, int dim) {
	int i;
	for (i = 0; i < dim; i++) {
		real lo, hi;
		lo = bblo[i];
		hi = bbhi[i];
		if (point[i] < lo) return 0;
		if (point[i] > hi) return 0;
	}
	return 1;
}

inline
real kdtree_bb_mindist2(real* bblow1, real* bbhigh1, real* bblow2, real* bbhigh2, int dim)
{
	real d2 = 0.0;
	real delta;
	int i;
	for (i = 0; i < dim; i++) {
		real alo, ahi, blo, bhi;
		alo = bblow1[i];
		ahi = bbhigh1[i];
		blo = bblow2[i];
		bhi = bbhigh2[i];
		if ( ahi < blo ) {
			delta = blo - ahi;
			d2 += delta * delta;
		} else if ( bhi < alo ) {
			delta = alo - bhi;
			d2 += delta * delta;
		} // else delta=0.0;
	}
	return d2;
}

inline
real kdtree_bb_mindist2_exceeds(real* bblow1, real* bbhigh1,
								real* bblow2, real* bbhigh2, int dim,
								real maxd2) {
	real d2 = 0.0;
	real delta;
	int i;
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

inline
real kdtree_bb_maxdist2(real* bblow1, real* bbhigh1, real* bblow2, real* bbhigh2, int dim)
{
	real d2 = 0.0;
	real delta;
	int i;
	for (i = 0; i < dim; i++) {
		real alo, ahi, blo, bhi;
		real delta2;
		alo = bblow1[i];
		ahi = bbhigh1[i];
		blo = bblow2[i];
		bhi = bbhigh2[i];
		delta = bhi - alo;
		delta2 = ahi - blo;
		if (delta2 > delta)
			delta = delta2;
		d2 += delta * delta;
	}
	return d2;
}

inline
real kdtree_bb_maxdist2_exceeds(real* bblow1, real* bbhigh1,
								real* bblow2, real* bbhigh2, int dim,
								real maxd2)
{
	real d2 = 0.0;
	real delta;
	int i;
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


inline
real kdtree_bb_point_mindist2(real* bblow, real* bbhigh,
                              real* point, int dim)
{
	real d2 = 0.0;
	real delta;
	int i;
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
	}
	return d2;
}

inline
int kdtree_bb_point_mindist2_exceeds(real* bblow, real* bbhigh,
									 real* point, int dim, real maxd2) {
	real d2 = 0.0;
	real delta;
	int i;
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
			return 1;
	}
	return 0;
}

inline
real kdtree_bb_point_maxdist2(real* bblow, real* bbhigh, real* point, int dim)
{
	real d2 = 0.0;
	real delta1, delta2;
	int i;
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
	}
	return d2;
}

inline
real kdtree_bb_point_maxdist2_exceeds(real* bblow, real* bbhigh,
									  real* point, int dim, double maxd2)
{
	real d2 = 0.0;
	real delta1, delta2;
	int i;
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
			return 1;
	}
	return 0;
}

inline
int kdtree_node_to_nodeid(kdtree_t* kd, kdtree_node_t* node)
{
	return ((char*)node - (char*)kd->tree) / NODE_SIZE;
}

inline
kdtree_node_t* kdtree_nodeid_to_node(kdtree_t* kd, int nodeid)
{
	return NODE(nodeid);
}

inline
int kdtree_get_childid1(kdtree_t* kd, int nodeid)
{
	assert(!ISLEAF(nodeid));
	return 2*nodeid + 1;
}

inline
int kdtree_get_childid2(kdtree_t* kd, int nodeid)
{
	assert(!ISLEAF(nodeid));
	return 2*nodeid + 2;
}

inline
kdtree_node_t* kdtree_get_child1(kdtree_t* kd, kdtree_node_t* node)
{
	int nodeid = kdtree_node_to_nodeid(kd, node);
	if (ISLEAF(nodeid))
		return NULL;
	return CHILD_NEG(nodeid);
}

inline
kdtree_node_t* kdtree_get_child2(kdtree_t* kd, kdtree_node_t* node)
{
	int nodeid = kdtree_node_to_nodeid(kd, node);
	if (ISLEAF(nodeid))
		return NULL;
	return CHILD_POS(nodeid);
}

inline
int kdtree_node_is_leaf(kdtree_t* kd, kdtree_node_t* node)
{
	int nodeid = kdtree_node_to_nodeid(kd, node);
	return ISLEAF(nodeid);
}

inline
int kdtree_nodeid_is_leaf(kdtree_t* kd, int nodeid)
{
	return ISLEAF(nodeid);
}

inline
int kdtree_node_npoints(kdtree_node_t* node)
{
	return 1 + node->r - node->l;
}

inline
real kdtree_node_volume(kdtree_t* kd, kdtree_node_t* node) {
	real vol = 1.0;
	int i, D;
	real* lo, *hi;
	D = kd->ndim;
	lo = kdtree_get_bb_low(kd, node);
	hi = kdtree_get_bb_high(kd, node);
	for (i=0; i<D; i++)
		vol *= (hi[i] - lo[i]);
	return vol;
}

inline
real kdtree_node_node_mindist2(kdtree_t* tree1, kdtree_node_t* node1,
                               kdtree_t* tree2, kdtree_node_t* node2)
{
	int dim = tree1->ndim;
	real *lo1, *hi1, *lo2, *hi2;
	lo1 = kdtree_get_bb_low (tree1, node1);
	hi1 = kdtree_get_bb_high(tree1, node1);
	lo2 = kdtree_get_bb_low (tree2, node2);
	hi2 = kdtree_get_bb_high(tree2, node2);
	return kdtree_bb_mindist2(lo1, hi1, lo2, hi2, dim);
}

inline
int kdtree_node_node_mindist2_exceeds(kdtree_t* tree1, kdtree_node_t* node1,
									  kdtree_t* tree2, kdtree_node_t* node2,
									  real maxd2) {
	int dim = tree1->ndim;
	real *lo1, *hi1, *lo2, *hi2;
	lo1 = kdtree_get_bb_low (tree1, node1);
	hi1 = kdtree_get_bb_high(tree1, node1);
	lo2 = kdtree_get_bb_low (tree2, node2);
	hi2 = kdtree_get_bb_high(tree2, node2);
	return kdtree_bb_mindist2_exceeds(lo1, hi1, lo2, hi2, dim, maxd2);
}

inline
real kdtree_node_node_maxdist2(kdtree_t* tree1, kdtree_node_t* node1,
                               kdtree_t* tree2, kdtree_node_t* node2)
{
	int dim = tree1->ndim;
	real *lo1, *hi1, *lo2, *hi2;
	lo1 = kdtree_get_bb_low (tree1, node1);
	hi1 = kdtree_get_bb_high(tree1, node1);
	lo2 = kdtree_get_bb_low (tree2, node2);
	hi2 = kdtree_get_bb_high(tree2, node2);
	return kdtree_bb_maxdist2(lo1, hi1, lo2, hi2, dim);
}

inline
real kdtree_node_node_maxdist2_exceeds(kdtree_t* tree1, kdtree_node_t* node1,
									   kdtree_t* tree2, kdtree_node_t* node2,
									   real maxd2) {
	int dim = tree1->ndim;
	real *lo1, *hi1, *lo2, *hi2;
	lo1 = kdtree_get_bb_low (tree1, node1);
	hi1 = kdtree_get_bb_high(tree1, node1);
	lo2 = kdtree_get_bb_low (tree2, node2);
	hi2 = kdtree_get_bb_high(tree2, node2);
	return kdtree_bb_maxdist2_exceeds(lo1, hi1, lo2, hi2, dim, maxd2);
}

void kdtree_rangesearch_cb_rec(kdtree_t *kd, kdtree_node_t* node,
							   real *pt, real maxd2,
							   void (*rangesearch_callback)(kdtree_t* kd, real* pt, real maxd2, real* computed_d2, int indx, void* extra),
							   void* extra) {
	int i;
	/* Early exit - FIXME benchmark to see if this actually helps */
	if (kdtree_node_point_mindist2_exceeds(kd, node, pt, maxd2))
		return;
	/* FIXME benchmark to see if this helps: if the whole node is within
	   range, grab all its points. */
	if (!kdtree_node_point_maxdist2_exceeds(kd, node, pt, maxd2)) {
		for (i=node->l; i<=node->r; i++)
			rangesearch_callback(kd, pt, maxd2, NULL, i, extra);
		return;
	}
	if (kdtree_node_is_leaf(kd, node))
		for (i=node->l; i<=node->r; i++) {
			real dsqd = dist2(kd->data + i*kd->ndim, pt, kd->ndim);
			if (dsqd <= maxd2)
				rangesearch_callback(kd, pt, maxd2, &dsqd, i, extra);
		}
	else {
		kdtree_rangesearch_cb_rec(kd, kdtree_get_child1(kd, node), pt,
								  maxd2, rangesearch_callback, extra);
		kdtree_rangesearch_cb_rec(kd, kdtree_get_child2(kd, node), pt,
								  maxd2, rangesearch_callback, extra);
	}
}

void kdtree_rangesearch_callback(kdtree_t *kd, real *pt, real maxdistsquared,
								 void (*rangesearch_callback)(kdtree_t* kd, real* pt, real maxdist2, real* computed_d2, int indx, void* extra),
								 void* extra) {
	kdtree_rangesearch_cb_rec(kd, kdtree_get_root(kd), pt, maxdistsquared,
							  rangesearch_callback, extra);
}

inline static
void resize_results(kdtree_qres_t* res, int newsize, int d) {
	res->results = realloc(res->results, newsize * d * sizeof(real));
	res->sdists  = realloc(res->sdists , newsize * sizeof(real));
	res->inds    = realloc(res->inds   , newsize * sizeof(unsigned int));
}

inline static
void add_result(kdtree_qres_t* res, real sdist, unsigned int ind, real* pt,
				int d, int* res_size) {
	res->sdists[res->nres] = sdist;
	res->inds  [res->nres] = ind;
	memcpy(res->results + res->nres * d, pt, sizeof(real) * d);
	res->nres++;
	
	if (res->nres == *res_size) {
		// enlarge arrays.
		*res_size *= 2;
		resize_results(res, *res_size, d);
	}
}

/* Range seach helper */
void kdtree_rangesearch_actual(kdtree_t *kd, int nodeid, real *pt, real maxdistsqd,
							   kdtree_qres_t *res, int* res_size)
{
	int i, j;
	kdtree_node_t* node;

	node = kdtree_nodeid_to_node(kd, nodeid);
	/* Early exit - FIXME benchmark to see if this actually helps */
	if (kdtree_node_point_mindist2_exceeds(kd, node, pt, maxdistsqd))
		return;

	/* FIXME benchmark to see if this helps: if the whole node is within
	   range, grab all its points. */
	if (!kdtree_node_point_maxdist2_exceeds(kd, node, pt, maxdistsqd)) {
		for (i=node->l; i<=node->r; i++) {
			add_result(res, dist2(kd->data + i * kd->ndim, pt, kd->ndim),
					   (kd->perm ? kd->perm[i] : i), COORD(i, 0), kd->ndim, res_size);
		}
		return;
	}

	if (ISLEAF(nodeid)) {
		for (i=node->l; i<=node->r; i++) {
			real dsqd = 0.0;
			for (j = 0; j < kd->ndim; j++) {
				real delta = pt[j] - (*COORD(i, j));
				dsqd += delta * delta;
			}
			if (dsqd < maxdistsqd) {
				add_result(res, dsqd, (kd->perm ? kd->perm[i] : i), COORD(i, 0), kd->ndim,
						   res_size);
			}
		}
	} else {
		kdtree_rangesearch_actual(kd, 2*nodeid + 1, pt, maxdistsqd, res, res_size);
		kdtree_rangesearch_actual(kd, 2*nodeid + 2, pt, maxdistsqd, res, res_size);
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
	int res_size = KDTREE_MAX_RESULTS;
	if (!kd || !pt)
		return NULL;
	res = calloc(1, sizeof(kdtree_qres_t));
	resize_results(res, res_size, kd->ndim);

	/* Do the real rangesearch */
	kdtree_rangesearch_actual(kd, 0, pt, maxdistsquared, res, &res_size);

	/* Resize result arrays. */
	resize_results(res, res->nres, kd->ndim);

	/* Sort by ascending distance away from target point before returning */
	kdtree_qsort_results(res, kd->ndim);

	return res;
}

int kdtree_rangecount_rec(kdtree_t* kd, kdtree_node_t* node,
						  real* pt, real maxd2) {
	int i;

	if (kdtree_node_point_mindist2_exceeds(kd, node, pt, maxd2))
		return 0;

	/* FIXME benchmark to see if this helps: if the whole node is within
	   range, grab all its points. */
	if (!kdtree_node_point_maxdist2_exceeds(kd, node, pt, maxd2))
		return node->r - node->l + 1;

	if (kdtree_node_is_leaf(kd, node)) {
		int n = 0;
		for (i=node->l; i<=node->r; i++)
			if (!dist2_exceeds(kd->data + i*kd->ndim, pt, kd->ndim, maxd2))
				n++;
		return n;
	} else {
		return
			kdtree_rangecount_rec(kd, kdtree_get_child1(kd, node), pt, maxd2) +
			kdtree_rangecount_rec(kd, kdtree_get_child2(kd, node), pt, maxd2);
	}
}

int kdtree_rangecount(kdtree_t* kd, real* pt, real maxdistsquared) {
	return kdtree_rangecount_rec(kd, kdtree_get_root(kd), pt, maxdistsquared);
}

inline
real kdtree_node_point_mindist2(kdtree_t* kd, kdtree_node_t* node, real* pt) {
	return kdtree_bb_point_mindist2(kdtree_get_bb_low(kd, node),
									kdtree_get_bb_high(kd, node),
									pt, kd->ndim);
}

inline
int kdtree_node_point_mindist2_exceeds(kdtree_t* kd, kdtree_node_t* node,
									   real* pt, real maxd2) {
	return kdtree_bb_point_mindist2_exceeds
		(kdtree_get_bb_low(kd, node),
		 kdtree_get_bb_high(kd, node),
		 pt, kd->ndim, maxd2);
}

inline
real kdtree_node_point_maxdist2(kdtree_t* kd, kdtree_node_t* node, real* pt) {
	return kdtree_bb_point_maxdist2(kdtree_get_bb_low(kd, node),
									kdtree_get_bb_high(kd, node),
									pt, kd->ndim);
}

inline
int kdtree_node_point_maxdist2_exceeds(kdtree_t* kd, kdtree_node_t* node,
										real* pt, real maxd2) {
	return kdtree_bb_point_maxdist2_exceeds
		(kdtree_get_bb_low(kd, node),
		 kdtree_get_bb_high(kd, node),
		 pt, kd->ndim, maxd2);
}

void kdtree_nn_recurse(kdtree_t* kd, kdtree_node_t* node, real* pt,
					   real* bestmaxdist2, int* best_sofar) {
	kdtree_node_t *child1, *child2, *nearchild, *farchild;
	real child1mindist2, child2mindist2, nearmindist2, farmindist2;

	if (kdtree_node_is_leaf(kd, node)) {
		// compute the distance to each point owned by this node and return
		// the index of the best one.
		int ind;
		for (ind=node->l; ind<=node->r; ind++) {
			real* p = kd->data + ind * kd->ndim;
			real d2 = dist2(pt, p, kd->ndim);
			if (d2 < *bestmaxdist2) {
				*bestmaxdist2 = d2;
				*best_sofar = ind;
			}
		}
		return;
	}

	// compute the mindists to this node's children; recurse on the
	// closer one.  if, after the recursion returns, the other one's
	// mindist is less than the best maxdist found, then recurse on the
	// further child.

	child1 = kdtree_get_child1(kd, node);
	child2 = kdtree_get_child2(kd, node);
	child1mindist2 = kdtree_node_point_mindist2(kd, child1, pt);
	child2mindist2 = kdtree_node_point_mindist2(kd, child2, pt);

	if (child1mindist2 < child2mindist2) {
		nearchild = child1;
		nearmindist2 = child1mindist2;
		farchild = child2;
		farmindist2 = child2mindist2;
	} else {
		nearchild = child2;
		nearmindist2 = child2mindist2;
		farchild = child1;
		farmindist2 = child1mindist2;
	}

	// should we bother recursing on the near child?
	if (nearmindist2 > *bestmaxdist2)
		// we can't do better than the existing best.
		return;

	// recurse on the near child.
	kdtree_nn_recurse(kd, nearchild, pt, bestmaxdist2, best_sofar);

	// check if we should bother recursing on the far child.
	if (farmindist2 > *bestmaxdist2)
		return;

	// recurse on the far child.
	kdtree_nn_recurse(kd, farchild, pt, bestmaxdist2, best_sofar);
}

int kdtree_nearest_neighbour(kdtree_t* kd, real* pt, real* p_mindist2) {
	return kdtree_nearest_neighbour_within(kd, pt, 1e300, p_mindist2);
}

int kdtree_nearest_neighbour_within(kdtree_t* kd, real *pt, real maxd2,
									real* p_mindist2) {
	real bestd2 = maxd2;
	int ibest = -1;
	kdtree_nn_recurse(kd, kdtree_get_root(kd), pt, &bestd2, &ibest);
	if (p_mindist2 && (ibest != -1))
		*p_mindist2 = bestd2;
	return ibest;
}

/* Optimize the KDTree by by constricting hyperrectangles to minimum volume */
void kdtree_optimize(kdtree_t *kd)
{
	real high[KDTREE_MAX_DIM];
	real low[KDTREE_MAX_DIM];
	int i, j, k;

	for ( i = 0; i < kd->nnodes; i++ ) {
		for (k = 0; k < kd->ndim; k++) {
			high[k] = -KDT_INFTY;
			low[k] = KDT_INFTY;
		}
		for (j = NODE(i)->l; j <= NODE(i)->r; j++) {
			for (k = 0; k < kd->ndim; k++) {
				if (high[k] < *COORD(j, k))
					high[k] = *COORD(j, k);
				if (low[k] > *COORD(j, k))
					low[k] = *COORD(j, k);
			}
		}
		for (k = 0; k < kd->ndim; k++) {
			*LOW_HR(i, k) = low[k];
			*HIGH_HR(i, k) = high[k];
		}
	}
}

void kdtree_free_query(kdtree_qres_t *kq)
{
	assert(kq);
	free(kq->results);
	free(kq->sdists);
	free(kq->inds);
	free(kq);
}

void kdtree_free(kdtree_t *kd)
{
	assert(kd);
	assert(kd->tree);
	/* We don't free kd->data */
	free(kd->perm);
	free(kd->tree);
	free(kd);
}

/*****************************************************************************/
/* Output routines                                                           */
/*****************************************************************************/

/* Output a graphviz style description of the tree, for input to dot program */
void kdtree_output_dot(FILE* fid, kdtree_t* kd)
{
	int i, j, D = kd->ndim;
	fprintf(fid, "digraph {\nnode [shape = record];\n");

	for (i = 0; i < kd->nnodes; i++) {
		fprintf(fid, "node%d [ label =\"<f0> %d | <f1> (%d) \\n L=(",
		        i, NODE(i)->l, i);
		for (j = 0; j < D; j++) {
			fprintf(fid, "%.2lf", *LOW_HR(i, j));
			if (j < D - 1)
				fprintf(fid, ",");
		}
		fprintf(fid, ") \\n H=(");
		for (j = 0; j < D; j++) {
			fprintf(fid, "%.2lf", *HIGH_HR(i, j));
			if (j < D - 1)
				fprintf(fid, ",");
		}
		fprintf(fid, ") | <f2> %d \"];\n",
		        NODE(i)->r);
		if ((2*i + 2) < kd->nnodes) {
			fprintf(fid, "\"node%d\":f2 -> \"node%d\":f1;\n" , i, 2*i + 2);
		}
		if ((2*i + 1) < kd->nnodes) {
			fprintf(fid, "\"node%d\":f0 -> \"node%d\":f1;\n" , i, 2*i + 1);
		}
	}

	fprintf(fid, "}\n");
}
