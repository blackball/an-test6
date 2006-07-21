#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "kdtree_access.h"
#include "kdtree_macros.h"

typedef unsigned char bool;
#define TRUE 1
#define FALSE 0

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

kdtree_node_t* kdtree_get_root(kdtree_t* kd) {
	return kd->tree;
}

real* kdtree_node_get_point(kdtree_t* tree, kdtree_node_t* node, int ind) {
	return tree->data + (node->l + ind) * tree->ndim;
}

int kdtree_node_get_index(kdtree_t* tree, kdtree_node_t* node, int ind) {
	if (!tree->perm)
		return node->l + ind;
	return tree->perm[(node->l + ind)];
}

real* kdtree_get_bb_low(kdtree_t* tree, kdtree_node_t* node) {
	return (real*)(node + 1);
}

real* kdtree_get_bb_high(kdtree_t* tree, kdtree_node_t* node) {
	return (real*)((char*)(node + 1) + sizeof(real) * tree->ndim);
}

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

int kdtree_node_to_nodeid(kdtree_t* kd, kdtree_node_t* node) {
	return ((char*)node - (char*)kd->tree) / NODE_SIZE;
}

kdtree_node_t* kdtree_nodeid_to_node(kdtree_t* kd, int nodeid) {
	return NODE(nodeid);
}

int kdtree_get_childid1(kdtree_t* kd, int nodeid) {
	assert(!ISLEAF(nodeid));
	return 2*nodeid + 1;
}

int kdtree_get_childid2(kdtree_t* kd, int nodeid) {
	assert(!ISLEAF(nodeid));
	return 2*nodeid + 2;
}

kdtree_node_t* kdtree_get_child1(kdtree_t* kd, kdtree_node_t* node) {
	int nodeid = kdtree_node_to_nodeid(kd, node);
	if (ISLEAF(nodeid))
		return NULL;
	return CHILD_NEG(nodeid);
}

kdtree_node_t* kdtree_get_child2(kdtree_t* kd, kdtree_node_t* node) {
	int nodeid = kdtree_node_to_nodeid(kd, node);
	if (ISLEAF(nodeid))
		return NULL;
	return CHILD_POS(nodeid);
}

int kdtree_node_is_leaf(kdtree_t* kd, kdtree_node_t* node) {
	int nodeid = kdtree_node_to_nodeid(kd, node);
	return ISLEAF(nodeid);
}

int kdtree_nodeid_is_leaf(kdtree_t* kd, int nodeid) {
	return ISLEAF(nodeid);
}

int kdtree_node_npoints(kdtree_node_t* node) {
	return 1 + node->r - node->l;
}

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

real kdtree_node_point_mindist2(kdtree_t* kd, kdtree_node_t* node, real* pt) {
	return kdtree_bb_point_mindist2(kdtree_get_bb_low(kd, node),
									kdtree_get_bb_high(kd, node),
									pt, kd->ndim);
}

int kdtree_node_point_mindist2_exceeds(kdtree_t* kd, kdtree_node_t* node,
									   real* pt, real maxd2) {
	return kdtree_bb_point_mindist2_exceeds
		(kdtree_get_bb_low(kd, node),
		 kdtree_get_bb_high(kd, node),
		 pt, kd->ndim, maxd2);
}

real kdtree_node_point_maxdist2(kdtree_t* kd, kdtree_node_t* node, real* pt) {
	return kdtree_bb_point_maxdist2(kdtree_get_bb_low(kd, node),
									kdtree_get_bb_high(kd, node),
									pt, kd->ndim);
}

int kdtree_node_point_maxdist2_exceeds(kdtree_t* kd, kdtree_node_t* node,
										real* pt, real maxd2) {
	return kdtree_bb_point_maxdist2_exceeds
		(kdtree_get_bb_low(kd, node),
		 kdtree_get_bb_high(kd, node),
		 pt, kd->ndim, maxd2);
}

