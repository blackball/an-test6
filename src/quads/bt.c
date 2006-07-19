#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "bt.h"

/*
  The AVL tree portion of this code was adapted from GNU libavl.
*/

// data follows the bt_datablock*.
#define NODE_DATA(node) ((void*)(((bt_datablock*)(node)) + 1))
#define NODE_CHARDATA(node) ((char*)(((bt_datablock*)(node)) + 1))

bt* bt_new(int datasize, int blocksize) {
	bt* tree = calloc(1, sizeof(bt));
	if (!tree) {
		fprintf(stderr, "Failed to allocate a new bt struct: %s\n", strerror(errno));
		return NULL;
	}
	tree->datasize = datasize;
	tree->blocksize = blocksize;
	return tree;
}

int bt_size(bt* tree) {
	return tree->N;
}

int bt_height(bt* tree) {
	bt_node* n = tree->root;
	int h = 0;
	for (n=tree->root; n; h++) {
		if (n->balance > 0)
			n = n->children[1];
		else
			n = n->children[0];
	}
	return h;
}

static void bt_free_node(bt_node* node) {
	if (node->children[0]) {
		bt_free_node(node->children[0]);
		bt_free_node(node->children[1]);
	} else {
		free(node->data);
	}
	free(node);
}

void bt_free(bt* tree) {
	if (tree->root)
		bt_free_node(tree->root);
	free(tree);
}

static void* get_element(bt* tree, bt_node* node, int index) {
	return NODE_CHARDATA(node->data) + index * tree->datasize;
}

static void* first_element(bt_node* n) {
	return NODE_DATA(n->data);
}

static bt_node* bt_new_node(bt* tree) {
	static int nodenum = 1;
	bt_node* n = calloc(1, sizeof(bt_node));
	if (!n) {
		fprintf(stderr, "Failed to allocate a new bt_node: %s\n", strerror(errno));
		return NULL;
	}
#if defined(NODENUM)
	n->nodenum = nodenum++;
#endif
	return n;
}

static bt_node* bt_new_leaf(bt* tree) {
	bt_node* n = bt_new_node(tree);
	if (!n)
		return NULL;
	n->data = malloc(sizeof(bt_datablock) + tree->datasize * tree->blocksize);
	n->data->N = 0;
	return n;
}

static bool bt_node_insert(bt* tree, bt_node* node, void* data, bool unique,
						   compare_func compare, void* overflow) {
	int lower, upper;
	int nshift;
	int index;

	// binary search...
	lower = -1;
	upper = node->data->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, get_element(tree, node, mid));
		if (cmp >= 0)
			lower = mid;
		else
			upper = mid;
	}
	// index to insert at:
	index = lower + 1;

	// duplicate value?
	if (unique && (index > 0))
		if (compare(data, get_element(tree, node, index-1)) == 0)
			return FALSE;

	// shift...
	nshift = node->data->N - index;
	if (node->data->N == tree->blocksize) {
		// this node is full.  insert the element and put the overflowing
		// element in "overflow".
		if (nshift) {
			memcpy(overflow, get_element(tree, node, node->data->N-1), tree->datasize);
			nshift--;
		} else {
			memcpy(overflow, data, tree->datasize);
			return TRUE;
		}
	} else {
		node->data->N++;
		tree->N++;
	}
	memmove(get_element(tree, node, index+1),
			get_element(tree, node, index),
			nshift * tree->datasize);
	// insert...
	memcpy(get_element(tree, node, index), data, tree->datasize);
	return TRUE;
}

// increment the "Nleft" and "Nright" elements of the node's ancestors.
static void increment_nleftright(bt_node** ancestors, int nancestors, bt_node* child) {
	int i;
	bt_node* parent;
	for (i=nancestors-1; i>=0; i--) {
		parent = ancestors[i];
		if (parent->children[0] == child)
			parent->Nleft++;
		if (parent->children[1] == child)
			parent->Nright++;
		child = parent;
	}
}

