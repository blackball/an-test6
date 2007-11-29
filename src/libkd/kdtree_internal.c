/*
  This file is part of libkd.
  Copyright 2006, 2007 Dustin Lang and Keir Mierle.

  libkd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, version 2.

  libkd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with libkd; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "kdtree.h"
#include "kdtree_internal.h"
#include "kdtree_mem.h"

#define KDTREE_MAX_RESULTS 1000
#define KDTREE_MAX_DIM 100

#define UNUSED __attribute__((unused))

#define MANGLE(x) KDMANGLE(x, ETYPE, DTYPE, TTYPE)

/*
  The "external" type is the data type that the outside world works in.

  The "data" type is the type in which we store the points.

  The "tree" type is the type in which we store the bounding boxes or splitting planes.

  Recall that    etype >= dtype >= ttype:
  .   (etype >= dtype: because there's no point storing more precision than needed by the
  .                    outside world;
  .    dtype >= ttype: because there's no point keeping more precision in the splitting
  .                    plane than exists in the data.)


  The following will be defined:

  - etype:           typedef, the "external" type.
  - ETYPE_INTEGER:   1 if the "external" space is integral (not floating-point)
  - ETYPE_MIN
  - ETYPE_MAX:       the limits of the "external" space.
  - ETYPE:           the "external" type, in a form that the preprocessor can make C
  .                  identifiers out of it: eg 'd' for double; use this to get to the
  .                  particular data type in the kdtree unions:
  .         kdtree_qres_t* query_results = ...;
  .         etype* mydata = query_results.results.ETYPE;

  - dtype:           typedef, the "data" type.
  - DTYPE_INTEGER:   1 if the "data" space is integral (not floating-point)
  - DTYPE_DOUBLE:    1 if the "data" space has type 'double'.
  - DTYPE_MIN
  - DTYPE_MAX:       the limits of the "data" space.
  - DTYPE_KDT_DATA:  eg KDT_DATA_DOUBLE
  - DTYPE:           the "data" type, in a form that the preprocessor can make C
  .                  identifiers out of it: eg 'd' for double:
  .          dtype* mydata = kd->data.DTYPE;

  - ttype:           typedef, the "tree" type.
  - TTYPE_INTEGER:   1 if the "tree" space is integral (not floating-point)
  - TTYPE_MIN
  - TTYPE_MAX:       the limits of the "tree" space.
  - TTYPE_SQRT_MAX:  the square root of the maximum value of the "tree" space.
  - TTYPE:           the "tree" type, in a form that the preprocessor can make C
  .                  identifiers out of it: eg 'd' for double.
  .          ttype* mybb = kd->bb.TTYPE;

  - bigttype:        typedef, a type that can hold a "ttype" squared.
  - BIGTTYPE:        #define the bigttype; used for "STRINGIFY" macro.
  - BIGTTYPE_MAX:    maximum value of a 'bigttype'.


  - EQUAL_ED:   1 if "external" and "data" spaces are the same.
  - EQUAL_DT:   1 if "data" and "tree" spaces are the same.
  - EQUAL_ET:   1 if "external" and "tree" spaces are the same; implies EQUAL_ED && EQUAL_DT.


  - POINT_ED(kd, d, c, func):  d: dimension;
  .                            c: coordinate in that dimension,
  .                            func: function to apply (may be empty)
  .          Converts a coordinate from "external" to "data" space.
  .          eg      POINT_ED(kd, 0, 0.1, floor);
  .                  POINT_ED(kd, 1, 0.0, );

  - POINT_DT(kd, d, c, func):
  .          Converts a coordinate from "data" to "tree" space.

  - POINT_ET(kd, d, c, func):
  .          Converts a coordinate from "external" to "tree" space.

  - POINT_TD(kd, d, c):
  .          Converts a coordinate from "tree" to "data" space.

  - POINT_DE(kd, d, c):
  .          Converts a coordinate from "data" to "external" space.

  - POINT_TE(kd, d, c):
  .          Converts a coordinate from "tree" to "external" space.


  - DIST_ED(kd, dist, func):
  - DIST2_ED(kd, dist2, func):
  .          Convert distance or distance-squared from "external" to "data" space,
  .          optionally applying a function to the result.

  - DIST_DT(kd, dist, func):
  - DIST2_DT(kd, dist2, func):
  .          Convert distance or distance-squared from "data" to "tree" space.

  - DIST_ET(kd, dist, func):
  - DIST2_ET(kd, dist2, func):
  .          Convert distance or distance-squared from "external" to "tree" space.

  - DIST_TD(kd, dist):
  - DIST2_TD(kd, dist2):
  .          Convert distance or distance-squared from "tree" to "data" space.

  - DIST_DE(kd, dist):
  - DIST2_DE(kd, dist2):
  .          Convert distance or distance-squared from "data" to "external" space.

  - DIST_TE(kd, dist):
  - DIST2_TE(kd, dist2):
  .          Convert distance or distance-squared from "tree" to "external" space.
*/

// Which function do we use for rounding?
#define KD_ROUND rint

// Get the low corner of the bounding box
#define LOW_HR( kd, D, i) ((kd)->bb.TTYPE + (2*(i)*(D)))

// Get the high corner of the bounding box
#define HIGH_HR(kd, D, i) ((kd)->bb.TTYPE + ((2*(i)+1)*(D)))

// Get the splitting-plane position
#define KD_SPLIT(kd, i) ((kd)->split.TTYPE + (i))

// Get a pointer to the 'i'-th data point.
#define KD_DATA(kd, D, i) ((kd)->data.DTYPE + ((D)*(i)))

// Get the 'i'-th element of the permutation vector, or 'i' if there is no permutation vector.
#define KD_PERM(kd, i) ((kd)->perm ? (kd)->perm[i] : i)

// Get the dimension of this tree.
#if defined(KD_DIM)
#define DIMENSION(kd)   (KD_DIM)
#else
#define DIMENSION(kd)   (kd->ndim)
#endif

// Get the size of a single point in the tree.
#define SIZEOF_PT(kd)  (sizeof(dtype)*DIMENSION(kd))

// compatibility macros (for DEPRECATED old-fashioned trees)
#define COMPAT_NODE_SIZE(kd)    (sizeof(kdtree_node_t) + (SIZEOF_PT(kd) * 2))
#define COMPAT_HIGH_HR(kd, i)  ((ttype*)(((char*)(kd)->nodes) \
										  + COMPAT_NODE_SIZE(kd)*(i) \
										  + sizeof(kdtree_node_t) \
										  + SIZEOF_PT(kd)))
#define COMPAT_LOW_HR(kd, i)   ((ttype*) (((char*)(kd)->nodes) \
										   + COMPAT_NODE_SIZE(kd)*(i) \
										   + sizeof(kdtree_node_t)))

// "converted" <-> "dtype" conversion.
#define POINT_CTOR(kd, d, val)  (((val) - ((kd)->minval[(d)])) * (kd)->scale)
#define POINT_RTOC(kd, d, val)  (((val) * ((kd)->invscale)) + (kd)->minval[d])

#define DIST_CTOR(kd, dist)       ((dist) * (kd)->scale)
#define DIST2_CTOR(kd, dist2)     ((dist2) * (kd)->scale * (kd)->scale)
#define DIST2_RTOC(kd, dist2)     ((dist2) * (kd)->invscale * (kd)->invscale)


// this must be at least as big as the biggest integer TTYPE.
typedef u32 bigint;

#define ACTUAL_STRINGIFY(x) (#x)
#define STRINGIFY(x) ACTUAL_STRINGIFY(x)

#define MYGLUE2(a, b) a ## b
#define DIST_FUNC_MANGLE(x, suffix) MYGLUE2(x, suffix)

/* min/maxdist functions. */
#define CAN_OVERFLOW 0
#undef  DELTAMAX

#define PTYPE etype
#define DISTTYPE double
#define FUNC_SUFFIX 
#include "kdtree_internal_dists.c"
#undef PTYPE
#undef DISTTYPE
#undef FUNC_SUFFIX

#undef CAN_OVERFLOW
#define CAN_OVERFLOW 1

#define PTYPE ttype
#define DISTTYPE ttype
#define FUNC_SUFFIX _ttype
#define DELTAMAX TTYPE_SQRT_MAX
#include "kdtree_internal_dists.c"
#undef PTYPE
#undef DISTTYPE
#undef FUNC_SUFFIX
#undef DELTAMAX

#define PTYPE ttype
#define DISTTYPE bigttype
#define FUNC_SUFFIX _bigttype
#undef  DELTAMAX
#include "kdtree_internal_dists.c"
#undef PTYPE
#undef DISTTYPE
#undef FUNC_SUFFIX

#undef CAN_OVERFLOW

static bool bboxes(const kdtree_t* kd, int node,
				   ttype** p_tlo, ttype** p_thi, int D) {
	if (kd->bb.any) {
		// bb trees
		if (node >= kd->ninterior) {
			// leaf nodes don't have bboxes!
			return FALSE;
		}
		*p_tlo =  LOW_HR(kd, D, node);
		*p_thi = HIGH_HR(kd, D, node);
		return TRUE;
	} else if (kd->nodes) {
		// compat mode
		*p_tlo = COMPAT_LOW_HR (kd, node);
		*p_thi = COMPAT_HIGH_HR(kd, node);
		return TRUE;
	} else {
		return FALSE;
	}
}

static inline double dist2(const kdtree_t* kd, const etype* q, const dtype* p, int D) {
	int d;
	double d2 = 0.0;
#if defined(KD_DIM)
	D = KD_DIM;
#endif
	for (d=0; d<D; d++) {
		etype pp = POINT_DE(kd, d, p[d]);
		double delta;
		if (TTYPE_INTEGER) {
			if (q[d] > pp)
				delta = q[d] - pp;
			else
				delta = pp - q[d];
		} else {
			delta = q[d] - pp;
		}
		d2 += delta * delta;
	}
	return d2;
}

static inline void dist2_bailout(const kdtree_t* kd, const etype* q, const dtype* p,
                                 int D, double maxd2, bool* bailedout, double* d2res) {
	int d;
	double d2 = 0.0;
#if defined(KD_DIM)
	D = KD_DIM;
#endif
	for (d=0; d<D; d++) {
		double delta;
		etype pp = POINT_DE(kd, d, p[d]);
		if (TTYPE_INTEGER) {
			if (q[d] > pp)
				delta = q[d] - pp;
			else
				delta = pp - q[d];
		} else {
			delta = q[d]  - pp;
		}
		d2 += delta * delta;
		if (d2 > maxd2) {
			*bailedout = TRUE;
			return;
		}
	}
	*d2res = d2;
}

