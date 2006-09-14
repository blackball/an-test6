#ifndef KDTREE_H
#define KDTREE_H

#include <stdio.h>
#include <stdint.h>

#define KDTREE_MAX_LEVELS 1000
//#define KDT_INFTY 1e309

#define KDT_INFTY_DOUBLE 1e309
#define KDT_INFTY_FLOAT  1e39

#define KD_OPTIONS_COMPUTE_DISTS  0x1
#define KD_OPTIONS_SORT_DISTS     0x2
#define KD_OPTIONS_SMALL_RADIUS   0x4

typedef uint32_t u32;
typedef uint16_t u16;

typedef unsigned char bool;
#define TRUE 1
#define FALSE 0

#if 0
typedef double real;
struct kdtree_hr {
	real *low;            /* Lower hyperrectangle boundary */
	real *high;           /* Upper hyperrectangle boundary */
};
typedef struct kdtree_hr kdtree_hr_t;
#endif



struct kdtree_node {
    unsigned int l,r;              /* data(l:r) are coordinates below this node */
	/* Implicit hyperrectangle 
	real HR[sizeof(real)*D] Lower
	real HR[sizeof(real)*D] Upper
	*/
};
typedef struct kdtree_node kdtree_node_t;

struct kdtree {
	// (compatibility mode)
	kdtree_node_t *nodes;   /* Flat tree storing nodes and HR's */

	unsigned int* lr;      /* Points owned by leaf nodes, stored and manipulated
							  in a way that's too complicated to explain in this comment. */

	unsigned int *perm;    /* Permutation index */

	/* Bounding box: list of D-dimensional lower hyperrectangle corner followed by D-dimensional upper corner. */
	union {
		float* f;
		double* d;
		u32* i;
		u16* s;
		void* any;
	} bb;

	/* Split position (& dimension for ints). */
	union {
		float* f;
		double* d;
		u32* i;
		u16* s;
		void* any;
	} split;

	// Split dimension for floating-point types
	unsigned char* splitdim;

	// bitmasks for the split dimension and location.
	unsigned int dimbits;
	unsigned int dimmask;
	unsigned int splitmask;

	union {
		/* Raw coordinate data as xyzxyzxyz */
		float* f;
		double* d;
		u32* i;
		u16* s;
		void* any;
	} data;

	// does this data belong to me alone, ie, did I make a copy of the original
	// data array?
	bool datacopy;

	/*
	  union {
	  float* f;
	  double* d;
	  } minval;
	  union {
	  float* f;
	  double* d;
	  } maxval;
	  union {
	  float* f;
	  double* d;
	  } scale;
	*/

	double* minval;
	double* maxval;
	double* scale;

	unsigned int ndata;     /* Number of items */
	unsigned int ndim;      /* Number of dimensions */
	unsigned int nnodes;    /* Number of nodes */
	//unsigned int nleaf;     /* Number of leaf nodes */
	unsigned int nbottom;
	unsigned int ninterior; /* Number of internal nodes */
	void* mmapped;          /* Next two are for mmap'd access */
	unsigned int mmapped_size;  
};
typedef struct kdtree kdtree_t;

struct kdtree_qres {
	unsigned int nres;
#if 0
	real *results;         /* Each of the points returned from a query */
	real *sdists;          /* Squared distance from query point */
#endif
	unsigned int *inds;    /* Indexes into original data set */
};
typedef struct kdtree_qres kdtree_qres_t;


#if 0

/* Compute how many levels should be used if you have "N" points and you
   want "Nleaf" points in the leaf nodes.
*/
int kdtree_compute_levels(int N, int Nleaf);

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

#endif



/* Free results */
void kdtree_free_query(kdtree_qres_t *kd);

/* Free a tree; does not free kd->data */
void kdtree_free(kdtree_t *kd);

#if 0
/* Output Graphviz .dot format version of the tree */
void kdtree_output_dot(FILE* fid, kdtree_t* kd);

/* Sanity-check a tree. 0=okay. */
int kdtree_check(kdtree_t* t);

/* Reused by inttree */
int kdtree_qsort_results(kdtree_qres_t *kq, int D);
int kdtree_quickselect_partition(real *arr, unsigned int *parr, int l, int r, int D, int d);
int kdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d);
#endif

// include dimension-generic versions of the dimension-specific code.
#define KD_DIM_GENERIC 1

#endif /* KDTREE_H */


#if defined(KD_DIM) || defined(KD_DIM_GENERIC)

#define KDTT_DOUBLE      0
#define KDTT_FLOAT       1
#define KDTT_DOUBLE_U32  2
#define KDTT_DOUBLE_U16  3

#if defined(KD_DIM)
  #undef KDID
  #undef GLUE2
  #undef GLUE

  #define GLUE2(a, b) a ## b
  #define GLUE(a, b) GLUE2(a, b)
  #define KDID(x) GLUE(x ## _, KD_DIM)
#else
  #define KDID(x) x
#endif
#define KDFUNC(x) KDID(x)

/* Build a tree from an array of data, of size N*D*sizeof(real) */
kdtree_t* KDFUNC(kdtree_build)
	 (void *data, int N, int D, int maxlevel, int treetype, bool bb,
	  bool copydata);

#if 0

/* Range seach */
kdtree_qres_t* KDFUNC(kdtree_rangesearch)(kdtree_t *kd, real *pt, real maxdistsquared);

kdtree_qres_t* KDFUNC(kdtree_rangesearch_nosort)(kdtree_t *kd, real *pt, real maxdistsquared);

kdtree_qres_t* KDFUNC(kdtree_rangesearch_options)(kdtree_t *kd, real *pt, real maxdistsquared, int options);

#endif//if0

#if !defined(KD_DIM)
#undef KD_DIM_GENERIC
#endif

#endif

