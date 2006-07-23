#ifndef _KDTREE_H
#define _KDTREE_H

#include <stdio.h>

#define KDTREE_MAX_LEVELS 1000
#define KDT_INFTY 1e309

/* this is a fixed-point intree where data values are constrained in some
 * range */

typedef double real;

struct intkdtree_node {
    unsigned int split; /* first two bits are dimension */
};
typedef struct intkdtree_node intkdtree_node_t;

struct intkdtree {
	intkdtree_node_t *tree;   /* Flat tree storing nodes and HR's */
	unsigned int *lr;         /* Stores left and right for bottom of tree */
	real *data;               /* Raw coordinate data as xyzxyzxyz */
	real *bbox;               /* Hyperrectangle containing all points */
	real delta;               /* The delta size for this tree */
	real minval;              /* Min val in any dim */
	real maxval;              /* Max val in any dim */
	unsigned int *perm;       /* Permutation index */
	unsigned int ndata;       /* Number of items */
	unsigned int ndim;        /* Number of dimensions */
	unsigned int nnodes;      /* Number of internal nodes */
	unsigned int nbottom;     /* Number of internal nodes */
	unsigned int ninterior;   /* Number of internal nodes */
	void* mmapped;            /* Next two are for mmap'd access */
	unsigned int mmapped_size;  
};
typedef struct intkdtree intkdtree_t;

struct intkdtree_qres {
	unsigned int nres;
	real *results;         /* Each af the points returned from a query */
	real *sdists;          /* Squared distance from query point */
	unsigned int *inds;    /* Indexes into original data set */
};
typedef struct intkdtree_qres intkdtree_qres_t;

/* Build a tree from an array of data, of size N*D*sizeof(real) */
intkdtree_t *intkdtree_build(real *data, int ndata, int ndim, int maxlevel, real minval, real maxval);

/* Range seach */
intkdtree_qres_t *kdtree_rangesearch(intkdtree_t *kd, real *pt, real maxdistsquared);

/* Free results */
void intkdtree_free_query(intkdtree_qres_t *kd);

/* Free a tree; does not free kd->data */
void intkdtree_free(intkdtree_t *kd);

/* Output Graphviz .dot format version of the tree */
void intkdtree_output_dot(FILE* fid, intkdtree_t* kd);

/* Sanity-check a tree. 0=okay. */
int intkdtree_check(intkdtree_t* t);

/* Internal methods */
int intkdtree_qsort_results(intkdtree_qres_t *kq, int D);
int intkdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d);

#endif /* _KDTREE_H */
