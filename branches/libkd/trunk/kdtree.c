#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "kdtree.h"
#include "kdtree_access.h"
#include "kdtree_macros.h"

/*
  typedef unsigned char bool;
  #define TRUE 1
  #define FALSE 0
*/

#if 0
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
#endif//if0

/*****************************************************************************/
/* Building routines                                                         */
/*****************************************************************************/

int kdtree_compute_levels(int N, int Nleaf) {
    int levels;
	assert(Nleaf);
	levels = (int)rint(log(N / (double)Nleaf) * M_LOG2E);
    if (levels < 1)
        levels = 1;
	return levels;
}

int kdtree_left(kdtree_t* kd, int nodeid) {
	if (ISLEAF(kd, nodeid)) {
		int ind = nodeid - kd->ninterior;
		if (!ind) return 0;
		return kd->lr[ind-1] + 1;
	} else {
		// leftmost child's L.
		int level;
		int val;
		int dlevel;
		int twodl;
		int ind;
		for (level=0, val=(nodeid+1)>>1; val; val=val>>1, level++);
		//printf("nodeid %i, level %i\n", nodeid, level);
		dlevel = (kd->nlevels - 1) - level;
		//printf("dlevel %i\n", dlevel);
		twodl = (1 << dlevel);
		//printf("leftmost child %i\n", twodl*nodeid + twodl - 1);
		ind = (twodl*nodeid + twodl - 1) - kd->ninterior;
		if (!ind) return 0;
		return kd->lr[ind-1] + 1;
	}
}

int kdtree_right(kdtree_t* kd, int nodeid) {
	if (ISLEAF(kd, nodeid)) {
		int ind = nodeid - kd->ninterior;
		return kd->lr[ind];
	} else {
		// rightmost child's R.
		int level;
		int val;
		int dlevel;
		int twodl;
		int ind;
		//for (level=-1, val=nodeid+1; val; val=val>>1, level++);
		for (level=0, val=(nodeid+1)>>1; val; val=val>>1, level++);
		//printf("nodeid %i, level %i\n", nodeid, level);
		dlevel = (kd->nlevels - 1) - level;
		//printf("dlevel %i\n", dlevel);
		twodl = (1 << dlevel);
		//printf("rightmost child %i\n", twodl*nodeid + (twodl - 1)*2);
		ind = (twodl*nodeid + (twodl - 1)*2) - kd->ninterior;
		return kd->lr[ind];
	}
}

/*
  int kdtree_left(kdtree_t* kd, int nodeid) {
  int val;
  if (!nodeid) return 0;
  // if all trailing bits are 1, return 0.
  for (val = nodeid; val & 1; val = val >> 1);
  if (!val) return 0;
  return kd->lr[nodeid-1]+1;
  }
  int kdtree_right(kdtree_t* kd, int nodeid) {
  return kd->lr[nodeid];
  }
*/

#if 0
real* kdqsort_arr;
int kdqsort_D;
#endif//if0

#if 0
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

/*****************************************************************************/
/* Querying routines                                                         */
/*****************************************************************************/

#if 0
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
#endif

#if 0
#endif//if0

#if 0
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
	}

	return
		kdtree_rangecount_rec(kd, kdtree_get_child1(kd, node), pt, maxd2) +
		kdtree_rangecount_rec(kd, kdtree_get_child2(kd, node), pt, maxd2);
}

int kdtree_rangecount(kdtree_t* kd, real* pt, real maxdistsquared) {
	return kdtree_rangecount_rec(kd, kdtree_get_root(kd), pt, maxdistsquared);
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
#endif

#if 0
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
#endif//if0

void kdtree_free_query(kdtree_qres_t *kq)
{
	assert(kq);
	/*
	  free(kq->results);
	  free(kq->sdists);
	*/
	free(kq->inds);
	free(kq);
}

void kdtree_free(kdtree_t *kd)
{
	assert(kd);
	free(kd->nodes);
	free(kd->lr);
	free(kd->perm);
	free(kd->bb.any);
	free(kd->split.any);
	free(kd->splitdim);
	if (kd->datacopy)
		free(kd->data.any);
	free(kd->minval);
	free(kd->maxval);
	free(kd->scale);
	free(kd);
}

/*****************************************************************************/
/* Output routines                                                           */
/*****************************************************************************/

#if 0
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
#endif