static inline bool dist2_exceeds(const kdtree_t* kd, const etype* q, const dtype* p, int D, double maxd2) {
	int d;
	double d2 = 0.0;
#if defined(KD_DIM)
	D = KD_DIM;
#endif
	for (d=0; d<D; d++) {
		double delta;
		double pp = POINT_DE(kd, d, p[d]);
		if (TTYPE_INTEGER) {
			if (q[d] > pp)
				delta = q[d] - pp;
			else
				delta = pp - q[d];
		} else {
			delta = q[d] - pp;
		}
		d2 += delta * delta;
		if (d2 > maxd2)
			return 1;
	}
	return 0;
}

static bool bb_point_l1mindist_exceeds_ttype(ttype* lo, ttype* hi,
											  ttype* query, int D,
											  ttype maxl1, ttype maxlinf) {
	ttype dist = 0;
	ttype newdist;
	ttype delta;
	int d;
#if defined(KD_DIM)
	D = KD_DIM;
#endif
	for (d=0; d<D; d++) {
		if (query[d] < lo[d])
			delta = lo[d] - query[d];
		else if (query[d] > hi[d])
			delta = query[d] - hi[d];
		else
			continue;
		if (delta > maxlinf) {
			//printf("linf: %u > %u.\n", (unsigned int)delta, (unsigned int)maxlinf);
			return TRUE;
		}
		newdist = dist + delta;
		if (newdist < dist) {
			// overflow!
			return TRUE;
		}
		if (newdist > maxl1) {
			//printf("l1: %u > %u\n", (unsigned int)newdist, (unsigned int)maxl1);
			return TRUE;
		}
		dist = newdist;
	}
	return FALSE;
}

static void compute_splitbits(kdtree_t* kd) {
	int D;
	int bits;
	u32 val;
	D = kd->ndim;
	bits = 0;
	val = 1;
	while (val < D) {
		bits++;
		val *= 2;
	}
	kd->dimmask = val - 1;
	kd->dimbits = bits;
	kd->splitmask = ~kd->dimmask;
}

