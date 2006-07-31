#ifndef KDTREE_ACCESS_H
#define KDTREE_ACCESS_H

#include "kdtree.h"

/*
  Compute the inverse permutation of tree->perm and place it in "invperm".
 */
void kdtree_inverse_permutation(kdtree_t* tree, int* invperm);

int kdtree_is_point_in_rect(real* bblo, real* bbhi, real* point, int dim);

real kdtree_bb_mindist2(real* bblow1, real* bbhigh1,
						real* bblow2, real* bbhigh2, int dim);

real kdtree_bb_maxdist2(real* bblow1, real* bbhigh1,
						real* bblow2, real* bbhigh2, int dim);

real kdtree_bb_point_mindist2(real* bblow, real* bbhigh,
							  real* point, int dim);

real kdtree_bb_point_maxdist2(real* bblow, real* bbhigh,
							  real* point, int dim);

/***   Simple accessors   ***/

kdtree_node_t* kdtree_get_root(kdtree_t* kd);

int kdtree_node_npoints(kdtree_node_t* node);

/***   Nodeid accessors   ***/

int kdtree_nodeid_is_leaf(kdtree_t* tree, int nodeid);

kdtree_node_t* kdtree_nodeid_to_node(kdtree_t* kd, int nodeid);

int kdtree_get_childid1(kdtree_t* tree, int nodeid);

int kdtree_get_childid2(kdtree_t* tree, int nodeid);

/***   kdtree_node_t* accessors  ***/

real kdtree_node_volume(kdtree_t* tree, kdtree_node_t* node);

int kdtree_node_is_leaf(kdtree_t* tree, kdtree_node_t* node);

real* kdtree_node_get_point(kdtree_t* tree, kdtree_node_t* node, int ind);

int kdtree_node_get_index(kdtree_t* tree, kdtree_node_t* node, int ind);

kdtree_node_t* kdtree_get_child1(kdtree_t* tree, kdtree_node_t* node);

kdtree_node_t* kdtree_get_child2(kdtree_t* tree, kdtree_node_t* node);

real* kdtree_get_bb_low(kdtree_t* tree, kdtree_node_t* node);

real* kdtree_get_bb_high(kdtree_t* tree, kdtree_node_t* node);

real kdtree_node_node_mindist2(kdtree_t* tree1, kdtree_node_t* node1,
							   kdtree_t* tree2, kdtree_node_t* node2);

real kdtree_node_node_maxdist2(kdtree_t* tree1, kdtree_node_t* node1,
							   kdtree_t* tree2, kdtree_node_t* node2);

int kdtree_node_node_mindist2_exceeds(kdtree_t* tree1, kdtree_node_t* node1,
									  kdtree_t* tree2, kdtree_node_t* node2,
									  real maxd2);

real kdtree_node_node_maxdist2_exceeds(kdtree_t* tree1, kdtree_node_t* node1,
									   kdtree_t* tree2, kdtree_node_t* node2,
									   real maxd2);

real kdtree_node_point_mindist2(kdtree_t* kd, kdtree_node_t* node, real* pt);

real kdtree_node_point_maxdist2(kdtree_t* kd, kdtree_node_t* node, real* pt);

#define KD_ACCESS_GENERIC 1

#endif


#if defined(KD_ACCESS_GENERIC) || defined(KD_DIM)

int KDFUNC(kdtree_node_point_maxdist2_exceeds)
	 (kdtree_t* kd, kdtree_node_t* node,
	  real* pt, real maxd2);

int KDFUNC(kdtree_node_point_mindist2_exceeds)
	 (kdtree_t* kd, kdtree_node_t* node,
	  real* pt, real maxd2);

int KDFUNC(kdtree_bb_point_mindist2_exceeds)
	 (real* bblow, real* bbhigh,
	  real* point, int dim, real maxd2);

int KDFUNC(kdtree_bb_point_maxdist2_exceeds)
	 (real* bblow, real* bbhigh,
	  real* point, int dim, real maxd2);

#if !defined(KD_DIM)
#undef KD_ACCESS_GENERIC
#endif

#endif
