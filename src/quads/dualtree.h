/**
   \file

   Dual-tree search.

   Input:
   -a search tree
   -a query tree
   -a function that takes two nodes and returns true if the result set should
    contain that pair of nodes.
   -an extra value that will be passed to the decision function.
   -a function that is called for each pair of leaf nodes.
     (** actually, at least one of the nodes will be a leaf. **)
   -an extra value that will be passed to the leaf-node function

   The query tree is a kd-tree built out of the points you want to query.

   The search and query trees can be the same tree.
*/

//     (** query node will be a leaf; search node may not be. **)

#include "KD/kdtree.h"

typedef bool (*decision_function)(void* extra, node* search, node* query);
typedef void (*result_function)(void* extra, node* search, node* query);


void dual_tree_search(kdtree* search, kdtree* query,
                      decision_function decision,
                      void* decision_extra,
                      result_function result,
                      void* result_extra);