/* Sorts results by kq->sdists */
static int kdtree_qsort_results(kdtree_qres_t *kq, int D) {
	int beg[KDTREE_MAX_RESULTS], end[KDTREE_MAX_RESULTS], i = 0, j, L, R;
	static dtype piv_vec[KDTREE_MAX_DIM];
	unsigned int piv_perm;
	dtype piv;

	beg[0] = 0;
	end[0] = kq->nres - 1;
	while (i >= 0) {
		L = beg[i];
		R = end[i];
		if (L < R) {
			piv = kq->sdists[L];
			for (j = 0;j < D;j++)
				piv_vec[j] = kq->results.DTYPE[D * L + j];
			piv_perm = kq->inds[L];
			if (i == KDTREE_MAX_RESULTS - 1) /* Sanity */
				assert(0);
			while (L < R) {
				while (kq->sdists[R] >= piv && L < R)
					R--;
				if (L < R) {
					for (j = 0;j < D;j++)
						kq->results.DTYPE[D*L + j] = kq->results.DTYPE[D * R + j];
					kq->inds[L] = kq->inds[R];
					kq->sdists[L] = kq->sdists[R];
					L++;
				}
				while (kq->sdists[L] <= piv && L < R)
					L++;
				if (L < R) {
					for (j = 0;j < D;j++)
						kq->results.DTYPE[D*R + j] = kq->results.DTYPE[D * L + j];
					kq->inds[R] = kq->inds[L];
					kq->sdists[R] = kq->sdists[L];
					R--;
				}
			}
			for (j = 0;j < D;j++)
				kq->results.DTYPE[D*L + j] = piv_vec[j];
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
bool resize_results(kdtree_qres_t* res, int newsize, int D,
					bool do_dists, bool do_points) {
	if (do_dists)
		res->sdists  = REALLOC(res->sdists , newsize * sizeof(double));
	if (do_points)
		res->results.any = REALLOC(res->results.any, newsize * D * sizeof(etype));
	res->inds = REALLOC(res->inds, newsize * sizeof(u32));
	if (newsize && (!res->results.any || (do_dists && !res->sdists) || !res->inds))
		fprintf(stderr, "Failed to resize kdtree results arrays.\n");
	res->capacity = newsize;
	return TRUE;
}

static
bool add_result(const kdtree_t* kd, kdtree_qres_t* res, double sdist, unsigned int ind, const dtype* pt,
				int D, bool do_dists, bool do_points) {
	if (do_dists)
		res->sdists[res->nres] = sdist;
	res->inds  [res->nres] = ind;
	if (do_points) {
		int d;
		for (d=0; d<D; d++)
			res->results.ETYPE[res->nres * D + d] = POINT_DE(kd, d, pt[d]);
	}
	res->nres++;
	if (res->nres == res->capacity) {
		// enlarge arrays.
		return resize_results(res, res->capacity * 2, D, do_dists, do_points);
	}
	return TRUE;
}

/*
  Can the query be represented as a ttype?

 If so, place the converted value in "tquery".
*/
static bool ttype_query(const kdtree_t* kd, const etype* query, ttype* tquery) {
   etype val;
	int d, D=kd->ndim;
	for (d=0; d<D; d++) {
		val = POINT_ET(kd, d, query[d], );
		if (val < TTYPE_MIN || val > TTYPE_MAX)
			return FALSE;
		tquery[d] = (ttype)val;
	}
	return TRUE;
}

double MANGLE(kdtree_get_splitval)(const kdtree_t* kd, int nodeid) {
    int dim;
    ttype split = *KD_SPLIT(kd, nodeid);
    if (EQUAL_ET) {
        return split;
    }
    if (!kd->splitdim && TTYPE_INTEGER) {
        bigint tmpsplit = split;
        dim = tmpsplit & kd->dimmask;
        return POINT_TE(kd, dim, tmpsplit & kd->splitmask);
    } else {
        dim = kd->splitdim[nodeid];
    }
    return POINT_TE(kd, dim, split);
}


void MANGLE(kdtree_nn_bb)(const kdtree_t* kd, const etype* query,
                          double* p_bestd2, int* p_ibest) {
	int nodestack[100];
	double dist2stack[100];
	int stackpos = 0;
	int D = (kd ? kd->ndim : 0);
	bool use_tquery = FALSE;
	bool use_tmath = FALSE;
	bool use_bigtmath = FALSE;
	ttype tquery[D];
	double bestd2 = *p_bestd2;
	int ibest = *p_ibest;
	ttype tl2 = 0;
	bigttype bigtl2 = 0;

#if defined(KD_DIM)
	assert(kd->ndim == KD_DIM);
	D = KD_DIM;
#else
	D = kd->ndim;
#endif

	if (TTYPE_INTEGER) {
		use_tquery = ttype_query(kd, query, tquery);
	}
	if (TTYPE_INTEGER && use_tquery) {
        double dtl2 = DIST2_ET(kd, bestd2, );
		if (dtl2 < TTYPE_MAX) {
			use_tmath = TRUE;
		} else if (dtl2 < BIGTTYPE_MAX) {
			use_bigtmath = TRUE;
		}
		bigtl2 = ceil(dtl2);
		tl2    = bigtl2;
	}

	// queue root.
	nodestack[0] = 0;
	dist2stack[0] = 0.0;
    if (kd->fun->nn_enqueue)
        kd->fun->nn_enqueue(kd, 0, 1);

	while (stackpos >= 0) {
		int nodeid;
		int i;
		int L, R;
		ttype *tlo=NULL, *thi=NULL;
        int child;
        double childd2[2];
        double firstd2, secondd2;
        int firstid, secondid;
        
		if (dist2stack[stackpos] > bestd2) {
            // pruned!
            if (kd->fun->nn_prune)
                kd->fun->nn_prune(kd, nodestack[stackpos], dist2stack[stackpos], bestd2, 1);
			stackpos--;
			continue;
		}
		nodeid = nodestack[stackpos];
		stackpos--;
        if (kd->fun->nn_explore)
            kd->fun->nn_explore(kd, nodeid, dist2stack[stackpos+1], bestd2);

		if (KD_IS_LEAF(kd, nodeid) || KD_IS_LEAF(kd, KD_CHILD_LEFT(nodeid))) {
			dtype* data;
			L = kdtree_left(kd, nodeid);
			R = kdtree_right(kd, nodeid);
			for (i=L; i<=R; i++) {
				bool bailedout = FALSE;
				double dsqd;
                if (kd->fun->nn_point)
                    kd->fun->nn_point(kd, nodeid, i);
				data = KD_DATA(kd, D, i);
				dist2_bailout(kd, query, data, D, bestd2, &bailedout, &dsqd);
				if (bailedout)
					continue;
				// new best
				ibest = i;
				bestd2 = dsqd;
                if (kd->fun->nn_new_best)
                    kd->fun->nn_new_best(kd, nodeid, i, bestd2);
			}
			continue;
		}

        childd2[0] = childd2[1] = HUGE_VAL;
        for (child=0; child<2; child++) {
            bool bailed;
            double dist2;
            int childid = (child ? KD_CHILD_RIGHT(nodeid) : KD_CHILD_LEFT(nodeid));

            bboxes(kd, childid, &tlo, &thi, D);

            bailed = FALSE;
            if (TTYPE_INTEGER && use_tmath) {
                ttype newd2 = 0;
                bb_point_mindist2_bailout_ttype(tlo, thi, tquery, D, tl2, &bailed, &newd2);
                if (bailed) {
                    if (kd->fun->nn_prune)
                        kd->fun->nn_prune(kd, nodeid, newd2, bestd2, 2);
                    continue;
                }
                dist2 = DIST2_TE(kd, newd2);
            } else if (TTYPE_INTEGER && use_bigtmath) {
                bigttype newd2 = 0;
                bb_point_mindist2_bailout_bigttype(tlo, thi, tquery, D, bigtl2, &bailed, &newd2);
                if (bailed) {
                    if (kd->fun->nn_prune)
                        kd->fun->nn_prune(kd, nodeid, newd2, bestd2, 3);
                    continue;
                }
                dist2 = DIST2_TE(kd, newd2);
            } else {
                etype bblo, bbhi;
                int d;
                // this is just bb_point_mindist2_bailout...
                dist2 = 0.0;
                for (d=0; d<D; d++) { 
                    bblo = POINT_TE(kd, d, tlo[d]);
                    if (query[d] < bblo) {
                        dist2 += (bblo - query[d])*(bblo - query[d]);
                    } else {
                        bbhi = POINT_TE(kd, d, thi[d]);
                        if (query[d] > bbhi) {
                            dist2 += (query[d] - bbhi)*(query[d] - bbhi);
                        } else
                            continue;
                    }
                    if (dist2 > bestd2) {
                        bailed = TRUE;
                        break;
                    }
                }
				if (bailed) {
                    if (kd->fun->nn_prune)
                        kd->fun->nn_prune(kd, childid, dist2, bestd2, 4);
					continue;
                }
			}
            childd2[child] = dist2;
        }

        if (childd2[0] <= childd2[1]) {
            firstd2 = childd2[0];
            secondd2 = childd2[1];
            firstid = KD_CHILD_LEFT(nodeid);
            secondid = KD_CHILD_RIGHT(nodeid);
        } else {
            firstd2 = childd2[1];
            secondd2 = childd2[0];
            firstid = KD_CHILD_RIGHT(nodeid);
            secondid = KD_CHILD_LEFT(nodeid);
        }

        if (firstd2 == HUGE_VAL)
            continue;

        // it's a stack, so put the "second" one on first.
        if (secondd2 != HUGE_VAL) {
            stackpos++;
            nodestack[stackpos] = secondid;
            dist2stack[stackpos] = secondd2;
            if (kd->fun->nn_enqueue)
                kd->fun->nn_enqueue(kd, secondid, 2);
        }

        stackpos++;
        nodestack[stackpos] = firstid;
        dist2stack[stackpos] = firstd2;
        if (kd->fun->nn_enqueue)
            kd->fun->nn_enqueue(kd, firstid, 2);

	}
	*p_bestd2 = bestd2;
	*p_ibest = ibest;
}

void MANGLE(kdtree_nn)(const kdtree_t* kd, const etype* query,
					   double* p_bestd2, int* p_ibest) {
	int nodestack[100];
	double dist2stack[100];
	int stackpos = 0;
	int D = (kd ? kd->ndim : 0);
	bool use_tquery = FALSE;
	bool use_tmath = FALSE;
	bool use_bigtmath = FALSE;
	bool use_tsplit = FALSE;
	ttype tquery[D];
	double dtl1=0.0, dtl2=0.0, dtlinf=0.0;
	ttype tlinf = 0;
	ttype tl1 = 0;
	ttype tl2 = 0;
	bigttype bigtl2 = 0;

	double bestd2 = *p_bestd2;
	double bestdist;
	int ibest = *p_ibest;

    if (!kd->split.any) {
        MANGLE(kdtree_nn_bb)(kd, query, p_bestd2, p_ibest);
        return;
    }

#if defined(KD_DIM)
	assert(kd->ndim == KD_DIM);
	D = KD_DIM;
#else
	D = kd->ndim;
#endif

	bestdist = sqrt(bestd2);
	if (TTYPE_INTEGER) {
		use_tquery = ttype_query(kd, query, tquery);
	}

	if (TTYPE_INTEGER && use_tquery) {
		dtl1   = DIST_ET(kd, bestdist * sqrt(D),);
		dtl2   = DIST2_ET(kd, bestd2, );
		dtlinf = DIST_ET(kd, bestdist, );
		tl1    = ceil(dtl1);
		tlinf  = ceil(dtlinf);
		bigtl2 = ceil(dtl2);
		tl2    = bigtl2;
	}
	use_tsplit = use_tquery && (dtlinf < TTYPE_MAX);
	if (TTYPE_INTEGER && use_tquery && (kd->bb.any || kd->nodes)) {
		if (dtl2 < TTYPE_MAX) {
			use_tmath = TRUE;
			/*
			  printf("Using %s integer math.\n", STRINGIFY(TTYPE));
			  printf("(tl2 = %u).\n", (unsigned int)tl2);
			*/
		} else if (dtl2 < BIGTTYPE_MAX) {
			use_bigtmath = TRUE;
			/*
			  printf("Using %s/%s integer math.\n", STRINGIFY(TTYPE), STRINGIFY(BIGTTYPE));
			  printf("(bigtl2 = %llu).\n", (long long unsigned int)bigtl2);
			*/
		} else {
			/*
			  printf("L2 maxdist overflows u16 and u32 representation; not using int math.  %g -> %g > %u\n",
			  bestd2, dtl2, UINT32_MAX);
			*/
		}
	}

	// queue root.
	nodestack[0] = 0;
	dist2stack[0] = 0.0;
    if (kd->fun->nn_enqueue)
        kd->fun->nn_enqueue(kd, 0, 1);

	while (stackpos >= 0) {
		int nodeid;
		int i;
		int dim = -1;
		int L, R;
		ttype split = 0;
        double del;

		if (dist2stack[stackpos] > bestd2) {
            // pruned!
            if (kd->fun->nn_prune)
                kd->fun->nn_prune(kd, nodestack[stackpos], dist2stack[stackpos], bestd2, 1);
			stackpos--;
			continue;
		}
		nodeid = nodestack[stackpos];
		stackpos--;

        if (kd->fun->nn_explore)
            kd->fun->nn_explore(kd, nodeid, dist2stack[stackpos+1], bestd2);

		if (KD_IS_LEAF(kd, nodeid)) {
			dtype* data;
			int oldbest = ibest;
			L = kdtree_left(kd, nodeid);
			R = kdtree_right(kd, nodeid);
			for (i=L; i<=R; i++) {
				bool bailedout = FALSE;
				double dsqd;

                if (kd->fun->nn_point)
                    kd->fun->nn_point(kd, nodeid, i);

				data = KD_DATA(kd, D, i);
				dist2_bailout(kd, query, data, D, bestd2, &bailedout, &dsqd);
				if (bailedout)
					continue;
				// new best
				ibest = i;
				bestd2 = dsqd;

                if (kd->fun->nn_new_best)
                    kd->fun->nn_new_best(kd, nodeid, i, bestd2);
			}
			if (oldbest != ibest) {
				bestdist = sqrt(bestd2);
				if (TTYPE_INTEGER && (use_tmath || use_bigtmath || use_tsplit)) {
					tl1    = DIST_ET(kd, bestdist * sqrt(D), ceil);
					tlinf  = DIST_ET(kd, bestdist, ceil);
					bigtl2 = DIST2_ET(kd, bestd2, ceil);
					tl2    = bigtl2;
				}
			}
			continue;
		}

		if (kd->splitdim)
			dim = kd->splitdim[nodeid];

        // split/dim trees
        split = *KD_SPLIT(kd, nodeid);
        if (!kd->splitdim && TTYPE_INTEGER) {
            bigint tmpsplit;
            tmpsplit = split;
            dim = tmpsplit & kd->dimmask;
            split = tmpsplit & kd->splitmask;
        }

        // use splitting plane.
        if (TTYPE_INTEGER && use_tsplit) {
            if (tquery[dim] < split) {
                // query is on the "left" side of the split.
                assert(query[dim] < POINT_TE(kd, dim, split));
                // is the right child within range?
                // look mum, no int overflow!
                if (split - tquery[dim] <= tlinf) {
                    // right child is okay.
                    assert(POINT_TE(kd, dim, split) - query[dim] > 0.0);
                    assert(POINT_TE(kd, dim, split) - query[dim] <= bestdist);
                    stackpos++;
                    nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
                    del = DIST_TE(kd, split - tquery[dim]);
                    dist2stack[stackpos] = del*del;
                    if (kd->fun->nn_enqueue)
                        kd->fun->nn_enqueue(kd, KD_CHILD_RIGHT(nodeid), 4);
                } else {
                    if (kd->fun->nn_prune) {
                        del = DIST_TE(kd, split - tquery[dim]);
                        kd->fun->nn_prune(kd, KD_CHILD_RIGHT(nodeid), del*del, bestd2, 5);
                    }
                }
                stackpos++;
                nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
                dist2stack[stackpos] = 0.0;
                if (kd->fun->nn_enqueue)
                    kd->fun->nn_enqueue(kd, KD_CHILD_LEFT(nodeid), 5);

            } else {
                // query is on "right" side.
                assert(POINT_TE(kd, dim, split) <= query[dim]);
                // is the left child within range?
                if (tquery[dim] - split < tlinf) {
                    assert(query[dim] - POINT_TE(kd, dim, split) >= 0.0);
                    assert(query[dim] - POINT_TE(kd, dim, split) < bestdist);
                    stackpos++;
                    nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
                    del = DIST_TE(kd, tquery[dim] - split);
                    dist2stack[stackpos] = del*del;
                    if (kd->fun->nn_enqueue)
                        kd->fun->nn_enqueue(kd, KD_CHILD_LEFT(nodeid), 6);
                } else {
                    if (kd->fun->nn_prune) {
                        del = DIST_TE(kd, tquery[dim] - split);
                        kd->fun->nn_prune(kd, KD_CHILD_LEFT(nodeid), del*del, bestd2, 6);
                    }
                }
                stackpos++;
                nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
                dist2stack[stackpos] = 0.0;
                if (kd->fun->nn_enqueue)
                    kd->fun->nn_enqueue(kd, KD_CHILD_RIGHT(nodeid), 7);
            }
        } else {
            etype rsplit = POINT_TE(kd, dim, split);
            if (query[dim] < rsplit) {
                // query is on the "left" side of the split.
                // is the right child within range?
                if (rsplit - query[dim] <= bestdist) {
                    stackpos++;
                    nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
                    del = rsplit - query[dim];
                    dist2stack[stackpos] = del*del;
                    if (kd->fun->nn_enqueue)
                        kd->fun->nn_enqueue(kd, KD_CHILD_RIGHT(nodeid), 8);
                } else {
                    if (kd->fun->nn_prune) {
                        del = rsplit - query[dim];
                        kd->fun->nn_prune(kd, KD_CHILD_RIGHT(nodeid), del*del, bestd2, 7);
                    }
                }
                stackpos++;
                nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
                dist2stack[stackpos] = 0.0;
                if (kd->fun->nn_enqueue)
                    kd->fun->nn_enqueue(kd, KD_CHILD_LEFT(nodeid), 9);
            } else {
                // query is on the "right" side
                // is the left child within range?
                if (query[dim] - rsplit < bestdist) {
                    stackpos++;
                    nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
                    del = query[dim] - rsplit;
                    dist2stack[stackpos] = del*del;
                    if (kd->fun->nn_enqueue)
                        kd->fun->nn_enqueue(kd, KD_CHILD_LEFT(nodeid), 10);
                } else {
                    if (kd->fun->nn_prune) {
                        del = query[dim] - rsplit;
                        kd->fun->nn_prune(kd, KD_CHILD_LEFT(nodeid), del*del, bestd2, 8);
                    }
                }
                stackpos++;
                nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
                dist2stack[stackpos] = 0.0;
                if (kd->fun->nn_enqueue)
                    kd->fun->nn_enqueue(kd, KD_CHILD_RIGHT(nodeid), 11);
            }
        }
    }
	*p_bestd2 = bestd2;
	*p_ibest = ibest;
}

kdtree_qres_t* MANGLE(kdtree_rangesearch_options)
     (const kdtree_t* kd, kdtree_qres_t* res, const etype* query,
      double maxd2, int options)
{
	int nodestack[100];
	int stackpos = 0;
	int D = (kd ? kd->ndim : 0);
	bool do_dists;
	bool do_points = TRUE;
	bool do_wholenode_check;
	double maxdist = 0.0;
	ttype tlinf = 0;
	ttype tl1 = 0;
	ttype tl2 = 0;
	bigttype bigtl2 = 0;

	bool use_tquery = FALSE;
	bool use_tsplit = FALSE;
	bool use_tmath = FALSE;
	bool use_bigtmath = FALSE;

	bool do_precheck = FALSE;
	bool do_l1precheck = FALSE;

	double dtl1=0.0, dtl2=0.0, dtlinf=0.0;

	//dtype dquery[D];
	ttype tquery[D];

	if (!kd || !query)
		return NULL;
#if defined(KD_DIM)
	assert(kd->ndim == KD_DIM);
	D = KD_DIM;
#else
	D = kd->ndim;
#endif
	
	if (options & KD_OPTIONS_SORT_DISTS)
		// gotta compute 'em if ya wanna sort 'em!
		options |= KD_OPTIONS_COMPUTE_DISTS;
	do_dists = options & KD_OPTIONS_COMPUTE_DISTS;
	do_wholenode_check = !(options & KD_OPTIONS_SMALL_RADIUS);

	if ((options & KD_OPTIONS_SPLIT_PRECHECK) &&
		kd->bb.any && kd->splitdim) {
		do_precheck = TRUE;
	}

	if ((options & KD_OPTIONS_L1_PRECHECK) &&
		kd->bb.any) {
		do_l1precheck = TRUE;
	}

	maxdist = sqrt(maxd2);

	if (TTYPE_INTEGER &&
		(kd->split.any || do_precheck || do_l1precheck)) {
		use_tquery = ttype_query(kd, query, tquery);
	}

	if (TTYPE_INTEGER && use_tquery) {
		dtl1   = DIST_ET(kd, maxdist * sqrt(D),);
		dtl2   = DIST2_ET(kd, maxd2, );
		dtlinf = DIST_ET(kd, maxdist, );

		tl1    = ceil(dtl1);
		tlinf  = ceil(dtlinf);
		bigtl2 = ceil(dtl2);
		tl2    = bigtl2;
	}

	use_tsplit = use_tquery && (dtlinf < TTYPE_MAX);

	if (do_l1precheck)
		if (dtl1 > TTYPE_MAX) {
			//printf("L1 maxdist %g overflows ttype representation.  L1 precheck disabled.\n", dtl1);
			do_l1precheck = FALSE;
		}

	if (TTYPE_INTEGER && use_tquery && (kd->bb.any || kd->nodes)) {
		if (dtl2 < TTYPE_MAX) {
			use_tmath = TRUE;
			/*
			  printf("Using %s integer math.\n", STRINGIFY(TTYPE));
			  printf("(tl2 = %u).\n", (unsigned int)tl2);
			*/
		} else if (dtl2 < BIGTTYPE_MAX) {
			use_bigtmath = TRUE;
		} else {
			/*
			  printf("L2 maxdist overflows u16 and u32 representation; not using int math.  %g -> %g > %u\n",
			  maxd2, dtl2, UINT32_MAX);
			*/
		}
		if (use_bigtmath) {
			if (options & KD_OPTIONS_NO_BIG_INT_MATH)
				use_bigtmath = FALSE;
			else {
				/*
				  printf("Using %s/%s integer math.\n", STRINGIFY(TTYPE), STRINGIFY(BIGTTYPE));
				  printf("(bigtl2 = %llu).\n", (long long unsigned int)bigtl2);
				*/
			}
		}
	}


	if (res) {
		if (!res->capacity) {
			resize_results(res, KDTREE_MAX_RESULTS, D, do_dists, do_points);
		} else {
			// call the resize routine just in case the old result struct was
			// from a tree of different type or dimensionality.
			resize_results(res, res->capacity, D, do_dists, do_points);
		}
		res->nres = 0;
	} else {
		res = CALLOC(1, sizeof(kdtree_qres_t));
		if (!res) {
			fprintf(stderr, "Failed to allocate kdtree_qres_t struct.\n");
			return NULL;
		}
		resize_results(res, KDTREE_MAX_RESULTS, D, do_dists, do_points);
	}


	// queue root.
	nodestack[0] = 0;

	while (stackpos >= 0) {
		int nodeid;
		int i;
		int dim = -1;
		int L, R;
		ttype split = 0;
		ttype *tlo=NULL, *thi=NULL;

		nodeid = nodestack[stackpos];
		stackpos--;

		if (KD_IS_LEAF(kd, nodeid)) {
			dtype* data;
			L = kdtree_left(kd, nodeid);
			R = kdtree_right(kd, nodeid);

			if (do_dists) {
				for (i=L; i<=R; i++) {
					bool bailedout = FALSE;
					double dsqd;
					data = KD_DATA(kd, D, i);
					// FIXME benchmark dist2 vs dist2_bailout.

					// HACK - should do "use_dtype", just like "use_ttype".
					dist2_bailout(kd, query, data, D, maxd2, &bailedout, &dsqd);
					if (bailedout)
						continue;
					if (!add_result(kd, res, dsqd, KD_PERM(kd, i), data, D, do_dists, do_points))
						return NULL;
				}
			} else {
				for (i=L; i<=R; i++) {
					data = KD_DATA(kd, D, i);
					// HACK - should do "use_dtype", just like "use_ttype".
					if (dist2_exceeds(kd, query, data, D, maxd2))
						continue;
					if (!add_result(kd, res, 0.0, KD_PERM(kd, i), data, D, do_dists, do_points))
						return NULL;
				}
			}
			continue;
		}

		bboxes(kd, nodeid, &tlo, &thi, D);

		if (kd->splitdim)
			dim = kd->splitdim[nodeid];
		if (kd->split.any) {
			// split/dim trees
			split = *KD_SPLIT(kd, nodeid);
			if (!kd->splitdim && TTYPE_INTEGER) {
				bigint tmpsplit;
				tmpsplit = split;
				dim = tmpsplit & kd->dimmask;
				split = tmpsplit & kd->splitmask;
			}
		}

		if ((tlo && thi) &&
			(!kd->split.any || !(options & KD_OPTIONS_USE_SPLIT))) {
			bool wholenode = FALSE;

			if (do_precheck && nodeid) {
				bool isleftchild = KD_IS_LEFT_CHILD(nodeid);
				// we need to use the dimension our _parent_ split on, not ours!
				int pdim;
				bool cut;
				if (kd->splitdim)
					pdim = kd->splitdim[KD_PARENT(nodeid)];
				else {
					pdim = kd->split.TTYPE[KD_PARENT(nodeid)];
					pdim &= kd->dimmask;
				}
				if (TTYPE_INTEGER && use_tquery) {
					if (isleftchild)
						cut = ((tquery[pdim] > thi[pdim]) &&
							   (tquery[pdim] - thi[pdim] > tlinf));
					else
						cut = ((tlo[pdim] > tquery[pdim]) &&
							   (tlo[pdim] - tquery[pdim] > tlinf));
				} else {
					etype bb;
					if (isleftchild) {
 						bb = POINT_TE(kd, pdim, thi[pdim]);
						cut = (query[pdim] - bb > maxdist);
					} else {
 						bb = POINT_TE(kd, pdim, tlo[pdim]);
						cut = (bb - query[pdim] > maxdist);
					}
				}
				if (cut) {
					// precheck failed!
					//printf("precheck failed!\n");
					continue;
				}
			}

			if (TTYPE_INTEGER && do_l1precheck && use_tquery)
				if (bb_point_l1mindist_exceeds_ttype(tlo, thi, tquery, D, tl1, tlinf)) {
					//printf("l1 precheck failed!\n");
					continue;
				}

			if (TTYPE_INTEGER && use_tmath) {
				if (bb_point_mindist2_exceeds_ttype(tlo, thi, tquery, D, tl2))
					continue;
				wholenode = do_wholenode_check &&
					!bb_point_maxdist2_exceeds_ttype(tlo, thi, tquery, D, tl2);
			} else if (TTYPE_INTEGER && use_bigtmath) {
				if (bb_point_mindist2_exceeds_bigttype(tlo, thi, tquery, D, bigtl2))
					continue;
				wholenode = do_wholenode_check &&
					!bb_point_maxdist2_exceeds_bigttype(tlo, thi, tquery, D, bigtl2);
			} else {
				etype bblo[D], bbhi[D];
				int d;
				for (d=0; d<D; d++) {
					bblo[d] = POINT_TE(kd, d, tlo[d]);
					bbhi[d] = POINT_TE(kd, d, thi[d]);
				}
				if (bb_point_mindist2_exceeds(bblo, bbhi, query, D, maxd2))
					continue;
				wholenode = do_wholenode_check &&
					!bb_point_maxdist2_exceeds(bblo, bbhi, query, D, maxd2);
			}

			if (wholenode) {
				L = kdtree_left(kd, nodeid);
				R = kdtree_right(kd, nodeid);
				if (do_dists) {
					for (i=L; i<=R; i++) {
						double dsqd = dist2(kd, query, KD_DATA(kd, D, i), D);
						if (!add_result(kd, res, dsqd, KD_PERM(kd, i), KD_DATA(kd, D, i), D, do_dists, do_points))
							return NULL;
					}
				} else {
					for (i=L; i<=R; i++)
						if (!add_result(kd, res, 0.0, KD_PERM(kd, i), KD_DATA(kd, D, i), D, do_dists, do_points))
							return NULL;
				}
				continue;
			}

			stackpos++;
			nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
			stackpos++;
			nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);

		} else {

			if (TTYPE_INTEGER && use_tsplit) {

				// stolen from inttree.c
				if (tquery[dim] < split) {
					// query is on the "left" side of the split.
					assert(query[dim] < POINT_TE(kd, dim, split));
					stackpos++;
					nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
					// look mum, no int overflow!
					if (split - tquery[dim] <= tlinf) {
						// right child is okay.
						assert(POINT_TE(kd, dim, split) - query[dim] >= 0.0);
						assert(POINT_TE(kd, dim, split) - query[dim] <= maxdist);
						stackpos++;
						nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
					}

				} else {
					// query is on "right" side.
					assert(POINT_TE(kd, dim, split) <= query[dim]);
					stackpos++;
					nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
					if (tquery[dim] - split <= tlinf) {
						assert(query[dim] - POINT_TE(kd, dim, split) >= 0.0);
						assert(query[dim] - POINT_TE(kd, dim, split) <= maxdist);
						stackpos++;
						nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
					}
				}
			} else {
				dtype rsplit = POINT_TE(kd, dim, split);
				if (query[dim] < rsplit) {
					// query is on the "left" side of the split.
					stackpos++;
					nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
					if (rsplit - query[dim] <= maxdist) {
						stackpos++;
						nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
					}
				} else {
					// query is on the "right" side
					stackpos++;
					nodestack[stackpos] = KD_CHILD_RIGHT(nodeid);
					if (query[dim] - rsplit <= maxdist) {
						stackpos++;
						nodestack[stackpos] = KD_CHILD_LEFT(nodeid);
					}
				}
			}
		}
	}

	/* Resize result arrays. */
	if (!(options & KD_OPTIONS_NO_RESIZE_RESULTS))
		resize_results(res, res->nres, D, do_dists, do_points);

	/* Sort by ascending distance away from target point before returning */
	if (options & KD_OPTIONS_SORT_DISTS)
		kdtree_qsort_results(res, kd->ndim);

	return res;
}


static void* get_data(const kdtree_t* kd, int i) {
	return KD_DATA(kd, kd->ndim, i);
}

static void copy_data_double(const kdtree_t* kd, int start, int N,
							 double* dest) {
	UNUSED int i, j, d;
	int D;

	D = kd->ndim;
#if DTYPE_DOUBLE
	//#warning "Data type is double; just copying data."
	memcpy(dest, kd->data.DTYPE + start*D, N*D*sizeof(etype));
#elif (!DTYPE_INTEGER && !ETYPE_INTEGER)
	//#warning "Etype and Dtype are both reals; just casting values."
	for (i=0; i<(N * D); i++)
		dest[i] = kd->data.DTYPE[start*D + i];
#else
	//#warning "Using POINT_DE to convert data."
	j=0;
	for (i=0; i<N; i++)
		for (d=0; d<D; d++) {
			dest[j] = POINT_DE(kd, D, kd->data.DTYPE[(start + i)*D + d]);
			j++;
		}
#endif
}

static int nearest_neighbour_within(const kdtree_t* kd, const void *pt,
									double maxd2, double* p_bestd2) {
	double bestd2 = maxd2;
	int ibest = -1;
	MANGLE(kdtree_nn)(kd, pt, &bestd2, &ibest);
	if (p_bestd2 && (ibest != -1)) *p_bestd2 = bestd2;
	return ibest;
}

static int nearest_neighbour(const kdtree_t* kd, const void* pt,
							 double* p_mindist2) {
	return nearest_neighbour_within(kd, pt, HUGE_VAL, p_mindist2);
}

static kdtree_qres_t* rangesearch(const kdtree_t* kd, kdtree_qres_t* res,
								  const void* pt, double maxd2, int options) {
	return MANGLE(kdtree_rangesearch_options)(kd, res, (etype*)pt, maxd2, options);
}

void MANGLE(kdtree_update_funcs)(kdtree_t* kd) {
	memset(kd->fun, 0, sizeof(kdtree_funcs));
	kd->fun->get_data = get_data;
	kd->fun->copy_data_double = copy_data_double;
	kd->fun->nearest_neighbour = nearest_neighbour;
	kd->fun->nearest_neighbour_within = nearest_neighbour_within;
	kd->fun->rangesearch = rangesearch;
	//kd->fun->rangesearch = MANGLE(kdtree_rangesearch_options);
}

static dtype* kdqsort_arr;
static int kdqsort_D;

static int kdqsort_compare(const void* v1, const void* v2)
{
	int i1, i2;
	dtype val1, val2;
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

static int kdtree_qsort(dtype *arr, unsigned int *parr, int l, int r, int D, int d)
{
	int* permute;
	int i, j, N;
	dtype* tmparr;
	int* tmpparr;

	N = r - l + 1;
	permute = MALLOC(N * sizeof(int));
	if (!permute) {
		fprintf(stderr, "Failed to allocate extra permutation array.\n");
		return -1;
	}
	for (i = 0; i < N; i++)
		permute[i] = i;
	kdqsort_arr = arr + l * D + d;
	kdqsort_D = D;

	qsort(permute, N, sizeof(int), kdqsort_compare);

	// permute the data one dimension at a time...
	tmparr = MALLOC(N * sizeof(dtype));
	if (!tmparr) {
		fprintf(stderr, "Failed to allocate temp permutation array.\n");
		return -1;
	}
	for (j = 0; j < D; j++) {
		for (i = 0; i < N; i++) {
			int pi = permute[i];
			tmparr[i] = arr[(l + pi) * D + j];
		}
		for (i = 0; i < N; i++)
			arr[(l + i) * D + j] = tmparr[i];
	}
	FREE(tmparr);
	tmpparr = MALLOC(N * sizeof(int));
	if (!tmpparr) {
		fprintf(stderr, "Failed to allocate temp permutation array.\n");
		return -1;
	}
	for (i = 0; i < N; i++) {
		int pi = permute[i];
		tmpparr[i] = parr[l + pi];
	}
	memcpy(parr + l, tmpparr, N*sizeof(int));
	FREE(tmpparr);
	FREE(permute);
	return 0;
}


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
          memcpy(tmpdata,    arr+(il)*D, D*sizeof(dtype)); \
          memcpy(arr+(il)*D, arr+(ir)*D, D*sizeof(dtype)); \
          memcpy(arr+(ir)*D, tmpdata,    D*sizeof(dtype)); }}
#endif
#define ELEM_ROT(iA, iB, iC) { \
          tmpperm  = parr[iC]; \
          parr[iC] = parr[iB]; \
          parr[iB] = parr[iA]; \
          parr[iA] = tmpperm;  \
          assert(tmpperm != -1); \
          memcpy(tmpdata,    arr+(iC)*D, D*sizeof(dtype)); \
          memcpy(arr+(iC)*D, arr+(iB)*D, D*sizeof(dtype)); \
          memcpy(arr+(iB)*D, arr+(iA)*D, D*sizeof(dtype)); \
          memcpy(arr+(iA)*D, tmpdata,    D*sizeof(dtype)); }

