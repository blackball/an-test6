#ifndef KDTREE_H
#define KDTREE_H

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
    unsigned int l,r;              /* data(l:r) are coordinates below this node */
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
	real *results;         /* Each of the points returned from a query */
	real *sdists;          /* Squared distance from query point */
	unsigned int *inds;    /* Indexes into original data set */
};
typedef struct kdtree_qres kdtree_qres_t;

/* Build a tree from an array of data, of size N*D*sizeof(real) */
kdtree_t *kdtree_build(real *data, int ndata, int ndim, int maxlevel);

/* Compute how many levels should be used if you have "N" points and you
   want "Nleaf" points in the leaf nodes.
*/
int kdtree_compute_levels(int N, int Nleaf);

/* Range seach */
kdtree_qres_t *kdtree_rangesearch(kdtree_t *kd, real *pt, real maxdistsquared);

kdtree_qres_t *kdtree_rangesearch_iter(kdtree_t *kd, real *pt, real maxdistsquared);

kdtree_qres_t *kdtree_rangesearch_nosort(kdtree_t *kd, real *pt, real maxdistsquared);

/* Range seach using callback */
void kdtree_rangesearch_callback(kdtree_t *kd, real *pt, real maxdistsquared,
								 void (*rangesearch_callback)(kdtree_t* kd, real* pt, real maxdist2, real* computed_dist2, int indx, void* extra),
								 void* extra);

/* Counts points within range. */
int kdtree_rangecount(kdtree_t* kd, real* pt, real maxdistsquared);

/* Nearest neighbour: returns the index _in the kdtree_ of the nearest point;
 * the point is at  (kd->data + ind * kd->ndim)  and its permuted index is
 * (kd->perm[ind]).
 *
 * If "bestd2" is non-NULL, the distance-squared to the nearest neighbour
 * will be placed there.
 */
int kdtree_nearest_neighbour(kdtree_t* kd, real *pt, real* bestd2);

/* Nearest neighbour (if within a maximum range): returns the index
 * _in the kdtree_ of the nearest point, _if_ its distance is less than
 * maxd2.  (Otherwise, -1).
 *
 * If "bestd2" is non-NULL, the distance-squared to the nearest neighbour
 * will be placed there.
 */
int kdtree_nearest_neighbour_within(kdtree_t* kd, real *pt, real maxd2,
									real* bestd2);

/* Optimize the KDTree by by constricting hyperrectangles to minimum volume */
void kdtree_optimize(kdtree_t *kd);

/* Free results */
void kdtree_free_query(kdtree_qres_t *kd);

/* Free a tree; does not free kd->data */
void kdtree_free(kdtree_t *kd);

/* Output Graphviz .dot format version of the tree */
void kdtree_output_dot(FILE* fid, kdtree_t* kd);

/* Sanity-check a tree. 0=okay. */
int kdtree_check(kdtree_t* t);

/* Reused by inttree */
int kdtree_qsort_results(kdtree_qres_t *kq, int D);
int kdtree_quickselect_partition(real *arr, unsigned int *parr, int l, int r, int D, int d);

#endif /* KDTREE_H */
