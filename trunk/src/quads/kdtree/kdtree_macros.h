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

#define NODE_SIZE      (sizeof(kdtree_node_t) + sizeof(real)*KD_DIM*2)
#define HIGH_HR(x, d)  ((real*) (((void*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*((d) + KD_DIM)))
#define COORD(n,d)     ((real*)(kd->data + KD_DIM*(n) + (d)))
#define NODE_HIGH_BB(n) ((real*)((char*)(n) \
								 + sizeof(kdtree_node_t) \
								 + sizeof(real)*KD_DIM))

#else

#define NODE_SIZE      (sizeof(kdtree_node_t) + sizeof(real)*kd->ndim*2)
#define NODE_HIGH_BB(n) ((real*)((char*)(n) \
								 + sizeof(kdtree_node_t) \
								 + sizeof(real)*kd->ndim))
#define HIGH_HR(x, d)  ((real*) (((void*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*((d) \
                                 + kd->ndim)))
#define COORD(n,d)     ((real*)(kd->data + kd->ndim*(n) + (d)))

#endif



#define NODE_LOW_BB(n) ((real*)((n)+1))
#define NODE(x)        ((kdtree_node_t*) (((void*)kd->tree) + NODE_SIZE*(x)))
#define LOW_HR(x, d)   ((real*) (((void*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*(d)))
#define PARENT(x)      NODE(((x)-1)/2)
#define CHILD_NEG(x)   NODE(2*(x) + 1)
#define CHILD_POS(x)   NODE(2*(x) + 2)
#define ISLEAF(x)      ((2*(x)+1) >= kd->nnodes)

#endif