static int kdtree_quickselect_partition(dtype *arr, unsigned int *parr, int L, int R, int D, int d) {
	int i;
	int low, high;
	int median;

#if defined(KD_DIM)
	// tell the compiler this is a constant...
	D = KD_DIM;
#endif

	/* sanity is good */
	assert(R >= L);

	/* Find the median and partition the data */
	low = L;
	high = R;
	median = (low + high + 1) / 2;
	while(1) {
		dtype vals[3];
		dtype tmp;
		dtype pivot;
		int i,j;
		int iless, iequal, igreater;
		int endless, endequal, endgreater;
		int middle;
		int nless, nequal;
		// temp storage for ELEM_SWAP and ELEM_ROT macros.
		dtype tmpdata[D];
		int tmpperm;

		if (high == low)
			break;

		/* Choose the pivot: find the median of the values in low, middle, and high
		   positions. */
		middle = (low + high) / 2;
		vals[0] = GET(low);
		vals[1] = GET(middle);
		vals[2] = GET(high);
		/* (Bubblesort the three elements.) */
		for (i=0; i<2; i++)
			for (j=0; j<(2-i); j++)
				if (vals[j] > vals[j+1]) {
					tmp = vals[j];
					vals[j] = vals[j+1];
					vals[j+1] = tmp;
				}
		assert(vals[0] <= vals[1]);
		assert(vals[1] <= vals[2]);

		pivot = vals[1];

		/* Count the number of items that are less than, and equal to, the pivot. */
		nless = nequal = 0;
		for (i=low; i<=high; i++) {
			if (GET(i) < pivot)
				nless++;
			else if (GET(i) == pivot)
				nequal++;
		}

		/* These are the indices where the <, =, and > entries will start. */
		iless = low;
		iequal = low + nless;
		igreater = low + nless + nequal;

		/* These are the indices where they will end; ie the elements less than the
		   pivot will live in [iless, endless).  (But note that we'll be incrementing
		   "iequal" et al in the loop below.) */
		endless = iequal;
		endequal = igreater;
		endgreater = high+1;

		while (1) {
			/* Find an element in the "less" section that is out of place. */
			while ( (iless < endless) && (GET(iless) < pivot) )
				iless++;

			/* Find an element in the "equal" section that is out of place. */
			while ( (iequal < endequal) && (GET(iequal) == pivot) )
				iequal++;

			/* Find an element in the "greater" section that is out of place. */
			while ( (igreater < endgreater) && (GET(igreater) > pivot)  )
				igreater++;


			/* We're looking at three positions, and each one has three cases:
			   we're finished that segment, or the element we're looking at belongs in
			   one of the other two segments.  This yields 27 cases, but many of them
			   are ruled out because, eg, if the element at "iequal" belongs in the "less"
			   segment, then we can't be done the "less" segment.

			   It turns out there are only 6 cases to handle:

			   ---------------------------------------------
			   case   iless    iequal   igreater    action
			   ---------------------------------------------
			   1      D        D        D           done
			   2      G        ?        L           swap l,g
			   3      E        L        ?           swap l,e
			   4      ?        G        E           swap e,g
			   5      E        G        L           rotate A
			   6      G        L        E           rotate B
			   ---------------------------------------------

			   legend:
			   D: done
			   ?: don't care
			   L: (element < pivot)
			   E: (element == pivot)
			   G: (element > pivot)
			*/

			/* case 1: done? */
			if ((iless == endless) && (iequal == endequal) && (igreater == endgreater))
				break;

			/* case 2: swap l,g */
			if ((iless < endless) && (igreater < endgreater) &&
				(GET(iless) > pivot) && (GET(igreater) < pivot)) {
				ELEM_SWAP(iless, igreater);
				assert(GET(iless) < pivot);
				assert(GET(igreater) > pivot);
				continue;
			}

			/* cases 3,4,5,6 */
			assert(iequal < endequal);
			if (GET(iequal) > pivot) {
				/* cases 4,5: */
				assert(igreater < endgreater);
				if (GET(igreater) == pivot) {
					/* case 4: swap e,g */
					ELEM_SWAP(iequal, igreater);
					assert(GET(iequal) == pivot);
					assert(GET(igreater) > pivot);
				} else {
					/* case 5: rotate. */
					assert(GET(iless) == pivot);
					assert(GET(iequal) > pivot);
					assert(GET(igreater) < pivot);
					ELEM_ROT(iless, iequal, igreater);
					assert(GET(iless) < pivot);
					assert(GET(iequal) == pivot);
					assert(GET(igreater) > pivot);
				}
			} else {
				/* cases 3,6 */
				assert(GET(iequal) < pivot);
				assert(iless < endless);
				if (GET(iless) == pivot) {
					/* case 3: swap l,e */
					ELEM_SWAP(iless, iequal);
					assert(GET(iless) < pivot);
					assert(GET(iequal) == pivot);
				} else {
					/* case 6: rotate. */
					assert(GET(iless) > pivot);
					assert(GET(iequal) < pivot);
					assert(GET(igreater) == pivot);
					ELEM_ROT(igreater, iequal, iless);
					assert(GET(iless) < pivot);
					assert(GET(iequal) == pivot);
					assert(GET(igreater) > pivot);
				}
			}
		}

		/* Reset the indices of where the segments start. */
		iless = low;
		iequal = low + nless;
		igreater = low + nless + nequal;

		/* Assert that "<" values are in the "less" partition, "=" values are in the
		   "equal" partition, and ">" values are in the "greater" partition. */
		for (i=iless; i<iequal; i++)
			assert(GET(i) < pivot);
		for (i=iequal; i<igreater; i++)
			assert(GET(i) == pivot);
		for (i=igreater; i<=high; i++)
			assert(GET(i) > pivot);

		/* Is the median in the "<", "=", or ">" partition? */
		if (median < iequal)
			/* median is in the "<" partition.
			   low is unchanged.
			*/
			high = iequal - 1;
		else if (median < igreater) {
			/* the median is inside the "=" partition; we're done! */
			break;
		} else
			/* median is in the ">" partition.
			   high is unchanged. */
			low = igreater;
	}

	/* check that it worked. */
	for (i=L; i<median; i++)
		assert(GET(i) <= GET(median));
	for (i=median; i<=R; i++)
		assert(GET(i) >= GET(median));

	assert (L < median);
	assert (median <= R);

	return median;
}
#undef ELEM_SWAP
#undef ELEM_ROT
#undef GET



