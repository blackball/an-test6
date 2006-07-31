#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "kdtree.h"
#include "kdtree_macros.h"
#include "kdtree_access.h"
#include "starutil.h"

static inline real KDFUNC(dist2)(real* p1, real* p2, int d) {
	int i;
	real d2 = 0.0;
#if defined(KD_DIM)
	d = KD_DIM;
#endif
	for (i=0; i<d; i++)
		d2 += (p1[i] - p2[i]) * (p1[i] - p2[i]);
	return d2;
}

static inline real KDFUNC(dist2_bailout)(real* p1, real* p2, int d, real maxd2) {
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

int KDFUNC(kdtree_quickselect_partition)
	 (real *arr, unsigned int *parr, int l, int r, int D, int d) {
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

/* Build a tree from an array of data, of size N*D*sizeof(real) */
/* If the root node is level 0, then maxlevel is the level at which there may
 * not be enough points to keep the tree complete (i.e. last level) */
kdtree_t* KDFUNC(kdtree_build)
	 (real *data, int N, int D, int maxlevel) {
	int i;
	kdtree_t *kd;
	int nnodes;
	int level = 0, dim=-1, m;
	int lnext;

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
		/* (avoid doing kd->data[NODE(i)*D + d] many times by taking advantage of the
		   fact that the data is stored lexicographically; just use ++ on the pointer) */
		pdata = kd->data + NODE(i)->l * D;
		for (j=NODE(i)->l; j<=NODE(i)->r; j++)
			for (d=0; d<D; d++) {
				if (*pdata > hi[d]) hi[d] = *pdata;
				if (*pdata < lo[d]) lo[d] = *pdata;
				pdata++;
			}

		for (d=0; d<D; d++) {
			*LOW_HR(i, d) = lo[d];
			*HIGH_HR(i, d) = hi[d];
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
		m = KDFUNC(kdtree_quickselect_partition)(data, kd->perm, NODE(i)->l, NODE(i)->r, D, dim);

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

inline static
bool resize_results(kdtree_qres_t* res, int newsize, int d) {
	res->results = realloc(res->results, newsize * d * sizeof(real));
	res->sdists  = realloc(res->sdists , newsize * sizeof(real));
	res->inds    = realloc(res->inds   , newsize * sizeof(unsigned int));
	if (newsize && (!res->results || !res->sdists || !res->inds))
		fprintf(stderr, "Failed to resize kdtree results arrays.\n");
	return TRUE;
}

inline static
bool add_result(kdtree_qres_t* res, real sdist, unsigned int ind, real* pt,
				int d, int* res_size) {
	res->sdists[res->nres] = sdist;
	res->inds  [res->nres] = ind;
	memcpy(res->results + res->nres * d, pt, sizeof(real) * d);
	res->nres++;
	
	if (res->nres == *res_size) {
		// enlarge arrays.
		*res_size *= 2;
		return resize_results(res, *res_size, d);
	}
	return TRUE;
}

/* Range seach */
kdtree_qres_t* KDFUNC(kdtree_rangesearch)
	 (kdtree_t *kd, real *pt, real maxdistsquared)
{
	kdtree_qres_t *res;
	if (!kd || !pt)
		return NULL;
	res = KDFUNC(kdtree_rangesearch_nosort)(kd, pt, maxdistsquared);
	if (!res)
		return NULL;
	/* Sort by ascending distance away from target point before returning */
	kdtree_qsort_results(res, kd->ndim);
	return res;
}

kdtree_qres_t* KDFUNC(kdtree_rangesearch_nosort)
	 (kdtree_t *kd, real *pt, real maxd2) {
	int nodestack[100];
	int stackpos = 0;
	kdtree_qres_t *res;
	int res_size;
	int* p_res_size = &res_size;
	int D;

	if (!kd || !pt)
		return NULL;
#if defined(KD_DIM)
	assert(kd->ndim == KD_DIM);
	D = KD_DIM;
#else
	D = kd->ndim;
#endif
	res = calloc(1, sizeof(kdtree_qres_t));
	if (!res) {
		fprintf(stderr, "Failed to allocate kdtree_qres_t struct.\n");
		return NULL;
	}
	res_size = KDTREE_MAX_RESULTS;
	resize_results(res, res_size, D);

	// queue root.
	nodestack[0] = 0;

	while (stackpos >= 0) {
		kdtree_node_t* node;
		int nodeid;
		int i;

		nodeid = nodestack[stackpos];
		stackpos--;
		node = NODE(nodeid);
		if (KDFUNC(kdtree_node_point_mindist2_exceeds)(kd, node, pt, maxd2))
			continue;
		/* FIXME benchmark to see if this helps: if the whole node is within
		   range, grab all its points. */
		if (!KDFUNC(kdtree_node_point_maxdist2_exceeds)(kd, node, pt, maxd2)) {
			for (i=node->l; i<=node->r; i++) {
				if (!add_result(res, KDFUNC(dist2)(kd->data + i * D, pt, D),
								(kd->perm ? kd->perm[i] : i), COORD(i, 0), D, p_res_size))
					return NULL;
			}
			continue;
		}
		if (ISLEAF(nodeid)) {
			for (i=node->l; i<=node->r; i++) {
				// FIXME benchmark dist2 vs dist2_bailout.
				real dsqd = KDFUNC(dist2_bailout)(pt, COORD(i, 0), D, maxd2);
				if (dsqd == -1.0)
					continue;
				if (!add_result(res, dsqd, (kd->perm ? kd->perm[i] : i), COORD(i, 0), D, p_res_size))
					return NULL;
			}
			continue;
		}
		stackpos++;
		nodestack[stackpos] = 2 * nodeid + 1;
		stackpos++;
		nodestack[stackpos] = 2 * nodeid + 2;
	}

	/* Resize result arrays. */
	resize_results(res, res->nres, D);
	return res;
}

int KDFUNC(kdtree_node_point_mindist2_exceeds)
	 (kdtree_t* kd, kdtree_node_t* node,
	  real* pt, real maxd2) {
	int D;
#if defined(KD_DIM)
	D = KD_DIM;
#else
	D = kd->ndim;
#endif
	return KDFUNC(kdtree_bb_point_mindist2_exceeds)
		(NODE_LOW_BB(node), NODE_HIGH_BB(node), pt, D, maxd2);
}

int KDFUNC(kdtree_node_point_maxdist2_exceeds)
	 (kdtree_t* kd, kdtree_node_t* node,
	  real* pt, real maxd2) {
	int D;
#if defined(KD_DIM)
	D = KD_DIM;
#else
	D = kd->ndim;
#endif
	return KDFUNC(kdtree_bb_point_maxdist2_exceeds)
		(NODE_LOW_BB(node), NODE_HIGH_BB(node), pt, D, maxd2);
}

int KDFUNC(kdtree_bb_point_maxdist2_exceeds)
	 (real* bblow, real* bbhigh,
	  real* point, int dim, double maxd2)
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
			return 1;
	}
	return 0;
}

int KDFUNC(kdtree_bb_point_mindist2_exceeds)
	 (real* bblow, real* bbhigh,
	  real* point, int dim, real maxd2) {
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
			return 1;
	}
	return 0;
}