static bt_node* next_node(bt_node** ancestors, int nancestors,
						  bt_node* child,
						  bt_node** nextancestors, int* nnextancestors) {
	// -first, find the first ancestor of whom we are a left
	//  (grand^n)-child.
	bt_node* parent;
	int i, j;
	for (i=nancestors-1; i>=0; i--) {
		parent = ancestors[i];
		if (parent->children[0] == child)
			break;
		child = parent;
	}
	if (i < 0) {
		// no next node.
		return NULL;
	}

	// we share ancestors from the root to "parent".
	for (j=i; j>=0; j--)
		nextancestors[j] = ancestors[j];
	*nnextancestors = i+1;

	// -next, find the leftmost leaf of the parent's right branch.
	child = parent->children[1];
	while (child->children[0]) {
		nextancestors[(*nnextancestors)++] = child;
	    child = child->children[0];
	}
	return child;
}

#define AVL_MAX_HEIGHT 32

static bool bt_node_contains(bt* tree, bt_node* node, void* data,
							 compare_func compare) {
	int lower, upper;
	lower = -1;
	upper = node->data->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, get_element(tree, node, mid));
		if (cmp == 0) return TRUE;
		if (cmp > 0)
			lower = mid;
		else
			upper = mid;
	}
	// duplicate value?
	if (lower >= 0)
		if (compare(data, get_element(tree, node, lower)) == 0)
			return TRUE;
	return FALSE;
}

bool bt_contains(bt* tree, void* data, compare_func compare) {
	bt_node *n;
	int cmp;
	int dir;

	if (!tree->root)
		return FALSE;

	dir = 0;
	for (n = tree->root; n->children[1]; n = n->children[dir]) {
		cmp = compare(data, first_element(n->children[1]));
		if (cmp == 0)
			return TRUE;
		dir = (cmp > 0);
	}
	return bt_node_contains(tree, n, data, compare);
}