static int kdtree_check_node(const kdtree_t* kd, int nodeid) {
	int sum, i;
	int D = kd->ndim;
	int L, R;
	int d;

	L = kdtree_left (kd, nodeid);
	R = kdtree_right(kd, nodeid);

	assert(L < kd->ndata);
	assert(R < kd->ndata);
	assert(L >= 0);
	assert(R >= 0);
	assert(L <= R);
	if (L >= kd->ndata || R >= kd->ndata || L < 0 || R < 0 || L > R) {
		fprintf(stderr, "kdtree_check: L,R out of range.\n");
		return -1;
	}

	// if it's the root node, make sure that each value in the permutation
	// array is present exactly once.
	if (!nodeid && kd->perm) {
		unsigned char* counts = CALLOC(kd->ndata, 1);
		for (i=0; i<kd->ndata; i++)
			counts[kd->perm[i]]++;
		for (i=0; i<kd->ndata; i++)
			assert(counts[i] == 1);
		for (i=0; i<kd->ndata; i++)
			if (counts[i] != 1) {
				fprintf(stderr, "kdtree_check: permutation vector failure.\n");
				return -1;
			}
		FREE(counts);
	}

	sum = 0;
	if (kd->perm) {
		for (i=L; i<=R; i++) {
			sum += kd->perm[i];
			assert(kd->perm[i] >= 0);
			assert(kd->perm[i] < kd->ndata);
			if (kd->perm[i] < 0 || kd->perm[i] >= kd->ndata) {
				fprintf(stderr, "kdtree_check: permutation vector range failure.\n");
				return -1;
			}
		}
	}

	if (KD_IS_LEAF(kd, nodeid)) {
		return 0;

	}
	if (kd->bb.any) {
		ttype* bb;
		ttype *plo, *phi;
		bool ok = FALSE;
		plo = LOW_HR( kd, D, nodeid);
		phi = HIGH_HR(kd, D, nodeid);

		// bounding-box sanity.
		for (d=0; d<D; d++) {
			assert(plo[d] <= phi[d]);
			if (plo[d] > phi[d]) {
				fprintf(stderr, "kdtree_check: bounding-box sanity failure.\n");
				return -1;
			}
		}
		// check that the points owned by this node are within its bounding box.
		for (i=L; i<=R; i++) {
			dtype* dat = KD_DATA(kd, D, i);
			for (d=0; d<D; d++) {
				UNUSED ttype t = POINT_DT(kd, d, dat[d], KD_ROUND);
				assert(plo[d] <= t);
				assert(t <= phi[d]);
				if (plo[d] > t || t > phi[d]) {
					fprintf(stderr, "kdtree_check: bounding-box failure.\n");
					return -1;
				}
			}
		}

		if (!KD_IS_LEAF(kd, KD_CHILD_LEFT(nodeid))) {
			// check that the children's bounding box corners are within
			// the parent's bounding box.
			bb = LOW_HR(kd, D, KD_CHILD_LEFT(nodeid));
			for (d=0; d<D; d++) {
				assert(plo[d] <= bb[d]);
				assert(bb[d] <= phi[d]);
				if (plo[d] > bb[d] || bb[d] > phi[d]) {
					fprintf(stderr, "kdtree_check: bounding-box nesting failure.\n");
					return -1;
				}
			}
			bb = HIGH_HR(kd, D, KD_CHILD_LEFT(nodeid));
			for (d=0; d<D; d++) {
				assert(plo[d] <= bb[d]);
				assert(bb[d] <= phi[d]);
				if (plo[d] > bb[d] || bb[d] > phi[d]) {
					fprintf(stderr, "kdtree_check: bounding-box nesting failure.\n");
					return -1;
				}
			}
			bb = LOW_HR(kd, D, KD_CHILD_RIGHT(nodeid));
			for (d=0; d<D; d++) {
				assert(plo[d] <= bb[d]);
				assert(bb[d] <= phi[d]);
				if (plo[d] > bb[d] || bb[d] > phi[d]) {
					fprintf(stderr, "kdtree_check: bounding-box nesting failure.\n");
					return -1;
				}
			}
			bb = HIGH_HR(kd, D, KD_CHILD_RIGHT(nodeid));
			for (d=0; d<D; d++) {
				assert(plo[d] <= bb[d]);
				assert(bb[d] <= phi[d]);
				if (plo[d] > bb[d] || bb[d] > phi[d]) {
					fprintf(stderr, "kdtree_check: bounding-box nesting failure.\n");
					return -1;
				}
			}
			// check that the children don't overlap with positive volume.
			for (d=0; d<D; d++) {
				ttype* bb1 = HIGH_HR(kd, D, KD_CHILD_LEFT(nodeid));
				ttype* bb2 = LOW_HR(kd, D, KD_CHILD_RIGHT(nodeid));
				if (bb2[d] >= bb1[d]) {
					ok = TRUE;
					break;
				}
			}
			assert(ok);
			if (!ok) {
				fprintf(stderr, "kdtree_check: peer overlap failure.\n");
				return -1;
			}
		}
	}
	if (kd->split.any) {

		if (!KD_IS_LEAF(kd, KD_CHILD_LEFT(nodeid))) {
			// check split/dim.
			ttype split;
			int dim = 0;
			int cL, cR;
			dtype dsplit;

			split = *KD_SPLIT(kd, nodeid);
			if (kd->splitdim)
				dim = kd->splitdim[nodeid];
			else {
				if (TTYPE_INTEGER) {
					bigint tmpsplit;
					tmpsplit = split;
					dim = tmpsplit & kd->dimmask;
					split = tmpsplit & kd->splitmask;
				}
			}

			dsplit = POINT_TD(kd, dim, split);

			cL = kdtree_left (kd, KD_CHILD_LEFT(nodeid));
			cR = kdtree_right(kd, KD_CHILD_LEFT(nodeid));
			for (i=cL; i<=cR; i++) {
				UNUSED dtype* dat = KD_DATA(kd, D, i);
				assert(dat[dim] <= dsplit);
				if (dat[dim] > dsplit) {
					fprintf(stderr, "kdtree_check: split-plane failure.\n");
					return -1;
				}
			}

			cL = kdtree_left (kd, KD_CHILD_RIGHT(nodeid));
			cR = kdtree_right(kd, KD_CHILD_RIGHT(nodeid));
			for (i=cL; i<=cR; i++) {
				UNUSED dtype* dat = KD_DATA(kd, D, i);
				assert(dat[dim] >= dsplit);
				if (dat[dim] < dsplit) {
					fprintf(stderr, "kdtree_check: split-plane failure.\n");
					return -1;
				}
			}
		}
	}
	return 0;
}

