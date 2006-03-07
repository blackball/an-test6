#include "dualtree.h"

#define BAIL_OUT() printf("Bailing out: file %s, line %i", __FILE__, __LINE__)

/*
  The world's (10^6 + 1)th linked-list implementation...
*/
struct list_node
{
	void* data;
	struct list_node* next;
};
typedef struct list_node list_node;

struct list
{
	list_node* head;
	list_node* tail;
};
typedef struct list list;

#define list_allocate_node malloc
#define list_allocate malloc
#define list_free free
#define list_free_node free

void list_init(list* l)
{
	l->head = NULL;
	l->tail = NULL;
}

void list_prepend(list* l, void* data)
{
	list_node* n = list_allocate_node(sizeof(list_node));
	n->data = data;
	n->next = l->head;
	if (!l->head) {
		// first node in the list.
		l->tail = n;
	}
	l->head = n;
}

void list_append(list* l, void* data)
{
	list_node* n = list_allocate_node(sizeof(list_node));
	n->data = data;
	n->next = NULL;
	if (l->tail) {
		l->tail->next = n;
	} else {
		// first node in the list
		l->head = n;
	}
	l->tail = n;
}

/* frees all the nodes in this list. */
void list_free_nodes(list* l)
{
	list_node* n;
	for (n = l->head; n;) {
		list_node* tmp = n->next;
		list_free_node(n);
		n = tmp;
	}
	l->head = NULL;
	l->tail = NULL;
}

/*
  The actual dual-tree stuff.
*/

/*
  At each step of the recursion, we have a query node and a list of
  candidate search nodes.
*/
void dualtree_recurse(list* nodes, list* leaves,
                       node* query, dualtree_callbacks* callbacks) {

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

	list_node* e;

	list* childnodes;
	list* childleaves;
	list_node* leavesoldtail;

	decision_function decision;
	void* decision_extra;

	// if the query node is a leaf, run the result function on each
	// search node.
	if (node_is_leaf(query)) {
		start_of_results_function start = callbacks->start_results;
		end_of_results_function end = callbacks->end_results;
		result_function result = callbacks->result;
		void* result_extra = callbacks->result_extra;

		if (start) {
			start(callbacks->start_extra, query);
		}

		// non-leaf nodes
		if (result) {
		    for (e = nodes->head; e; e = e->next) {
			node * sn = (node*)e->data;
			result(result_extra, sn, query);
		    }
		    // leaf nodes
		    for (e = leaves->head; e; e = e->next) {
			node * sn = (node*)e->data;
			result(result_extra, sn, query);
		    }
		}

		if (end) {
			end(callbacks->end_extra, query);
		}

		return ;
	}

	// if there are search leaves but no search nodes, run the result
	// function on each leaf.  (Note that the query node is not a leaf!)
	if (!nodes->head) {
	    start_of_results_function start = callbacks->start_results;
	    end_of_results_function   end   = callbacks->end_results;
	    result_function result = callbacks->result;
	    void* result_extra = callbacks->result_extra;

		if (start) {
			start(callbacks->start_extra, query);
		}

		// leaf nodes
		if (result) {
		    for (e = leaves->head; e; e = e->next) {
			node * sn = (node*)e->data;
			result(result_extra, sn, query);
		    }
		}

		if (end) {
			end(callbacks->end_extra, query);
		}

		return ;
	}

	childnodes = list_allocate(sizeof(list));
	childleaves = leaves;
	leavesoldtail = leaves->tail;
	decision = callbacks->decision;
	decision_extra = callbacks->decision_extra;

	list_init(childnodes);

	for (e = nodes->head; e; e = e->next) {
		node * sn = (node*)e->data;
		if (decision(decision_extra, sn, query)) {

			// since this node was in the "searchnodes" list, it should
			// NOT be a leaf, so its children should be non-NULL.
			if ((!sn->child1) || (!sn->child2))
				BAIL_OUT();

			// append the children to the appropriate lists:
			list_append((node_is_leaf(sn->child1) ? childleaves : childnodes),
			            sn->child1);
			list_append((node_is_leaf(sn->child2) ? childleaves : childnodes),
			            sn->child2);

		}
	}

	// recurse on the children!
	dualtree_recurse(childnodes, childleaves, query->child1, callbacks);

	dualtree_recurse(childnodes, childleaves, query->child2, callbacks);

	// put the "leaves" list back the way it was...
	{
	    // create a temp list containing all the elements we appended...
	    if (leavesoldtail && leavesoldtail->next) {
		list fakelist;
		list_init(&fakelist);

		fakelist.head = leavesoldtail->next;
		fakelist.tail = childleaves->tail;
		// free all the nodes in the temp list...
		list_free_nodes(&fakelist);
	    } else if (!leavesoldtail) {
		// there was no leaflist; remove everything from the childleaves list.
		list_free_nodes(childleaves);
		leaves->head = NULL;
	    }
	    // put the leaves list back the way it was.
	    leaves->tail = leavesoldtail;
	    if (leavesoldtail) {
		leavesoldtail->next = NULL;
	    }
	    childleaves = NULL;
	}

	list_free_nodes(childnodes);
	list_free(childnodes);
}

void dualtree_search(kdtree* search, kdtree* query,
					  dualtree_callbacks* callbacks) {
	list sn;
	list sl;
	list_init(&sl);
	list_init(&sn);
	list_prepend((node_is_leaf(search->root) ? &sl : &sn), search->root);

	dualtree_recurse(&sn, &sl, query->root, callbacks);

	list_free_nodes(&sn);
	list_free_nodes(&sl);
}

