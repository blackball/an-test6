#include "dualtree_max.h"
#include "blocklist.h"

#define BAIL_OUT() printf("Bailing out: file %s, line %i", __FILE__, __LINE__)

#define debug(...) {}
// #define debug printf

struct candidate {
    node* n;
    // the query node against which the bounds were
    // computed.
    node* query;
    double upper;
    double lower;
};
typedef struct candidate candidate;

int compare_candidates(void* v1, void* v2) {
    double diff;
    candidate* c1 = (candidate*)v1;
    candidate* c2 = (candidate*)v2;
    diff = c1->lower - c2->lower;
    if (diff > 0.0)
	return 1;
    else if (diff < 0.0)
	return -1;
    else
	return 0;
}

/*
  During the recursion, we maintain two lists of nodes from the search tree
  that could possibly contain the maximum.  One list contains leaves, and
  the other contains non-leaves.  Each of these candidate nodes
  has upper and lower bounds.  The maximum of the lower bounds is the
  pruning threshold: any node whose upper bound is less than the threshold
  is pruned, since it can't possibly contain the best point.

  At each step in the recursion, we recurse on the children of the non-leaf
  candidates and the children of the target node, if they exist, or the target
  node itself.

  A heuristic is to sort the candidates in order of decreasing lower bound,
  and recurse on them in this order, since this tends to tighten the pruning
  bound faster, which might allow some of the parent's candidates to be
  eliminated.
*/
void dualtree_max_recurse(blocklist* nodes, blocklist* leaves,
			  node* query,
			  double pruning_threshold,
			  dualtree_max_callbacks* callbacks) {

    max_bounds_function bounds = callbacks->bounds;
    void* bounds_extra = callbacks->bounds_extra;

    debug("query %p: %i points; %i node candidates, %i leaves.\n",
	   query, query->num_points, blocklist_count(nodes), blocklist_count(leaves));

    // if all the candidates are leaves:
    if (!blocklist_count(nodes)) {
	// -prune the leaf list based on the final pruning threshold
	
	// -run the result function on the leaf set.

	max_start_of_results_function start = callbacks->start_results;
	max_end_of_results_function end = callbacks->end_results;
	max_result_function result = callbacks->result;
	void* result_extra = callbacks->result_extra;
	int i;

	if (start) {
	    start(callbacks->start_extra, query, leaves, &pruning_threshold);
	}
	if (result) {
	    blocklist* keepleaves;
	    int N;

	    N = blocklist_count(leaves);
	    keepleaves = blocklist_new(256, sizeof(candidate));

	    for (i=0; i<N; i++) {
		candidate* cand = blocklist_access(leaves, i);
		candidate keepcand;
		if (cand->upper < pruning_threshold) {
		    continue;
		}

		// careful here - the "leaves" list is shared between siblings
		// so we can't modify it.

		if (cand->query == query) {
		    memcpy(&keepcand, cand, sizeof(candidate));
		    /*
		      keepcand.query = cand->query;
		      keepcand.n = cand->n;
		      keepcand.lower = cand->lower;
		      keepcand.upper = cand->upper;
		    */
		} else {
		    // recompute bounds wrt this query node...
		    double lower, upper;
		    bounds(bounds_extra, query, cand->n, pruning_threshold, &lower, &upper);
		    if (lower > pruning_threshold) {
			pruning_threshold = lower;
		    }
		    keepcand.query = query;
		    keepcand.n = cand->n;
		    keepcand.lower = lower;
		    keepcand.upper = upper;
		}
		blocklist_insert_sorted(keepleaves, &keepcand, compare_candidates);
	    }

	    N = blocklist_count(keepleaves);
	    for (i=0; i<N; i++) {
		candidate* cand = blocklist_access(keepleaves, i);
		if (cand->upper < pruning_threshold) {
		    continue;
		}
		result(result_extra, query, cand->n, &pruning_threshold, cand->lower, cand->upper);
	    }
	}
	if (end) {
	    end(callbacks->end_extra, query);
	}

    } else {
	bool yleaf = node_is_leaf(query);
	int NYC;
	int i, j;
	int leaflength;
	blocklist* childnodes;
	double childthresh;

	childnodes = blocklist_new(256, sizeof(candidate));
	leaflength = blocklist_count(leaves);

	NYC = (yleaf ? 1 : 2);

	for (i=0; i<NYC; i++) {
	    node* ychild;
	    candidate childcand;

	    ychild = (yleaf ? query :
		      (i==0 ? query->child1 :
		       query->child2));

	    childthresh = pruning_threshold;
	    childcand.query = ychild;

	    debug("query %p: recursing on child %i: %p\n", query, i, ychild);

	    for (j=0; j<blocklist_count(nodes); j++) {
		// 
		double lower, upper;
		candidate* cand = blocklist_access(nodes, j);
		candidate childcand;
		int xc;

		if (cand->upper < childthresh) {
		    debug("candidate %i (%p) pruned.\n", j, cand->n);
		    continue;
		}
		debug("candidate %i (%p):\n", j, cand->n);

		for (xc=0; xc<2; xc++) {
		    node* xchild = (xc == 0 ?
				    cand->n->child1 :
				    cand->n->child2);
		    bounds(bounds_extra, query, xchild, childthresh, &lower, &upper);

		    debug("query child %p, cand %i, child %p: bounds [%g, %g]\n",
			   ychild, j, xchild, lower, upper);

		    if (upper > childthresh) {
			if (lower > childthresh) {
			    childthresh = lower;
			}
			childcand.lower = lower;
			childcand.upper = upper;
			childcand.n = xchild;

			if (node_is_leaf(xchild)) {
			    blocklist_append(leaves, &childcand);
			    debug("Added leaf candidate.\n");
			} else {
			    //blocklist_append(childnodes, &childcand);
			    blocklist_insert_sorted(childnodes, &childcand, compare_candidates);
			    debug("Added node candidate.\n");
			}
		    } else {
			debug("child pruned.\n");
		    }
		}
	    }

	    dualtree_max_recurse(childnodes, leaves, ychild, childthresh, callbacks);

	    blocklist_remove_index_range(leaves, leaflength, blocklist_count(leaves) - leaflength);
	    if (i == 0) {
		blocklist_remove_all_but_first(childnodes);
	    }

	}

	blocklist_free(childnodes);
    }
}

void dualtree_max(kdtree* search, kdtree* query,
		  dualtree_max_callbacks* callbacks) {
    blocklist *nodes, *leaves;
    candidate cand;
    double lower, upper;

    nodes  = blocklist_new(256, sizeof(candidate));
    leaves = blocklist_new(256, sizeof(candidate));

    callbacks->bounds(callbacks->bounds_extra, query->root, search->root, -1e300, &lower, &upper);

    cand.query = query->root;
    cand.n = search->root;
    cand.lower = lower;
    cand.upper = upper;

    if (node_is_leaf(search->root)) {
	blocklist_append(leaves, &cand);
    } else {
	blocklist_append(nodes, &cand);
    }

    dualtree_max_recurse(nodes, leaves, query->root, lower, callbacks);

    blocklist_free(nodes);
    blocklist_free(leaves);
}

