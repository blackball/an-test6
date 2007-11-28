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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "kdtree.h"
#include "kdtree_internal.h"
#include "kdtree_internal_common.h"
#include "kdtree_mem.h"

KD_DECLARE(kdtree_update_funcs, void, (kdtree_t*));
KD_DECLARE(kdtree_get_splitval, double, (const kdtree_t*, int));

void kdtree_update_funcs(kdtree_t* kd) {
	KD_DISPATCH(kdtree_update_funcs, kd->treetype,, (kd));
}

int kdtree_level_start(const kdtree_t* kd, int level) {
    return (1 << level) - 1;
}

int kdtree_level_end(const kdtree_t* kd, int level) {
    return (1 << (level+1)) - 2;
}

static inline u8 node_level(const kdtree_t* kd, int nodeid) {
	int val = (nodeid + 1) >> 1;
	u8 level = 0;
	while (val) {
		val = val >> 1;
		level++;
	}
	return level;
}

int kdtree_get_level(const kdtree_t* kd, int nodeid) {
    return node_level(kd, nodeid);
}

int kdtree_get_splitdim(const kdtree_t* kd, int nodeid) {
    u32 tmpsplit;
    if (kd->splitdim)
        return kd->splitdim[nodeid];

    switch (kdtree_treetype(kd)) {
    case KDT_TREE_U32:
        tmpsplit = kd->split.u[nodeid];
        break;
    case KDT_TREE_U16:
        tmpsplit = kd->split.s[nodeid];
        break;
    default:
        return -1;
    }
    return tmpsplit & kd->dimmask;
}

double kdtree_get_splitval(const kdtree_t* kd, int nodeid) {
    double res;
	KD_DISPATCH(kdtree_get_splitval, kd->treetype, res=, (kd, nodeid));
    return res;
}

void* kdtree_get_data(const kdtree_t* kd, int i) {
	switch (kdtree_datatype(kd)) {
	case KDT_DATA_DOUBLE:
		return kd->data.d + kd->ndim * i;
	case KDT_DATA_FLOAT:
		return kd->data.f + kd->ndim * i;
	case KDT_DATA_U32:
		return kd->data.u + kd->ndim * i;
	case KDT_DATA_U16:
		return kd->data.s + kd->ndim * i;
	default:
		fprintf(stderr, "kdtree_get_data: invalid data type %i.\n", kdtree_datatype(kd));
		return NULL;
	}
}

void kdtree_copy_data_double(const kdtree_t* kd, int start, int N, double* dest) {
	int i;
	int d, D;
	D = kd->ndim;
	switch (kdtree_datatype(kd)) {
	case KDT_DATA_DOUBLE:
		memcpy(dest, kd->data.d + start*D,
			   N * D * sizeof(double));
		break;
	case KDT_DATA_FLOAT:
		for (i=0; i<(N * D); i++)
			dest[i] = kd->data.f[start*D + i];
		break;
	case KDT_DATA_U32:
		for (i=0; i<N; i++)
			for (d=0; d<D; d++)
				dest[i*D + d] = POINT_INVSCALE(kd, d, kd->data.u[(start + i)*D + d]);
		break;
	case KDT_DATA_U16:
		for (i=0; i<N; i++)
			for (d=0; d<D; d++)
				dest[i*D + d] = POINT_INVSCALE(kd, d, kd->data.s[(start + i)*D + d]);
		break;
	default:
		fprintf(stderr, "kdtree_copy_data_double: invalid data type %i.\n", kdtree_datatype(kd));
		return;
	}
}

void kdtree_inverse_permutation(const kdtree_t* tree, int* invperm) {
	int i;
	if (!tree->perm) {
		for (i=0; i<tree->ndata; i++)
			invperm[i] = i;
	} else {
		for (i=0; i<tree->ndata; i++)
			invperm[tree->perm[i]] = i;
	}
}

const char* kdtree_kdtype_to_string(int kdtype) {
	switch (kdtype) {
	case KDT_DATA_DOUBLE:
	case KDT_TREE_DOUBLE:
	case KDT_EXT_DOUBLE:
		return "double";
	case KDT_DATA_FLOAT:
	case KDT_TREE_FLOAT:
	case KDT_EXT_FLOAT:
		return "float";
	case KDT_DATA_U32:
	case KDT_TREE_U32:
		return "u32";
	case KDT_DATA_U16:
	case KDT_TREE_U16:
		return "u16";
	default:
		return NULL;
	}
}

int kdtree_kdtype_parse_data_string(const char* str) {
	if (!str) return KDT_DATA_NULL;
	if (!strcmp(str, "double")) {
		return KDT_DATA_DOUBLE;
	} else if (!strcmp(str, "float")) {
		return KDT_DATA_FLOAT;
	} else if (!strcmp(str, "u32")) {
		return KDT_DATA_U32;
	} else if (!strcmp(str, "u16")) {
		return KDT_DATA_U16;
	} else
		return KDT_DATA_NULL;
}

