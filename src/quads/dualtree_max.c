/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <string.h>

#include "dualtree_max.h"
#include "starutil.h"
#include "bl.h"

//#define debug(args...) printf(args)
#define debug(args...) {}

struct candidate {
    kdtree_node_t* n;
    // the query node against which the bounds were
    // computed.
    kdtree_node_t* ynode;
    double upper;
    double lower;
};
typedef struct candidate candidate;

int compare_candidates_lower(const void* v1, const void* v2) {
    candidate* c1 = (candidate*)v1;
    candidate* c2 = (candidate*)v2;
	// we want the list sorted in DECREASING order of lower-bound,
	// so this looks backward from what you'd expect.
	if (c1->lower > c2->lower)
		return -1;
	if (c1->lower < c2->lower)
		return 1;
	return 0;
}

int compare_candidates_upper(const void* v1, const void* v2) {
    candidate* c1 = (candidate*)v1;
    candidate* c2 = (candidate*)v2;
	if (c1->upper > c2->upper)
		return -1;
	if (c1->upper < c2->upper)
		return 1;
	return 0;
}

typedef int (*compare_function)(const void*, const void*);

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
void dualtree_max_recurse(kdtree_t* xtree,
						  kdtree_t* ytree,
						  pl* nodes, bl* leaves,
						  kdtree_node_t* ynode,
						  double pruning_threshold,
						  dualtree_max_callbacks* callbacks,
						  int gotoleaves,
						  int sort_on_upper) {

    max_bounds_function bounds = callbacks->bounds;
    void* bounds_extra = callbacks->bounds_extra;
	compare_function compare = (sort_on_upper ?
								compare_candidates_upper :
								compare_candidates_lower);

    debug("query %p: %i points; %i node candidates, %i leaves.\n",
		  query, query->num_points, blocklist_count(nodes), blocklist_count(leaves));

    // if all the candidates are leaves:
    if (!bl_size(nodes)) {
		// -prune the leaf list based on the final pruning threshold
		// -run the result function on the leaf set.
		max_start_of_results_function start;
		max_end_of_results_function end;
		max_result_function result;
		void* result_extra;
		int i;

		if (gotoleaves && !kdtree_node_is_leaf(ytree, ynode)) {
			dualtree_max_recurse(xtree, ytree, nodes, leaves,
								 kdtree_get_child1(ytree, ynode),
								 pruning_threshold, callbacks, gotoleaves,
								 sort_on_upper);
			dualtree_max_recurse(xtree, ytree, nodes, leaves,
								 kdtree_get_child2(ytree, ynode),
								 pruning_threshold, callbacks, gotoleaves,
								 sort_on_upper);
			return;
		}

		start = callbacks->start_results;
		end = callbacks->end_results;
		result = callbacks->result;
		result_extra = callbacks->result_extra;

		if (start) {
			start(callbacks->start_extra, ynode, &pruning_threshold);
		}
		if (result) {
			bl* keepleaves;
			int N;

			N = bl_size(leaves);
			keepleaves = bl_new(256, sizeof(candidate));

			for (i=0; i<N; i++) {
				candidate* cand = bl_access(leaves, i);
				candidate keepcand;
				if (cand->upper < pruning_threshold)
					continue;

				// careful here - the "leaves" list is shared between siblings
				// so we can't modify it.

				if (cand->ynode == ynode) {
					memcpy(&keepcand, cand, sizeof(candidate));
				} else {
					// recompute bounds wrt this query node...
					double lower, upper;
					bounds(bounds_extra, ynode, cand->n, pruning_threshold, &lower, &upper);
					if (upper < pruning_threshold)
						continue;

					// (careful - lower doesn't have to be computed unless upper > threshold...)
					if (lower > pruning_threshold)
						pruning_threshold = lower;
					keepcand.ynode = ynode;
					keepcand.n = cand->n;
					keepcand.lower = lower;
					keepcand.upper = upper;
				}
				bl_insert_sorted(keepleaves, &keepcand, compare);
			}

			N = pl_size(keepleaves);
			for (i=0; i<N; i++) {
				candidate* cand = bl_access(keepleaves, i);
				if (cand->upper < pruning_threshold)
					continue;
				result(result_extra, ynode, cand->n, &pruning_threshold, cand->lower, cand->upper);
			}

			bl_free(keepleaves);
		}
		if (end) {
			end(callbacks->end_extra, ynode);
		}

    } else {
		bool yleaf;
		int NYC;
		int i, j;
		int leaflength;
		bl* childnodes;
		double childthresh;

		childnodes = bl_new(256, sizeof(candidate));
		leaflength = bl_size(leaves);

		yleaf = kdtree_node_is_leaf(ytree, ynode);
		NYC = (yleaf ? 1 : 2);

		for (i=0; i<NYC; i++) {
			kdtree_node_t* ychild;
			candidate childcand;

			if (yleaf) {
				ychild = ynode;
			} else {
				if (i == 0)
					ychild = kdtree_get_child1(ytree, ynode);
				else
					ychild = kdtree_get_child2(ytree, ynode);
			}

			childthresh = pruning_threshold;
			childcand.ynode = ychild;

			debug("query %p: recursing on child %i: %p\n", query, i, ychild);

			for (j=0; j<bl_size(nodes); j++) {
				double lower, upper;
				candidate* cand = bl_access(nodes, j);
				int xc;

				if (cand->upper < childthresh) {
					debug("candidate %i (%p) pruned.\n", j, cand->n);
					continue;
				}
				debug("candidate %i (%p):\n", j, cand->n);

				for (xc=0; xc<2; xc++) {
					kdtree_node_t* xchild = (xc == 0 ?
											 kdtree_get_child1(xtree, cand->n) :
											 kdtree_get_child2(xtree, cand->n));
					bounds(bounds_extra, ynode, xchild, childthresh, &lower, &upper);

					debug("query child %p, cand %i, child %p: bounds [%g, %g]\n",
						  ychild, j, xchild, lower, upper);

					if (upper > childthresh) {
						if (lower > childthresh)
							childthresh = lower;
						childcand.lower = lower;
						childcand.upper = upper;
						childcand.n = xchild;

						if (kdtree_node_is_leaf(xtree, xchild)) {
							bl_append(leaves, &childcand);
							debug("Added leaf candidate.\n");
						} else {
							bl_insert_sorted(childnodes, &childcand, compare);
							debug("Added node candidate.\n");
						}
					} else {
						debug("child pruned.\n");
					}
				}
			}

			dualtree_max_recurse(xtree, ytree, childnodes, leaves, ychild, childthresh, callbacks, gotoleaves, sort_on_upper);

			bl_remove_index_range(leaves, leaflength, bl_size(leaves) - leaflength);
			if (i == 0) {
				bl_remove_all_but_first(childnodes);
			}

		}

		bl_free(childnodes);
    }
}

void dualtree_max(kdtree_t* xtree, kdtree_t* ytree,
				  dualtree_max_callbacks* callbacks,
				  int go_to_leaves, int sort_on_upper) {
    bl *nodes, *leaves;
    candidate cand;
    double lower, upper;
	kdtree_node_t *xroot, *yroot;

    nodes  = bl_new(256, sizeof(candidate));
    leaves = bl_new(256, sizeof(candidate));

	yroot = kdtree_get_root(ytree);
	xroot = kdtree_get_root(xtree);

    callbacks->bounds(callbacks->bounds_extra, yroot, xroot,
					  -1e300, &lower, &upper);

    cand.ynode = yroot;
    cand.n = xroot;
    cand.lower = lower;
    cand.upper = upper;

    if (kdtree_node_is_leaf(xtree, xroot)) {
		bl_append(leaves, &cand);
    } else {
		bl_append(nodes, &cand);
    }

    dualtree_max_recurse(xtree, ytree, nodes, leaves,
						 yroot, lower, callbacks, go_to_leaves,
						 sort_on_upper);

    bl_free(nodes);
    bl_free(leaves);
}

