#include "dualtree.h"
#include "bl.h"

/*
  At each step of the recursion, we have a query node and a list of
  candidate search nodes.
*/
static void dualtree_recurse(kdtree_t* xtree, kdtree_t* ytree,
							 il* nodes, il* leaves,
							 int ynode, dualtree_callbacks* callbacks) {

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
	decision_function decision;
	void* decision_extra;
	int i, N;

	// if the query node is a leaf...
	if (KD_IS_LEAF(ytree, ynode)) {
		// ... then run the result function on each search node.
		result_function result = callbacks->result;
		void* result_extra = callbacks->result_extra;

		if (callbacks->start_results)
			callbacks->start_results(callbacks->start_extra, ytree, ynode);

		if (result) {
			// non-leaf nodes
			N = il_size(nodes);
			for (i=0; i<N; i++)
				result(result_extra, xtree, il_get(nodes, i), ytree, ynode);
		    // leaf nodes
			N = il_size(leaves);
			for (i=0; i<N; i++)
				result(result_extra, xtree, il_get(leaves, i), ytree, ynode);
		}
		if (callbacks->end_results)
			callbacks->end_results(callbacks->end_extra, ytree, ynode);

		return;
	}

	// if there are search leaves but no search nodes, run the result
	// function on each leaf.  (Note that the query node is not a leaf!)
	if (!il_size(nodes)) {
	    result_function result = callbacks->result;
	    void* result_extra = callbacks->result_extra;

		if (callbacks->start_results)
			callbacks->start_results(callbacks->start_extra, ytree, ynode);

		// leaf nodes
		if (result) {
			N = il_size(leaves);
			for (i=0; i<N; i++)
				result(result_extra, xtree, il_get(leaves, i), ytree, ynode);
		}

		if (callbacks->end_results)
			callbacks->end_results(callbacks->end_extra, ytree, ynode);

		return;
	}

	leafmarker = il_size(leaves);
	childnodes = il_new(32);
	decision = callbacks->decision;
	decision_extra = callbacks->decision_extra;

	N = il_size(nodes);
	for (i=0; i<N; i++) {
		int child1, child2;
		int xnode = il_get(nodes, i);
		if (!decision(decision_extra, xtree, xnode, ytree, ynode))
			continue;

		child1 = KD_CHILD_LEFT(xnode);
		child2 = KD_CHILD_RIGHT(xnode);

		if (KD_IS_LEAF(xtree, child1)) {
			il_append(leaves, child1);
			il_append(leaves, child2);
		} else {
			il_append(childnodes, child1);
			il_append(childnodes, child2);
		}
	}

	// recurse on the Y children!
	dualtree_recurse(xtree, ytree, childnodes, leaves,
					 KD_CHILD_LEFT(ynode), callbacks);
	dualtree_recurse(xtree, ytree, childnodes, leaves,
					 KD_CHILD_RIGHT(ynode), callbacks);

	// put the "leaves" list back the way it was...
	il_remove_index_range(leaves, leafmarker, il_size(leaves)-leafmarker);
	il_free(childnodes);
}

void dualtree_search(kdtree_t* xtree, kdtree_t* ytree,
					 dualtree_callbacks* callbacks) {
	int xnode, ynode;
	il* nodes = il_new(32);
	il* leaves = il_new(32);
	// root nodes.
	xnode = ynode = 0;
	if (KD_IS_LEAF(xtree, xnode))
		il_append(leaves, xnode);
	else
		il_append(nodes, xnode);

	dualtree_recurse(xtree, ytree, nodes, leaves, ynode, callbacks);

	il_free(nodes);
	il_free(leaves);
}