int kdtree_kdtype_parse_tree_string(const char* str) {
	if (!str) return KDT_TREE_NULL;
	if (!strcmp(str, "double")) {
		return KDT_TREE_DOUBLE;
	} else if (!strcmp(str, "float")) {
		return KDT_TREE_FLOAT;
	} else if (!strcmp(str, "u32")) {
		return KDT_TREE_U32;
	} else if (!strcmp(str, "u16")) {
		return KDT_TREE_U16;
	} else
		return KDT_TREE_NULL;
}

int kdtree_kdtype_parse_ext_string(const char* str) {
	if (!str) return KDT_EXT_NULL;
	if (!strcmp(str, "double")) {
		return KDT_EXT_DOUBLE;
	} else if (!strcmp(str, "float")) {
		return KDT_EXT_FLOAT;
	} else
		return KDT_EXT_NULL;
}

int kdtree_kdtypes_to_treetype(int exttype, int treetype, int datatype) {
	// HACK - asserts here...
	return exttype | treetype | datatype;
}

kdtree_t* kdtree_new(int N, int D, int Nleaf) {
	kdtree_t* kd;
	int maxlevel, nnodes;
	maxlevel = kdtree_compute_levels(N, Nleaf);
	kd = CALLOC(1, sizeof(kdtree_t));
	nnodes = (1 << maxlevel) - 1;
	kd->nlevels = maxlevel;
	kd->ndata = N;
	kd->ndim = D;
	kd->nnodes = nnodes;
	kd->nbottom = 1 << (maxlevel - 1);
	kd->ninterior = kd->nbottom - 1;
	assert(kd->nbottom + kd->ninterior == kd->nnodes);
	kd->fun = CALLOC(1, sizeof(kdtree_funcs));
	return kd;
}

void kdtree_set_limits(kdtree_t* kd, double* low, double* high) {
	int D = kd->ndim;
	if (!kd->minval) {
		kd->minval = MALLOC(D * sizeof(double));
	}
	if (!kd->maxval) {
		kd->maxval = MALLOC(D * sizeof(double));
	}
	memcpy(kd->minval, low,  D * sizeof(double));
	memcpy(kd->maxval, high, D * sizeof(double));
}

KD_DECLARE(kdtree_convert_data, kdtree_t*, (kdtree_t* kd, void* data, int N, int D, int Nleaf));

kdtree_t* kdtree_convert_data(kdtree_t* kd, void *data,
							  int N, int D, int Nleaf, int treetype) {
	kdtree_t* res = NULL;
	KD_DISPATCH(kdtree_convert_data, treetype, res=, (kd, data, N, D, Nleaf));
	if (res)
		res->converted_data = TRUE;
	return res;
}

int kdtree_compute_levels(int N, int Nleaf) {
	int nnodes = N / Nleaf;
	int maxlevel = 1;
	while (nnodes) {
		nnodes = nnodes >> 1;
		maxlevel++;
	}
	return maxlevel;
}

int kdtree_first_leaf(const kdtree_t* kd, int nodeid) {
	int dlevel, twodl, nodeid_twodl;
	dlevel = (kd->nlevels - 1) - node_level(kd, nodeid);
	twodl = (1 << dlevel);
	nodeid_twodl = (nodeid << dlevel);
	return (nodeid_twodl + twodl - 1) - kd->ninterior;
}

int kdtree_last_leaf(const kdtree_t* kd, int nodeid) {
	int dlevel, twodl, nodeid_twodl;
	dlevel = (kd->nlevels - 1) - node_level(kd, nodeid);
	twodl = (1 << dlevel);
	nodeid_twodl = (nodeid << dlevel);
	return (nodeid_twodl + (twodl - 1)*2) - kd->ninterior;
}

int kdtree_left(const kdtree_t* kd, int nodeid) {
	if (kd->nodes) {
		// assume old "real" = "double".
		kdtree_node_t* node = (kdtree_node_t*)
			(((unsigned char*)kd->nodes) +
			 nodeid * (kd->ndim * 2 * sizeof(double) + sizeof(kdtree_node_t)));
		return node->l;
	}
	if (KD_IS_LEAF(kd, nodeid)) {
		int ind = nodeid - kd->ninterior;
		if (!ind) return 0;
		return kd->lr[ind-1] + 1;
	} else {
		// leftmost child's L.
		int leftmost = kdtree_first_leaf(kd, nodeid);
		if (!leftmost) return 0;
		return kd->lr[leftmost-1] + 1;
	}
}

int kdtree_right(const kdtree_t* kd, int nodeid) {
	if (kd->nodes) {
		// assume old "real" = "double".
		kdtree_node_t* node = (kdtree_node_t*)
			(((unsigned char*)kd->nodes) +
			 nodeid * (kd->ndim * 2 * sizeof(double) + sizeof(kdtree_node_t)));
		return node->r;
	}
	if (KD_IS_LEAF(kd, nodeid)) {
		int ind = nodeid - kd->ninterior;
		return kd->lr[ind];
	} else {
		// rightmost child's R.
		int rightmost = kdtree_last_leaf(kd, nodeid);
		return kd->lr[rightmost];
	}
}

