/*
   File:        kdtree.h
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:35:27 EST 2004
   Description: Simple kdtree implementation
   Copyright (c) Carnegie Mellon University
*/

#ifndef kdtree_H
#define kdtree_H

/* Includes the "node" structure that represents a node in the kdtree, and also
   includes the "kdtree" structure that contains (a pointer to) the root
   node of the tree plus some other useful information.

   In this project "k" always refers to the dimensionality of the input space.
*/

/* Before reading this .h file, it is worth understanding 

     hrect.h - The hyper-rectangle data structure (used for bounding boxes)
     amiv.h  - The ivec structure (vector of integers)
     amdyv_array.h - Read the part about dyv_array (vector of dyv's, where
                a dyv is a vector of floating point numbers).
*/

#include "distances.h"

/* A "node" contains a set of points in k-d space.

   There are two types of node:

     A "leaf node" explicitly contains an array of datapoints.

     A "non-leaf node" does not have an explicit list of points associated with
     it. It does have two child nodes, called child1 and child2.

   The set of points owned by a given node n is:

     If n is a leaf node: the points in n's explicit points array.

     If n is a non-leaf node:
          (set of points owned by n->child1) union (set of points owned by n->child2)

*/
typedef struct node
{
  hrect *box;        /* The bounding box of the points owned by n. */
  int num_points;    /* The number of points owned by n */

  ivec *pindexes;    /* NULL if a non-leaf node.
                        If a leaf-node, contains the pindexes associated with points
                        owned by the current node. What's a pindex? See the 
                        "Pindex Note" below.
		     */
  dyv_array *points; /* NULL if a non-leaf node.
                        If a leaf-node, contains the points owned by n,
                        in which case num_points == dyv_array_size(n->points) and
                        num_points == ivec_size(pindexes).
		     */

  struct node *child1; /* NULL if a leaf node.
                          If a non-leaf node, contains a child of n */
  struct node *child2; /* NULL if a leaf node.
                          If a non-leaf node, contains a child of n */

                       /* We define

		       set of points owned by n ==
                         (set of points owned by n->child1) union 
                         (set of points owned by n->child2)

                       Notice that we're insisting that

                        leaf if and only if pindexes != NULL 
                        leaf if and only if points != NULL 
                        leaf if and only if child1 == NULL 
                        leaf if and only if child2 == NULL 

			and so

                        child1 == NULL if and only if child2 == NULL
                        pindexes == NULL if and only if points == NULL

		       */
} node;

/* A kdtree structure is really just a structre that holds a root node
   of a kdtree, but it also includes some information about the tree.
*/
typedef struct kdtree
{
  node *root;     /* The root node. */
  int num_nodes;  /* The number of nodes (num leaf plus num internal) in the tree */
  int max_depth;  /* The maximum depth of any node in the tree. If the tree
                     contained only one node, this would be 1 */
  int rmin;       /* The rmin parameter (described in the mk_kdtree_from_points
                     function below) with which the tree was built. No leaf node
                     contains more than "rmin" points. */
} kdtree;

/* Pindexes explained
   -------- ---------

   "pindex" stands for "point index". Any dataset is made up of a set of
   R datapoints in k-dimensional space. Here's an example where R == 5 and k==3

    Point 0:   (3.0,1.0,4.0)
    Point 1:   (5.0,9.0,2.0)
    Point 2:   (6.0,5.0,3.0)
    Point 3:   (5.0,8.0,9.0)
    Point 4:   (7.0,9.0,3.0)

   The pindex is the point-number (zero-indexed)...a concise way of refering to
   one of the points in the dataset. It's useful because, for example, when we
   are searching for the 12 nearest neighbors of some query, we don't want to
   return a copy of the actual 12 vectors, but instead just a reference to
   which points in the original dataset are those twelve vectors. And so we'll
   just return an array (actually an ivec from amiv.h) of twelve pindexes.

   You will often see the full dataset of points referred to as

     dyv_array *pindex_to_point

   That's because the dyv_array is an array that maps the integer index called
   "pindex" to a point (where a point is a vector in k-dimensional space).
*/


/* Frees kdtree and all its components (including all its nodes and their contents) */
void free_kdtree(kdtree *x);

/* Prints the structure using the standard Auton printing convention */
void fprintf_kdtree(FILE *s,char *m1,kdtree *x,char *m2);

/* Simple print to stdout. Useful to call in the debugger to see pretty-printed
   dump of tree */
void pkdtree(kdtree *x);

/* Makes a complete copy of old and all its subcomponents. */
kdtree *mk_copy_kdtree(kdtree *old);

/* Builds and returns a kdtree.

   dyv_array_ref(pindex_to_point,pi) is a dyv of size k representing the pi'th
                                     datapoint in our input dataset.

   rmin is the leaf-decision parameter. A node will be a leaf if either
    (a) It has "rmin" or fewer points
    (b) All its points are in the same location.
*/
kdtree *mk_kdtree_from_points(dyv_array *pindex_to_point,int rmin);

/* Adds a point to a kdtree. */
void add_point_to_kdtree(kdtree *kd, dyv *x);
double add_point_to_kdtree_dsq(kdtree *kd, dyv *x, 
			       int newpindex, int *nearestpindex);

/* Helper function for adding point to kdtree */
void replace_pindexes(node *subtree, ivec *new_pindexes);

/* Returns the depth of the tree. If the tree is just a root node, returns 1 */
int kdtree_max_depth(kdtree *kd);

/* Returns "k", the number of dimensions of the datapoints in kd */
int kdtree_num_dims(kdtree *kd);

/* Returns the rmin parameter that had been used to build the tree. */
int kdtree_rmin(kdtree *kd);

/* Returns the number of points "R" in the dataset represented by the tree */
int kdtree_num_points(kdtree *kd);

/* Returns the number of nodes the tree. If rmin == 1 and no points are exactly
   on top of each other, this is 2R-1 (where R = number of points in dataset) */
int kdtree_num_nodes(kdtree *kd);

/* Prints (to stdout) the vital statistics about the tree */
void explain_kdtree(kdtree *kd);

/***** Utility functions on nodes ******/

/* Usually a programmer who is using functions from this file will only
   need to work with the kdtree structure. But the following functions are
   in the header for those rare cases where the programmer actually needs to work
   on the nodes. 
*/

/* The minimum squared distance from x to any point in n */
double node_dyv_min_dsqd(node *n,dyv *x);

/* The maximum squared distance from x to any point in n */
double node_dyv_max_dsqd(node *n,dyv *x);

/* The next eight lines are just an "inline" definition of a function

bool node_is_leaf(n);

which returns TRUE if and only if n is a leaf. For non-C experts, the reason
it has this strange definition is so that if compiled with AM_FAST declared
it will be expanded as a macro and so we'll incur no function call overhead. 

When called in debug mode it returns the same result, but does some 
consistency checking. */

/* Returns TRUE iff n is a leaf (i.e. has no children) */
bool slow_node_is_leaf(node *n);

#ifdef AMFAST
#define node_is_leaf(n) ((n)->child1==NULL)
#else
#define node_is_leaf(n) slow_node_is_leaf(n)
#endif


#endif /* ifndef kdtree_H */
