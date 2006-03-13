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
