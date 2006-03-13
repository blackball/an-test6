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

#include "starutil.h"
#include "kdtree/kdtree.h"

typedef bool (*decision_function_2)(void* extra, kdtree_node_t* search, kdtree_node_t* query);
typedef void (*start_of_results_function_2)(void* extra, kdtree_node_t* query);
typedef void (*result_function_2)(void* extra, kdtree_node_t* search, kdtree_node_t* query);
typedef void (*end_of_results_function_2)(void* extra, kdtree_node_t* query);

struct dualtree_callbacks_2 {
	decision_function_2          decision;
	void*                      decision_extra;
	start_of_results_function_2  start_results;
	void*                      start_extra;
	result_function_2            result;
	void*                      result_extra;
	end_of_results_function_2    end_results;
	void*                      end_extra;
};
typedef struct dualtree_callbacks_2 dualtree_callbacks_2;

void dualtree_search_2(kdtree_t* search, kdtree_t* query,
					   dualtree_callbacks_2* callbacks);


