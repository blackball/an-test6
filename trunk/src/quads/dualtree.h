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

#include "KD/kdtree.h"

typedef bool (*decision_function)(void* extra, node* search, node* query);
typedef void (*start_of_results_function)(void* extra, node* query);
typedef void (*result_function)(void* extra, node* search, node* query);
typedef void (*end_of_results_function)(void* extra, node* query);

struct dualtree_callbacks {
	decision_function          decision;
	void*                      decision_extra;
	start_of_results_function  start_results;
	void*                      start_extra;
	result_function            result;
	void*                      result_extra;
	end_of_results_function    end_results;
	void*                      end_extra;
};
typedef struct dualtree_callbacks dualtree_callbacks;

void dualtree_search(kdtree* search, kdtree* query,
					 dualtree_callbacks* callbacks);