int kdtree_npoints(const kdtree_t* kd, int nodeid) {
	return 1 + kdtree_right(kd, nodeid) - kdtree_left(kd, nodeid);
}

void kdtree_free_query(kdtree_qres_t *kq)
{
	if (!kq) return;
	FREE(kq->results.any);
	FREE(kq->sdists);
	FREE(kq->inds);
	FREE(kq);
}

void kdtree_free(kdtree_t *kd)
{
	if (!kd) return;
	FREE(kd->nodes);
	FREE(kd->lr);
	FREE(kd->perm);
	FREE(kd->bb.any);
	FREE(kd->split.any);
	FREE(kd->splitdim);
	if (kd->converted_data)
		FREE(kd->data.any);
	FREE(kd->minval);
	FREE(kd->maxval);
	FREE(kd->fun);
	FREE(kd);
}

int kdtree_nearest_neighbour(const kdtree_t* kd, const void* pt, double* p_mindist2) {
	return kdtree_nearest_neighbour_within(kd, pt, HUGE_VAL, p_mindist2);
}

KD_DECLARE(kdtree_check, int, (const kdtree_t* kd));

int kdtree_check(const kdtree_t* kd) {
	int res = -1;
	KD_DISPATCH(kdtree_check, kd->treetype, res=, (kd));
	return res;
}

KD_DECLARE(kdtree_nn, void, (const kdtree_t* kd, const void* data, double* bestd2, int* pbest));

int kdtree_nearest_neighbour_within(const kdtree_t* kd, const void *pt, double maxd2,
									double* p_mindist2) {
	double bestd2 = maxd2;
	int ibest = -1;

	KD_DISPATCH(kdtree_nn, kd->treetype,, (kd, pt, &bestd2, &ibest));

	if (p_mindist2 && (ibest != -1))
		*p_mindist2 = bestd2;
	return ibest;
}

KD_DECLARE(kdtree_node_node_mindist2_exceeds, bool, (const kdtree_t* kd1, int node1, const kdtree_t* kd2, int node2, double maxd2));

bool kdtree_node_node_mindist2_exceeds(const kdtree_t* kd1, int node1,
									   const kdtree_t* kd2, int node2,
									   double dist2) {
	bool res = FALSE;
	KD_DISPATCH(kdtree_node_node_mindist2_exceeds, kd1->treetype, res=, (kd1, node1, kd2, node2, dist2));
	return res;
}

KD_DECLARE(kdtree_node_node_maxdist2_exceeds, bool, (const kdtree_t* kd1, int node1, const kdtree_t* kd2, int node2, double maxd2));

bool kdtree_node_node_maxdist2_exceeds(const kdtree_t* kd1, int node1,
									   const kdtree_t* kd2, int node2,
									   double dist2) {
	bool res = FALSE;
	KD_DISPATCH(kdtree_node_node_maxdist2_exceeds, kd1->treetype, res=, (kd1, node1, kd2, node2, dist2));
	return res;
}

KD_DECLARE(kdtree_node_point_mindist2_exceeds, bool, (const kdtree_t* kd, int node, const void* query, double maxd2));

bool kdtree_node_point_mindist2_exceeds(const kdtree_t* kd, int node, const void* pt,
										double dist2) {
	bool res = FALSE;
	KD_DISPATCH(kdtree_node_point_mindist2_exceeds, kd->treetype, res=, (kd, node, pt, dist2));
	return res;
}

KD_DECLARE(kdtree_node_point_maxdist2_exceeds, bool, (const kdtree_t* kd, int node, const void* query, double maxd2));

bool kdtree_node_point_maxdist2_exceeds(const kdtree_t* kd, int node, const void* pt,
										double dist2) {
	bool res = FALSE;
	KD_DISPATCH(kdtree_node_point_maxdist2_exceeds, kd->treetype, res=, (kd, node, pt, dist2));
	return res;
}

KD_DECLARE(kdtree_nodes_contained, void, (const kdtree_t* kd,
										  const void* querylow, const void* queryhi,
										  void (*callback_contained)(const kdtree_t* kd, int node, void* extra),
										  void (*callback_overlap)(const kdtree_t* kd, int node, void* extra),
										  void* cb_extra));

void kdtree_nodes_contained(const kdtree_t* kd,
							const void* querylow, const void* queryhi,
							void (*callback_contained)(const kdtree_t* kd, int node, void* extra),
							void (*callback_overlap)(const kdtree_t* kd, int node, void* extra),
							void* cb_extra) {
	KD_DISPATCH(kdtree_nodes_contained, kd->treetype,, (kd, querylow, queryhi, callback_contained, callback_overlap, cb_extra));
}

KD_DECLARE(kdtree_get_bboxes, bool, (const kdtree_t* kd, int node,
									 void* bblo, void* bbhi));

bool kdtree_get_bboxes(const kdtree_t* kd, int node, void* bblo, void* bbhi) {
	bool res = FALSE;
	KD_DISPATCH(kdtree_get_bboxes, kd->treetype, res=, (kd,node, bblo, bbhi));
	return res;
}






/***********************************************************************/
/* Commented-out...                                                    */
/***********************************************************************/

#if 0
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
