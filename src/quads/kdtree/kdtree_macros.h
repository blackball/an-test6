#ifndef KDTREE_MACROS_H
#define KDTREE_MACROS_H

/*****************************************************************************/
/* Utility macros                                                            */
/*****************************************************************************/
#define KDTREE_MAX_DIM 10
#define KDTREE_MAX_RESULTS 1000

/* Most macros operate on a variable kdtree_t *kd, assumed to exist. */
/* x is a node index, d is a dimension, and n is a point index */

#if defined(KD_DIM)
#define DIMENSION   (KD_DIM)
#else
#define DIMENSION   (kd->ndim)
#endif

#define SIZEOF_PT  (sizeof(real)*DIMENSION)

#define NODE_SIZE      (sizeof(kdtree_node_t) + (SIZEOF_PT * 2))

#define HIGH_HR(x, d)  ((real*) (((char*)kd->tree) \
                                 + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + SIZEOF_PT \
                                 + sizeof(real)*(d)

#define COORD(n,d)     ((real*)(kd->data + DIMENSION*(n) + (d)))

#define NODE_HIGH_BB(n) ((real*)((char*)(n) \
								 + sizeof(kdtree_node_t) \
								 + sizeof(real)*DIMENSION))

								 // warning, this only works if "n" has type kdtree_node_t*.
#define NODE_LOW_BB(n) ((real*)((n)+1))

#define NODE(x)        ((kdtree_node_t*) (((char*)kd->tree) + NODE_SIZE*(x)))

#define LOW_HR(x, d)   ((real*) (((char*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*(d)))

#define PARENT(x)      NODE(((x)-1)/2)
#define CHILD_NEG(x)   NODE(2*(x) + 1)
#define CHILD_POS(x)   NODE(2*(x) + 2)
#define ISLEAF(x)      ((2*(x)+1) >= kd->nnodes)

#endif
