#include "kdtree/kdtree.h"
#include "bl.h"

typedef void (*max_bounds_function_2)(void* extra, kdtree_node_t* ynode, kdtree_node_t* xnode,
									  double pruning_thresh, double* lower, double* upper);
typedef void (*max_start_of_results_function_2)(void* extra, kdtree_node_t* ynode,
												/*pl* leaves,*/
												double* pruning_thresh);
typedef void (*max_result_function_2)(void* extra, kdtree_node_t* ynode, kdtree_node_t* xnode,
									  double* pruning_threshold, double lower, double upper);
typedef void (*max_end_of_results_function_2)(void* extra, kdtree_node_t* query);

struct dualtree_max_callbacks_2 {
    max_bounds_function_2            bounds;
    void*                          bounds_extra;
    max_start_of_results_function_2  start_results;
    void*                          start_extra;
    max_result_function_2            result;
    void*                          result_extra;
    max_end_of_results_function_2    end_results;
    void*                          end_extra;
};
typedef struct dualtree_max_callbacks_2 dualtree_max_callbacks_2;

void dualtree_max_2(kdtree_t* xtree, kdtree_t* ytree,
					dualtree_max_callbacks_2* callbacks,
					int go_to_leaves);