bool bt_insert(bt* tree, void* data, bool unique, compare_func compare) {
	bt_node *y, *z; /* Top node to update balance factor, and parent. */
	bt_node *p, *q; /* Iterator, and parent. */
	bt_node *n;     /* Newly inserted node. */
	bt_node *w;     /* New root of rebalanced subtree. */
	int dir;                /* Direction to descend. */
	bt_node* nq;

	bt_node* ancestors[AVL_MAX_HEIGHT];
	int nancestors = 0;
	unsigned char da[AVL_MAX_HEIGHT]; /* Cached comparison results. */
	int k = 0;              /* Number of cached results. */
	unsigned char overflow[tree->datasize];
	bool rtn;
	bool willfit;
	int cmp;
	bt_node* lastcompared;
	int lastcmp = -1000;

	if (!tree->root) {
		// inserting the first element...
		n = bt_new_leaf(tree);
		tree->root = n;
		bt_node_insert(tree, n, data, unique, compare, NULL);
		return TRUE;
	}

	lastcompared = NULL;
	z = y = tree->root;
	dir = 0;
	for (q = z, p = y; p; q = p, p = p->children[dir]) {
		if (p->children[1]) {
			cmp = compare(data, first_element(p->children[1]));
			lastcmp = cmp;
			lastcompared = p->children[1];
		} else
			cmp = -1;

		if (unique && (cmp == 0))
			return FALSE;

		if (p->balance != 0) {
			z = q;
			y = p;
			k = 0;
		}
		ancestors[nancestors++] = p;
		da[k++] = dir = (cmp > 0);
	}
	nancestors--;

	// will this element fit in the current node?
	willfit = (q->data->N < tree->blocksize);
	if (willfit) {
		rtn = bt_node_insert(tree, q, data, unique, compare, overflow);
		// duplicate value?
		if (!rtn)
			return rtn;
		increment_nleftright(ancestors, nancestors, q);
		return TRUE;
	}

	if (lastcompared != q)
		cmp = compare(data, first_element(q));
	else
		cmp = lastcmp;

	if (cmp > 0) {
		// insert the new element into this node and shuffle the
		// overflowing element (which may be the new element)
		// into the next node, if it exists, or a new node.
		bt_node* nextnode;
		bt_node* nextancestors[AVL_MAX_HEIGHT];
		int nnextancestors;

		/*
		  HACK - should we traverse the tree looking for the next node,
		  or just take the right sibling if we're the left child of a
		  balanced parent?
		*/

		rtn = bt_node_insert(tree, q, data, unique, compare, overflow);
		if (!rtn)
			// duplicate value.
			return rtn;
		nextnode = next_node(ancestors, nancestors, q, nextancestors, &nnextancestors);
		if (nextnode && (nextnode->data->N < tree->blocksize)) {
			// there's room; insert the element!
			rtn = bt_node_insert(tree, nextnode, overflow, unique, compare, NULL);
			increment_nleftright(nextancestors, nnextancestors, nextnode);
			/*
			  printf("inserted element in next node (%i)\n", nextnode->nodenum);
			  bt_print(tree, print_int);
			  printf("\n");
			*/
			return rtn;
		}

		// no room (or no next node); add a new node to the right to hold
		// the overflowed data.
		dir = 1;
		data = overflow;

	} else {
		// add a new node to the left.
		dir = 0;
	}

	// create a new node to hold this element.
	n = q->children[dir] = bt_new_leaf(tree);
	if (!n)
		return FALSE;

	// create a new node to hold q's data.
	nq = bt_new_node(tree);
	if (!nq)
		return FALSE;
	q->children[1-dir] = nq;
	nq->data = q->data;
	if (dir) {
		q->Nleft = nq->data->N;
		q->Nright = 1;
	} else {
		q->Nright = nq->data->N;
		q->Nleft = 1;
		q->data = n->data;
	}

	rtn = bt_node_insert(tree, n, data, unique, compare, NULL);
	if (!rtn)
		return FALSE;
	increment_nleftright(ancestors, nancestors, q);

	if (!y)
		return TRUE;

	for (p = y, k = 0; p != q; p = p->children[da[k]], k++)
		if (da[k] == 0)
			p->balance--;
		else
			p->balance++;

	/*
	  printf("After updating balance:\n");
	  bt_print_structure(tree, print_int);
	  printf("\n");
	  printf("Node y = %i.\n", y->nodenum);
	*/

	if (y->balance == -2) {
		bt_node *x = y->children[0];
		if (x->balance == -1) {
			w = x;
			y->children[0] = x->children[1];
			x->children[1] = y;
			x->balance = y->balance = 0;

			y->Nleft  = x->Nright;
			x->Nright = y->Nleft + y->Nright;
			y->data = y->children[0]->data;

        } else {
			assert (x->balance == 1);
			w = x->children[1];
			x->children[1] = w->children[0];
			w->children[0] = x;
			y->children[0] = w->children[1];
			w->children[1] = y;

			x->Nright = w->Nleft;
			w->Nleft  = x->Nleft + x->Nright;
			y->Nleft  = w->Nright;
			w->Nright = y->Nleft + y->Nright;

			w->data = w->children[0]->data;
			y->data = y->children[0]->data;

			if (w->balance == -1) {
				x->balance = 0;
				y->balance = 1;
			} else if (w->balance == 0)
				x->balance = y->balance = 0;
			else {
				x->balance = -1;
				y->balance = 0;
			}
			w->balance = 0;
        }
    } else if (y->balance == 2) {
		bt_node *x = y->children[1];
		if (x->balance == 1) {
			w = x;
			y->children[1] = x->children[0];
			x->children[0] = y;
			x->balance = y->balance = 0;

			y->Nright = x->Nleft;
			x->Nleft  = y->Nleft + y->Nright;

			x->data = x->children[0]->data;

        } else {
			assert (x->balance == -1);
			w = x->children[0];
			x->children[0] = w->children[1];
			w->children[1] = x;
			y->children[1] = w->children[0];
			w->children[0] = y;

			x->Nleft  = w->Nright;
			w->Nright = x->Nleft + x->Nright;
			y->Nright = w->Nleft;
			w->Nleft  = y->Nleft + y->Nright;

			x->data = x->children[0]->data;
			w->data = w->children[0]->data;

			if (w->balance == 1) {
				x->balance = 0;
				y->balance = -1;
			} else if (w->balance == 0)
				x->balance = y->balance = 0;
			else {
				x->balance = 1;
				y->balance = 0;
			}
			w->balance = 0;
        }
    } else
		goto finished;

	if (y == tree->root)
		tree->root = w;
	else {
		if (y == z->children[0]) {
			z->children[0] = w;
			z->Nleft = w->Nleft + w->Nright;
			z->data = w->data;
		} else {
			z->children[1] = w;
			z->Nright = w->Nleft + w->Nright;
		}
	}

 finished:
	/*
	  printf("After rotating the tree:\n");
	  bt_print_structure(tree, print_int);
	  printf("\n");
	*/
	return TRUE;
}

