/**
*/

#include "KD/kdtree.h"
#include "blocklist.h"

typedef void (*max_bounds_function)(void* extra, node* query, node* search, double pruning_thresh, double* lower, double* upper);
typedef void (*max_start_of_results_function)(void* extra, node* query, blocklist* leaves, double* pruning_thresh);
typedef void (*max_result_function)(void* extra, node* query, node* search, double* pruning_threshold,
				    double lower, double upper);
typedef void (*max_end_of_results_function)(void* extra, node* query);

struct dualtree_max_callbacks {
    max_bounds_function            bounds;
    void*                          bounds_extra;
    max_start_of_results_function  start_results;
    void*                          start_extra;
    max_result_function            result;
    void*                          result_extra;
    max_end_of_results_function    end_results;
    void*                          end_extra;
};
typedef struct dualtree_max_callbacks dualtree_max_callbacks;

void dualtree_max(kdtree* search, kdtree* query,
				  dualtree_max_callbacks* callbacks,
				  int go_to_leaves);


