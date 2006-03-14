#include "dualtree.h"
#include "bl.h"

#define BAIL_OUT() printf("Bailing out: file %s, line %i", __FILE__, __LINE__)

/*
  At each step of the recursion, we have a query node and a list of
  candidate search nodes.
*/
void dualtree_recurse(kdtree_t* xtree, kdtree_t* ytree, il* nodes, il* leaves,
					  int ynodeid, dualtree_callbacks* callbacks) {

	// general idea:
	//   for each element in search list:
	//     if decision(elem, query)
	//       add children of elem to child search list
	//   recurse on children of query, child search list
	//   empty the child search list

	// annoyances:
	//   -trees are of different heights, so we can reach the leaves of one
	//    before the leaves of the other.  if we hit a leaf in the query
	//    tree, just call the result function with all the search nodes,
	//    leaves or not.  if we hit a leaf in the search tree, add it to
	//    the leaf-list.
	//   -we want to share search lists between the children, but that means
	//    that the children can't modify the lists - or if they do, they
	//    have to undo any changes they make.  if we only append items, then
	//    we can undo changes by remembering the original tail and setting
	//    its "next" to null after we're done (we also have to free any nodes
	//    we allocate).

	int leafmarker;
	il* childnodes;

	kdtree_node_t* ynode;

	decision_function decision;
	void* decision_extra;
	int i, N;

	ynode = kdtree_nodeid_to_node(ytree, ynodeid);

	// if the query node is a leaf, run the result function on each
	// search node.
	if (kdtree_nodeid_is_leaf(ytree, ynodeid)) {
		start_of_results_function start = callbacks->start_results;
		end_of_results_function end = callbacks->end_results;
		result_function result = callbacks->result;
		void* result_extra = callbacks->result_extra;

		if (start) {
			start(callbacks->start_extra, ynode);
		}

		if (result) {
			// non-leaf nodes
			N = il_size(nodes);
			for (i=0; i<N; i++) {
				int xid = il_get(nodes, i);
				kdtree_node_t* xnode = kdtree_nodeid_to_node(xtree, xid);
				result(result_extra, xnode, ynode);
			}
		    // leaf nodes
			N = il_size(leaves);
			for (i=0; i<N; i++) {
				int xid = il_get(leaves, i);
				kdtree_node_t* xnode = kdtree_nodeid_to_node(xtree, xid);
				result(result_extra, xnode, ynode);
			}
		}

		if (end) {
			end(callbacks->end_extra, ynode);
		}
		return;
	}

	// if there are search leaves but no search nodes, run the result
	// function on each leaf.  (Note that the query node is not a leaf!)
	if (!il_size(nodes)) {
	    start_of_results_function start = callbacks->start_results;
	    end_of_results_function   end   = callbacks->end_results;
	    result_function result = callbacks->result;
	    void* result_extra = callbacks->result_extra;

		if (start) {
			start(callbacks->start_extra, ynode);
		}

		// leaf nodes
		if (result) {
			N = il_size(leaves);
			for (i=0; i<N; i++) {
				int xid = il_get(leaves, i);
				kdtree_node_t* xnode = kdtree_nodeid_to_node(xtree, xid);
				result(result_extra, xnode, ynode);
			}
		}

		if (end) {
			end(callbacks->end_extra, ynode);
		}
		return;
	}

	leafmarker = il_size(leaves);
	childnodes = il_new(32);
	decision = callbacks->decision;
	decision_extra = callbacks->decision_extra;

	N = il_size(nodes);
	for (i=0; i<N; i++) {
		int childid1, childid2;
		int xid = il_get(nodes, i);
		kdtree_node_t* xnode = kdtree_nodeid_to_node(xtree, xid);
		if (!decision(decision_extra, xnode, ynode))
			continue;

		childid1 = kdtree_get_childid1(xtree, xid);
		childid2 = kdtree_get_childid2(xtree, xid);

		if (kdtree_nodeid_is_leaf(xtree, childid1))
			il_append(leaves, childid1);
		else
			il_append(childnodes, childid1);

		if (kdtree_nodeid_is_leaf(xtree, childid2))
			il_append(leaves, childid2);
		else
			il_append(childnodes, childid2);
	}

	// recurse on the children!
	dualtree_recurse(xtree, ytree, childnodes, leaves,
					 kdtree_get_childid1(ytree, ynodeid), callbacks);

	dualtree_recurse(xtree, ytree, childnodes, leaves,
					 kdtree_get_childid2(ytree, ynodeid), callbacks);

	// put the "leaves" list back the way it was...
	il_remove_index_range(leaves, leafmarker, il_size(leaves)-leafmarker);

	il_free(childnodes);
}

void dualtree_search(kdtree_t* xtree, kdtree_t* ytree,
					 dualtree_callbacks* callbacks) {
	il* nodes = il_new(32);
	il* leaves = il_new(32);

	if (kdtree_node_is_leaf(xtree, xtree->tree))
		il_append(leaves, 0);
	else
		il_append(nodes, 0);

	dualtree_recurse(xtree, ytree, nodes, leaves, 0, callbacks);

	il_free(nodes);
	il_free(leaves);
}

