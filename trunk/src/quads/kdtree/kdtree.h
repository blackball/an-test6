#ifndef _KDTREE_H
#define _KDTREE_H

#include <stdio.h>

#define KDTREE_MAX_LEVELS 1000
#define KDT_INFTY 1e309

typedef double real;
struct kdtree_hr {
	real *low;            /* Lower hyperrectangle boundry */
	real *high;           /* Upper hyperrectangle boundry */
};
typedef struct kdtree_hr kdtree_hr_t;

struct kdtree_node {
    unsigned int dim;              /* Splitting dimension */
    unsigned int l,r;              /* data(l:r) are coordinates below this node */
	real pivot;           /* Pivot location */
	/* Implicit hyperrectangle 
	real HR[sizeof(real)*D] Lower
	real HR[sizeof(real)*D] Upper
	*/
};
typedef struct kdtree_node kdtree_node_t;

struct kdtree {
	kdtree_node_t *tree;   /* Flat tree storing nodes and HR's */
	real *data;            /* Raw coordinate data as xyzxyzxyz */
	unsigned int *perm;    /* Permutation index */
	unsigned int ndata;    /* Number of items */
	unsigned int ndim;     /* Number of dimensions */
	unsigned int nnodes;   /* Number of internal nodes */
	void* mmapped;         /* Next two are for mmap'd access */
	unsigned int mmapped_size;  
};
typedef struct kdtree kdtree_t;

struct kdtree_qres {
	unsigned int nres;
	real *results;         /* Each af the points returned from a query */
	real *sdists;          /* Squared distance from query point */
	unsigned int *inds;    /* Indexes into original data set */
};
typedef struct kdtree_qres kdtree_qres_t;

int kdtree_node_is_leaf(kdtree_t* tree, kdtree_node_t* node);

int kdtree_nodeid_is_leaf(kdtree_t* tree, int nodeid);

inline kdtree_node_t* kdtree_nodeid_to_node(kdtree_t* kd, int nodeid);

int kdtree_node_npoints(kdtree_node_t* node);

kdtree_node_t* kdtree_get_child1(kdtree_t* tree, kdtree_node_t* node);

kdtree_node_t* kdtree_get_child2(kdtree_t* tree, kdtree_node_t* node);

int kdtree_get_childid1(kdtree_t* tree, int nodeid);

int kdtree_get_childid2(kdtree_t* tree, int nodeid);

real kdtree_bb_mindist2(real* bblow1, real* bbhigh1,
						real* bblow2, real* bbhigh2, int dim);

real kdtree_bb_maxdist2(real* bblow1, real* bbhigh1,
						real* bblow2, real* bbhigh2, int dim);

real* kdtree_get_bb_low(kdtree_t* tree, kdtree_node_t* node);

real* kdtree_get_bb_high(kdtree_t* tree, kdtree_node_t* node);

real kdtree_node_node_mindist2(kdtree_t* tree1, kdtree_node_t* node1,
							   kdtree_t* tree2, kdtree_node_t* node2);

real kdtree_node_node_maxdist2(kdtree_t* tree1, kdtree_node_t* node1,
							   kdtree_t* tree2, kdtree_node_t* node2);

/* Build a tree from an array of data, of size N*D*sizeof(real) */
kdtree_t *kdtree_build(real *data, int ndata, int ndim, int maxlevel);

/* Range seach */
kdtree_qres_t *kdtree_rangesearch(kdtree_t *kd, real *pt, real maxdistsquared);

/* Optimize the KDTree by by constricting hyperrectangles to minimum volume */
void kdtree_optimize(kdtree_t *kd);

/* Free results */
void kdtree_free_query(kdtree_qres_t *kd);

/* Free a tree; does not free kd->data */
void kdtree_free(kdtree_t *kd);

/* Output Graphviz .dot format version of the tree */
void kdtree_output_dot(FILE* fid, kdtree_t* kd);

/* Internal methods */
int kdtree_qsort_results(kdtree_qres_t *kq, int D);
int kdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d);

#endif /* _KDTREE_H */