void* bt_access(bt* tree, int index) {
	bt_node* n = tree->root;
	int offset = index;
	while (n->children[0]) {
		if (offset >= n->Nleft) {
			// right child
			offset -= n->Nleft;
			n = n->children[1];
		} else
			// left child
			n = n->children[0];
	}
	return NODE_CHARDATA(n->data) + offset * tree->datasize;
}

static void bt_print_node(bt* tree, bt_node* node, char* indent,
						  void (*print_element)(void* val)) {
	if (node->children[0] || node->children[1]) {
		char* subind;
		printf("%s", indent);
#if defined(NODENUM)
		printf("Node %i.  ", node->nodenum);
#endif
		printf("Nleft=%i, Nright=%i (sum=%i), balance %i.\n",
			   node->Nleft, node->Nright, node->Nleft + node->Nright, node->balance);
		subind = malloc(strlen(indent) + 2 + 1);
		sprintf(subind, "%s  ", indent);
		if (node->children[0]) {
			printf("%sLeft child:\n", indent);
			bt_print_node(tree, node->children[0], subind, print_element);
		} else
			printf("%sNo left child.\n", indent);

		if (node->children[1]) {
			printf("%sRight child:\n", indent);
			bt_print_node(tree, node->children[1], subind, print_element);
		} else
			printf("%sNo right child.\n", indent);
		free(subind);
	} else {
		int i;
		if (node->data) {
			printf("%s", indent);
#if defined(NODENUM)
			printf("Node %i.  ", node->nodenum);
#endif
			printf("Leaf: N=%i.\n", node->data->N);
			if (print_element) {
				printf("%s[ ", indent);
				for (i=0; i<node->data->N; i++) {
					print_element(get_element(tree, node, i));
				}
				printf("]\n");
			}
		} else {
			printf("%sEmpty node.\n", indent);
		}
	}
}

void bt_print(bt* tree, void (*print_element)(void* val)) {
	printf("  bt: N=%i, datasize=%i, blocksize=%i.\n", tree->N,
		   tree->datasize, tree->blocksize);
	if (!tree->root) {
		printf("  Empty tree.\n");
		return;
	}
	bt_print_node(tree, tree->root, "  ", print_element);
}

static void bt_print_struct_node(bt* tree, bt_node* node, char* indent,
								 void (*print_element)(void* val)) {

	printf("%s", indent);
#if defined(NODENUM)
	printf("Node %i.  ", node->nodenum);
#endif
	printf("(bal %i)", node->balance);
	if (node->children[0] || node->children[1]) {
		char* subind;
		printf("\n");
		subind = malloc(strlen(indent) + 3 + 1);
		sprintf(subind, "%s|--", indent);
		if (node->children[0]) {
			bt_print_struct_node(tree, node->children[0], subind, print_element);
		} else
			printf("%s|--x\n", indent);

		if (node->children[1]) {
			bt_print_struct_node(tree, node->children[1], subind, print_element);
		} else
			printf("%s|--x\n", indent);
	} else {
		int i;
		if (node->data) {
			if (print_element) {
				printf(" [ ");
				for (i=0; i<node->data->N; i++) {
					print_element(get_element(tree, node, i));
				}
				printf("]");
			}
		}
		printf("\n");
	}
}

void bt_print_structure(bt* tree, void (*print_element)(void* val)) {
	bt_print_struct_node(tree, tree->root, "   ", print_element);
}