int MANGLE(kdtree_check)(const kdtree_t* kd) {
	int i;
	for (i=0; i<kd->nnodes; i++) {
		if (kdtree_check_node(kd, i))
			return -1;
	}
	return 0;
}

static double maxrange(double* lo, double* hi, int D) {
	double range;
	int d;
	range = 0.0;
	for (d=0; d<D; d++)
		if (hi[d] - lo[d] > range)
			range = hi[d] - lo[d];
	if (range == 0.0)
		return 1.0;
	return range;
}

static double compute_scale(dtype* ddata, int N, int D,
							double* lo, double* hi) {
	double range;
	int i, d;
	for (d=0; d<D; d++) {
		lo[d] = DTYPE_MAX;
		hi[d] = DTYPE_MIN;
	}
	for (i=0; i<N; i++) {
		for (d=0; d<D; d++) {
			if (*ddata > hi[d]) hi[d] = *ddata;
			if (*ddata < lo[d]) lo[d] = *ddata;
			ddata++;
		}
	}
	range = maxrange(lo, hi, D);
	return (double)TTYPE_MAX / range;
}

// same as "compute_scale" but takes an "etype".
static double compute_scale_ext(etype* edata, int N, int D,
								double* lo, double* hi) {
	double range;
	int i, d;
	for (d=0; d<D; d++) {
		lo[d] = ETYPE_MAX;
		hi[d] = ETYPE_MIN;
	}
	for (i=0; i<N; i++) {
		for (d=0; d<D; d++) {
			if (*edata > hi[d]) hi[d] = *edata;
			if (*edata < lo[d]) lo[d] = *edata;
			edata++;
		}
	}
	range = maxrange(lo, hi, D);
	return (double)TTYPE_MAX / range;
}

