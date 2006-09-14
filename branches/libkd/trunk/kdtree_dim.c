#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "kdtree.h"
#include "kdtree_macros.h"
#include "kdtree_access.h"
#include "kdtree_internal.h"

#if 0

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

static inline real KDFUNC(dist2_exceeds)(real* p1, real* p2, int d, real maxd2) {
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

#endif//if0


// declarations (should go elsewhere)
kdtree_t* KDMANGLE(kdtree_build, double, double)(double* data, int N, int D, int maxlevel, bool bb, bool copydata);
kdtree_t* KDMANGLE(kdtree_build, float, float)(float* data, int N, int D, int maxlevel, bool bb, bool copydata);
kdtree_t* KDMANGLE(kdtree_build, double, u32)(double* data, int N, int D, int maxlevel, bool bb, bool copydata);
kdtree_t* KDMANGLE(kdtree_build, double, u16)(double* data, int N, int D, int maxlevel, bool bb, bool copydata);

/* Build a tree from an array of data, of size N*D*sizeof(real) */
/* If the root node is level 0, then maxlevel is the level at which there may
 * not be enough points to keep the tree complete (i.e. last level) */
kdtree_t* KDFUNC(kdtree_build)
	 (void *data, int N, int D, int maxlevel, int treetype, bool bb,
	  bool copydata) {
	switch (treetype) {
	case KDTT_DOUBLE:
		return KDMANGLE(kdtree_build, double, double)((double*)data, N, D, maxlevel, bb, copydata);
		break;
	case KDTT_FLOAT:
		return KDMANGLE(kdtree_build, float, float)((float*)data, N, D, maxlevel, bb, copydata);
		break;
	case KDTT_DOUBLE_U32:
		return KDMANGLE(kdtree_build, double, u32)((double*)data, N, D, maxlevel, bb, copydata);
		break;
	case KDTT_DOUBLE_U16:
		return KDMANGLE(kdtree_build, double, u16)((double*)data, N, D, maxlevel, bb, copydata);
		break;
	default:
		break;
	}
	return NULL;
}


#if 0

inline static
bool resize_results(kdtree_qres_t* res, int newsize, int d,
					bool do_dists) {
	if (do_dists)
		res->sdists  = realloc(res->sdists , newsize * sizeof(real));
	res->results = realloc(res->results, newsize * d * sizeof(real));
	res->inds    = realloc(res->inds   , newsize * sizeof(unsigned int));
	if (newsize && (!res->results || (do_dists && !res->sdists) || !res->inds))
		fprintf(stderr, "Failed to resize kdtree results arrays.\n");
	return TRUE;
}

inline static
bool add_result(kdtree_qres_t* res, real sdist, unsigned int ind, real* pt,
				int d, int* res_size, bool do_dists) {
	if (do_dists)
		res->sdists[res->nres] = sdist;
	res->inds  [res->nres] = ind;
	memcpy(res->results + res->nres * d, pt, sizeof(real) * d);
	res->nres++;
	
	if (res->nres == *res_size) {
		// enlarge arrays.
		*res_size *= 2;
		return resize_results(res, *res_size, d, do_dists);
	}
	return TRUE;
}

/* Range seach */
kdtree_qres_t* KDFUNC(kdtree_rangesearch)
	 (kdtree_t *kd, real *pt, real maxd2) {
	return KDFUNC(kdtree_rangesearch_options)(kd, pt, maxd2, KD_OPTIONS_COMPUTE_DISTS | KD_OPTIONS_SORT_DISTS);
}

kdtree_qres_t* KDFUNC(kdtree_rangesearch_nosort)
	 (kdtree_t *kd, real *pt, real maxd2) {
	return KDFUNC(kdtree_rangesearch_options)(kd, pt, maxd2, KD_OPTIONS_COMPUTE_DISTS);
}

kdtree_qres_t* KDFUNC(kdtree_rangesearch_options)
	 (kdtree_t *kd, real *pt, real maxd2, int options) {
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
		if (do_wholenode_check &&
			!KDFUNC(kdtree_node_point_maxdist2_exceeds)(kd, node, pt, maxd2)) {
			if (do_dists) {
				for (i=node->l; i<=node->r; i++)
					if (!add_result(res, KDFUNC(dist2)(kd->data + i * D, pt, D),
									(kd->perm ? kd->perm[i] : i), COORD(i, 0), D, p_res_size, do_dists))
						return NULL;
			} else {
				for (i=node->l; i<=node->r; i++)
					if (!add_result(res, 0.0, (kd->perm ? kd->perm[i] : i), COORD(i, 0), D, p_res_size, do_dists))
						return NULL;
			}
			continue;
		}
		if (ISLEAF(nodeid)) {
			if (do_dists) {
				for (i=node->l; i<=node->r; i++) {
					// FIXME benchmark dist2 vs dist2_bailout.
					real dsqd = KDFUNC(dist2_bailout)(pt, COORD(i, 0), D, maxd2);
					if (dsqd == -1.0)
						continue;
					if (!add_result(res, dsqd, (kd->perm ? kd->perm[i] : i), COORD(i, 0), D, p_res_size, do_dists))
						return NULL;
				}
			} else {
				for (i=node->l; i<=node->r; i++) {
					if (KDFUNC(dist2_exceeds)(pt, COORD(i, 0), D, maxd2))
						continue;
					if (!add_result(res, 0.0, (kd->perm ? kd->perm[i] : i), COORD(i, 0), D, p_res_size, do_dists))
						return NULL;
				}
			}
			continue;
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

#endif //if0