kdtree_t* MANGLE(kdtree_convert_data)
	 (kdtree_t* kd, etype* edata, int N, int D, int Nleaf) {
	dtype* ddata;
	int i, d;
	if (!kd)
		kd = kdtree_new(N, D, Nleaf);

	if (!kd->minval && !kd->maxval) {
		kd->minval = MALLOC(D * sizeof(double));
		kd->maxval = MALLOC(D * sizeof(double));
		kd->scale = compute_scale_ext(edata, N, D, kd->minval, kd->maxval);
	} else {
		// limits were pre-set by the user.  just compute scale.
		double range;
		range = maxrange(kd->minval, kd->maxval, D);
		kd->scale = (double)DTYPE_MAX / range;
	}
	kd->invscale = 1.0 / kd->scale;

	ddata = kd->data.any = MALLOC(N * D * sizeof(dtype));
	for (i=0; i<N; i++) {
		for (d=0; d<D; d++) {
			//*ddata = POINT_ED(kd, d, *edata, KD_ROUND);
			// Bizarrely, the above causes conversion errors,
			// while the below does not...
			etype dd = POINT_ED(kd, d, *edata, KD_ROUND);
			// FIXME - what to do here - clamp?
			if (dd > DTYPE_MAX) {
				fprintf(stderr, "Clamping value %.12g -> %.12g to %u.\n", (double)*edata, (double)dd, (unsigned int)DTYPE_MAX);
				dd = DTYPE_MAX;
			}
			if (dd < DTYPE_MIN) {
				fprintf(stderr, "Clamping value %.12g -> %.12g to %u.\n", (double)*edata, (double)dd, (unsigned int)DTYPE_MIN);
				dd = DTYPE_MIN;
			}
			*ddata = (dtype)dd;

			// DEBUG: check it.
			/*{
			  etype ee = POINT_DE(kd, d, *ddata);
			  if (DIST_ED(kd, fabs(ee - *edata), ) > 1) {
			  double dd = POINT_ED(kd, d, *edata, );
			  double ddr = KD_ROUND(dd);
			  uint uddr = (uint)ddr;
			  fprintf(stderr, "Error: converting data: %g -> %u -> %g.\n",
			  *edata, (uint)*ddata, ee);
			  fprintf(stderr, "unrounded: %.10g.  rounded: %.10g.  uint: %u\n",
			  dd, KD_ROUND(dd), (uint)(KD_ROUND(dd)));
			  fprintf(stderr, " %.10g, %.10g, %u.\n",
			  dd, ddr, uddr);
			  }
			  }*/

			ddata++;
			edata++;
		}
	}
	return kd;
}


kdtree_t* MANGLE(kdtree_build)
	 (kdtree_t* kd, dtype* data, int N, int D, int Nleaf, unsigned int options) {
	dtype* rdata;
	int i;
	int xx;
	int lnext, level;
	int maxlevel;

	maxlevel = kdtree_compute_levels(N, Nleaf);

	assert(maxlevel > 0);
	assert(D <= KDTREE_MAX_DIM);
#if defined(KD_DIM)
	assert(D == KD_DIM);
	// let the compiler know that D is a constant...
	D = KD_DIM;
#endif

	/* Parameters checking */
	if (!data || !N || !D) {
        fprintf(stderr, "Data, N, or D is zero.\n");
		return NULL;
    }
	/* Make sure we have enough data */
	if ((1 << maxlevel) - 1 > N) {
        fprintf(stderr, "Too few data points for number of tree levels (%i < %i)!\n", N, (1 << maxlevel) - 1);
		return NULL;
    }

	if (!kd)
		kd = kdtree_new(N, D, Nleaf);

	if (!kd->data.any)
		kd->data.any = data;

	/* perm stores the permutation indexes. This gets shuffled around during
	 * sorts to keep track of the original index. */
	kd->perm = MALLOC(sizeof(u32) * N);
	for (i = 0;i < N;i++)
		kd->perm[i] = i;
	assert(kd->perm);

	kd->lr = MALLOC(kd->nbottom * sizeof(u32));
	assert(kd->lr);

	if (options & KD_BUILD_BBOX) {
		kd->bb.any = MALLOC(kd->ninterior * 2 * D * sizeof(ttype));
		assert(kd->bb.any);
	}
	if (options & KD_BUILD_SPLIT) {
		kd->split.any = MALLOC(kd->ninterior * sizeof(ttype));
		assert(kd->split.any);
	}
	if (((options & KD_BUILD_SPLIT) && !TTYPE_INTEGER) ||
		(options & KD_BUILD_SPLITDIM)) {
		kd->splitdim = MALLOC(kd->ninterior * sizeof(u8));
		kd->splitmask = UINT32_MAX;
	} else if (options & KD_BUILD_SPLIT)
		compute_splitbits(kd);

	if (TTYPE_INTEGER && !DTYPE_INTEGER) {
		// compute scaling params
		if (!kd->minval || !kd->maxval) {
			kd->minval = MALLOC(D * sizeof(double));
			kd->maxval = MALLOC(D * sizeof(double));
			kd->scale = compute_scale(kd->data.any, N, D, kd->minval, kd->maxval);
		} else {
			// limits were pre-set by the user.  just compute scale.
			double range;
			range = maxrange(kd->minval, kd->maxval, D);
			kd->scale = (double)TTYPE_MAX / range;
		}
		kd->invscale = 1.0 / kd->scale;
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
		dtype maxrange;
		ttype s;
		unsigned int c;
		int dim = 0;
		int m;
		dtype hi[D], lo[D];
		dtype qsplit = 0;
		unsigned int xx = 0;

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

		assert(right != (unsigned int)-1);

		/* More sanity */
		assert(0 <= left);
		assert(left <= right);
		assert(right < N);

		/* Find the bounding-box for this node. */
		for (d=0; d<D; d++) {
			hi[d] = DTYPE_MIN;
			lo[d] = DTYPE_MAX;
		}
		/* (since data is stored lexicographically we can just iterate through it) */
		/* (avoid doing kd->data[NODE(i)*D + d] many times; just ++ the pointer) */
		rdata = kd->data.DTYPE + left * D;
		for (j=left; j<=right; j++) {
			for (d=0; d<D; d++) {
				if (*rdata > hi[d]) hi[d] = *rdata;
				if (*rdata < lo[d]) lo[d] = *rdata;
				rdata++;
			}
		}
		if (options & KD_BUILD_BBOX) {
			for (d=0; d<D; d++) {
				(LOW_HR (kd, D, i))[d] = POINT_DT(kd, d, lo[d], floor);
				(HIGH_HR(kd, D, i))[d] = POINT_DT(kd, d, hi[d], ceil);
			}
		}

		/* Split along dimension with largest range */
		maxrange = DTYPE_MIN;
		for (d=0; d<D; d++)
			if ((hi[d] - lo[d]) >= maxrange) {
				maxrange = hi[d] - lo[d];
				dim = d;
			}
		d = dim;
		assert (d < D);

		if (TTYPE_INTEGER) {
			/* Sort the data. */

			/* Because the nature of the inttree is to bin the split
			 * planes, we have to be careful. Here, we MUST sort instead
			 * of merely partitioning, because we may not be able to
			 * properly represent the median as a split plane. Imagine the
			 * following on the dtype line: 
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
			if (kdtree_qsort(data, kd->perm, left, right, D, dim)) {
				fprintf(stderr, "kdtree_qsort failed.\n");
				// FIXME: memleak mania!
				return NULL;
			}
			//m = 1 + (left+right)/2;
			m = (1+left+right)/2;

			/* Make sure sort works */
			for(xx=left; xx<=right-1; xx++) { 
				assert(data[D*xx+d] <= data[D*(xx+1)+d]);
			}

			/* Encode split dimension and value. */
			/* "s" is the location of the splitting plane in the "tree"
			   data type. */
			s = POINT_DT(kd, d, data[D*m+d], KD_ROUND);

			if (kd->split.any) {
				/* If we are using the "split" array to store both the
				   splitting plane and the splitting dimension, then we
				   truncate a few bits from "s" here. */
				bigint tmps = s;
				tmps &= kd->splitmask;
				assert((tmps & kd->dimmask) == 0);
				s = tmps;
			}
			/* "qsplit" is the location of the splitting plane in the "data"
			   type. */
			qsplit = POINT_TD(kd, d, s);

			/* Play games to make sure we properly partition the data */
			while (m < right && data[D*m+d] < qsplit) m++;
			while (left < (m-1) && qsplit < data[D*(m-1)+d]) m--;

			/* Even more sanity */
			assert(m != 0);
			assert(left <= (m-1));
			assert(m <= right);
			for (xx=left; m && xx<=m-1; xx++)
				assert(data[D*xx+d] <= qsplit);
			for (xx=m; xx<=right; xx++)
				assert(qsplit <= data[D*xx+d]);

		} else {
			/* Pivot the data at the median */
			m = kdtree_quickselect_partition(data, kd->perm, left, right, D, dim);
			s = POINT_DT(kd, d, data[D*m+d], KD_ROUND);

			assert(m != 0);
			assert(left <= (m-1));
			assert(m <= right);
			for (xx=left; m && xx<=m-1; xx++)
				assert(data[D*xx+d] <= data[D*m+d]);
			for (xx=left; m && xx<=m-1; xx++)
				assert(data[D*xx+d] <= s);
			for (xx=m; xx<=right; xx++)
				assert(data[D*m+d] <= data[D*xx+d]);
			for (xx=m; xx<=right; xx++)
				assert(s <= data[D*xx+d]);
		}

		if (kd->split.any) {
			if (kd->splitdim)
				*KD_SPLIT(kd, i) = s;
			else {
				bigint tmps = s;
				*KD_SPLIT(kd, i) = tmps | dim;
			}
		}
		if (kd->splitdim)
			kd->splitdim[i] = dim;

		/* Store the R pointers for each child */
		c = 2*i;
		if (level == maxlevel - 2)
			c -= kd->ninterior;

		kd->lr[c+1] = m-1;
		kd->lr[c+2] = right;

        assert(c+2 < kd->nbottom);
	}

	for (xx=0; xx<kd->nbottom-1; xx++)
		assert(kd->lr[xx] <= kd->lr[xx+1]);

	/* do leaf nodes get bounding boxes?
     (nope, not at present).
     Why is that?
     */

	return kd;
}

bool MANGLE(kdtree_node_point_mindist2_exceeds)
	 (const kdtree_t* kd, int node, const etype* query, double maxd2) {
	int D = kd->ndim;
	int d;
	ttype* tlo, *thi;
	double d2 = 0.0;

	if (kd->nodes) {
		// compat mode
		tlo = COMPAT_LOW_HR (kd, node);
		thi = COMPAT_HIGH_HR(kd, node);
	} else if (kd->bb.any) {
		// bb trees
		tlo =  LOW_HR(kd, D, node);
		thi = HIGH_HR(kd, D, node);
	} else {
		fprintf(stderr, "Error: kdtree_node_point_mindist2_exceeds: kdtree does not have bounding boxes!\n");
		return FALSE;
	}
	for (d=0; d<D; d++) {
		etype delta;
	    etype lo = POINT_TE(kd, d, tlo[d]);
		//printf("d%i: lo %g, query %g\n", d, (double)lo, (double)query[d]);
		if (query[d] < lo)
			delta = lo - query[d];
		else {
			etype hi = POINT_TE(kd, d, thi[d]);
			//printf("   hi %g\n", (double)hi);
			if (query[d] > hi)
				delta = query[d] - hi;
			else
				continue;
		}
		d2 += delta * delta;
		//printf("  d2 %g\n", d2);
		if (d2 > maxd2)
			return TRUE;
	}
	return FALSE;
}

bool MANGLE(kdtree_node_point_maxdist2_exceeds)
	 (const kdtree_t* kd, int node, const etype* query, double maxd2) {
	int D = kd->ndim;
	int d;
	ttype* tlo=NULL, *thi=NULL;
	double d2 = 0.0;

	if (!bboxes(kd, node, &tlo, &thi, D)) {
		fprintf(stderr, "Error: kdtree_node_point_maxdist2_exceeds: kdtree does not have bounding boxes!\n");
		return FALSE;
	}
	for (d=0; d<D; d++) {
		etype delta1, delta2, delta;
		etype lo = POINT_TE(kd, d, tlo[d]);
		etype hi = POINT_TE(kd, d, thi[d]);
		if (query[d] < lo)
			delta = hi - query[d];
		else if (query[d] > hi)
			delta = query[d] - lo;
		else {
			delta1 = hi - query[d];
			delta2 = query[d] - lo;
			delta = (delta1 > delta2 ? delta1 : delta2);
		}
		d2 += delta*delta;
		if (d2 > maxd2)
			return TRUE;
	}
	return FALSE;
}

bool MANGLE(kdtree_node_node_maxdist2_exceeds)
	 (const kdtree_t* kd1, int node1,
	  const kdtree_t* kd2, int node2,
	  double maxd2) {
	ttype *tlo1=NULL, *tlo2=NULL, *thi1=NULL, *thi2=NULL;
	double d2 = 0.0;
	int d, D = kd1->ndim;

	//assert(kd1->treetype == kd2->treetype);
	assert(kd1->ndim == kd2->ndim);

	if (!bboxes(kd1, node1, &tlo1, &thi1, D)) {
		fprintf(stderr, "Error: kdtree_node_node_maxdist2_exceeds: kdtree does not have bounding boxes!\n");
		return FALSE;
	}

	if (!bboxes(kd2, node2, &tlo2, &thi2, D)) {
		fprintf(stderr, "Error: kdtree_node_node_maxdist2_exceeds: kdtree does not have bounding boxes!\n");
		return FALSE;
	}

	for (d=0; d<D; d++) {
		etype alo, ahi, blo, bhi;
		etype delta1, delta2, delta;
		alo = POINT_TE(kd1, d, tlo1[d]);
		ahi = POINT_TE(kd1, d, thi1[d]);
		blo = POINT_TE(kd2, d, tlo2[d]);
		bhi = POINT_TE(kd2, d, thi2[d]);
		// HACK - if etype is integer...
		if (ETYPE_INTEGER)
			fprintf(stderr, "HACK - int overflow is possible here.\n");
		delta1 = bhi - alo;
		delta2 = ahi - blo;
		delta = (delta1 > delta2 ? delta1 : delta2);
		d2 += delta*delta;
		if (d2 > maxd2)
			return TRUE;
	}
	return FALSE;
}

bool MANGLE(kdtree_node_node_mindist2_exceeds)
	 (const kdtree_t* kd1, int node1,
	  const kdtree_t* kd2, int node2,
	  double maxd2) {
	ttype *tlo1=NULL, *tlo2=NULL, *thi1=NULL, *thi2=NULL;
	double d2 = 0.0;
	int d, D = kd1->ndim;

	//assert(kd1->treetype == kd2->treetype);
	assert(kd1->ndim == kd2->ndim);

	if (!bboxes(kd1, node1, &tlo1, &thi1, D)) {
		fprintf(stderr, "Error: kdtree_node_node_mindist2_exceeds: kdtree does not have bounding boxes!\n");
		return FALSE;
	}

	if (!bboxes(kd2, node2, &tlo2, &thi2, D)) {
		fprintf(stderr, "Error: kdtree_node_node_mindist2_exceeds: kdtree does not have bounding boxes!\n");
		return FALSE;
	}

	for (d=0; d<D; d++) {
		etype alo, ahi, blo, bhi;
	    etype delta;
		ahi = POINT_TE(kd1, d, thi1[d]);
		blo = POINT_TE(kd2, d, tlo2[d]);
		if (ahi < blo)
			delta = blo - ahi;
		else {
			alo = POINT_TE(kd1, d, tlo1[d]);
			bhi = POINT_TE(kd2, d, thi2[d]);
			if (bhi < alo)
				delta = alo - bhi;
			else
				continue;
		}
		d2 += delta*delta;
		if (d2 > maxd2)
			return TRUE;
	}
	return FALSE;
}

static bool do_boxes_overlap(const ttype* lo1, const ttype* hi1,
							 const ttype* lo2, const ttype* hi2, int D) {
	int d;
	for (d=0; d<D; d++) {
		if (lo1[d] > hi2[d])
			return FALSE;
		if (lo2[d] > hi1[d])
			return FALSE;
	}
	return TRUE;
}

/* Is the first box contained within the second? */
static bool is_box_contained(const ttype* lo1, const ttype* hi1,
							 const ttype* lo2, const ttype* hi2, int D) {
	int d;
	for (d=0; d<D; d++) {
		if (lo1[d] < lo2[d])
			return FALSE;
		if (hi1[d] > hi2[d])
			return FALSE;
	}
	return TRUE;
}

static void nodes_contained_rec(const kdtree_t* kd,
								int nodeid,
								const ttype* qlo, const ttype* qhi,
								void (*cb_contained)(const kdtree_t* kd, int node, void* extra),
								void (*cb_overlap)(const kdtree_t* kd, int node, void* extra),
								void* cb_extra) {
	ttype *tlo=NULL, *thi=NULL;
	int D = kd->ndim;

	// leaf nodes don't have bounding boxes, so we have to do this check first!
	if (KD_IS_LEAF(kd, nodeid)) {
		cb_overlap(kd, nodeid, cb_extra);
		return;
	}

	if (!bboxes(kd, nodeid, &tlo, &thi, D)) {
		fprintf(stderr, "Error: kdtree_nodes_contained: node %i doesn't have a bounding box.\n", nodeid);
		return;
	}

	if (!do_boxes_overlap(tlo, thi, qlo, qhi, D))
		return;

	if (is_box_contained(tlo, thi, qlo, qhi, D)) {
		cb_contained(kd, nodeid, cb_extra);
		return;
	}

	nodes_contained_rec(kd,  KD_CHILD_LEFT(nodeid), qlo, qhi,
						cb_contained, cb_overlap, cb_extra);
	nodes_contained_rec(kd, KD_CHILD_RIGHT(nodeid), qlo, qhi,
						cb_contained, cb_overlap, cb_extra);
}

void MANGLE(kdtree_nodes_contained)
	 (const kdtree_t* kd,
	  const void* vquerylow, const void* vqueryhi,
	  void (*cb_contained)(const kdtree_t* kd, int node, void* extra),
	  void (*cb_overlap)(const kdtree_t* kd, int node, void* extra),
	  void* cb_extra) {
	int D = kd->ndim;
	int d;
	ttype qlo[D], qhi[D];
	const etype* querylow = vquerylow;
	const etype* queryhi = vqueryhi;

	for (d=0; d<D; d++) {
		double q;
		qlo[d] = q = POINT_ET(kd, d, querylow[d], floor);
		if (q < TTYPE_MIN) {
                    //fprintf(stderr, "Error: query value %g is below the minimum range of the tree %g.\n", q, (double)TTYPE_MIN);
                    qlo[d] = TTYPE_MIN;
		} else if (q > TTYPE_MAX) {
                    // query's low position is more than the tree's max: no overlap is possible.
                    return;
                }
		qhi[d] = q = POINT_ET(kd, d, queryhi [d], ceil );
		if (q > TTYPE_MAX) {
                    //fprintf(stderr, "Error: query value %g is above the maximum range of the tree %g.\n", q, (double)TTYPE_MAX);
                    qhi[d] = TTYPE_MAX;
		} else if (q < TTYPE_MIN) {
                    // query's high position is less than the tree's min: no overlap is possible.
                    return;
                }
	}

	nodes_contained_rec(kd, 0, qlo, qhi, cb_contained, cb_overlap, cb_extra);
}

bool MANGLE(kdtree_get_bboxes)(const kdtree_t* kd, int node,
							   void* vbblo, void* vbbhi) {
	etype* bblo = vbblo;
	etype* bbhi = vbbhi;
	ttype *tlo=NULL, *thi=NULL;
	int D = kd->ndim;
	int d;

	if (!bboxes(kd, node, &tlo, &thi, D))
		return FALSE;

	for (d=0; d<D; d++) {
		bblo[d] = POINT_TE(kd, d, tlo[d]);
		bbhi[d] = POINT_TE(kd, d, thi[d]);
	}
	return TRUE;
}
