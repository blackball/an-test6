#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "bt.h"

#define FALSE 0
#define TRUE 1

// data follows the bl_node*.
#define NODE_DATA(node) ((void*)(((bl_node*)(node)) + 1))
#define NODE_CHARDATA(node) ((char*)(((bl_node*)(node)) + 1))
//#define NODE_INTDATA(node) ((int*)(((bl_node*)(node)) + 1))

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

static void* get_element(bt* tree, bt_node* node, int index) {
	return NODE_CHARDATA(node->node) + index * tree->datasize;
}

static void* first_element(bt_node* n) {
	return NODE_DATA(n->node);
}

static bt_node* bt_new_node(bt* tree) {
	bt_node* n = calloc(1, sizeof(bt_node));
	if (!n) {
		fprintf(stderr, "Failed to allocate a new bt_node: %s\n", strerror(errno));
		return NULL;
	}
	return n;
}

static bt_node* bt_new_leaf(bt* tree) {
	bt_node* n = bt_new_node(tree);
	if (!n)
		return NULL;
	n->node = malloc(sizeof(bl_node) + tree->datasize * tree->blocksize);
	n->node->N = 0;
	return n;
}

static bool bt_node_insert(bt* tree, bt_node* node, void* data, bool unique,
						   compare_func compare, void* overflow) {
	int lower, upper;
	int nshift;
	int index;

	// binary search...
	lower = -1;
	upper = node->node->N;
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
	nshift = node->node->N - index;
	if (node->node->N == tree->blocksize) {
		// this node is full.  insert the element and put the overflowing
		// element in "overflow".
		if (index == node->node->N)
			memcpy(overflow, data, tree->datasize);
		else {
			memcpy(overflow, get_element(tree, node, node->node->N-1), tree->datasize);
			nshift--;
		}
	} else {
		node->node->N++;
		tree->N++;
	}
	memmove(get_element(tree, node, index+1), get_element(tree, node, index),
			nshift * tree->datasize);
	// insert...
	memcpy(get_element(tree, node, index), data, tree->datasize);
	return TRUE;
}

// increment the "Nleft" element of any ancestor of whom we are a left
// descendant.
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

#define AVL_MAX_HEIGHT 32

bool bt_insert(bt* tree, void* data, bool unique, compare_func compare) {
	bt_node *y, *z; /* Top node to update balance factor, and parent. */
	bt_node *p, *q; /* Iterator, and parent. */
	bt_node *n;     /* Newly inserted node. */
	bt_node *w;     /* New root of rebalanced subtree. */
	int dir;                /* Direction to descend. */

	bt_node* ancestors[AVL_MAX_HEIGHT];
	unsigned char da[AVL_MAX_HEIGHT]; /* Cached comparison results. */
	int k = 0;              /* Number of cached results. */
	unsigned char overflow[tree->datasize];
	bool rtn;
	bool willfit;

	if (!tree->root) {
		// inserting the first element...
		n = bt_new_leaf(tree);
		tree->root = n;
		bt_node_insert(tree, n, data, unique, compare, NULL);
		tree->root->balance = -1;
		return TRUE;
	}

	z = y = tree->root;
	dir = 0;
	for (q = z, p = y; p; q = p, p = p->children[dir]) {
		int cmp = compare(data, first_element(p));
		if (p->balance != 0) {
			z = q, y = p, k = 0;
		}
		ancestors[k] = p;
		da[k++] = dir = (cmp > 0);
    }

	// will this element fit in the current node?
	willfit = (q->node->N < tree->blocksize);
	rtn = bt_node_insert(tree, q, data, unique, compare, overflow);
	// duplicate value?
	if (!rtn)
		return rtn;
	if (willfit) {
		increment_nleftright(ancestors, k, q);
		return rtn;
	}

	// is there room in the next node?
	{
		// find the next node:
		// -first, find the first ancestor of whom we are a left
		//  (grand^n)-child.
		bt_node* granny;
		bt_node* child;
		bt_node* nextnode;
		int i;
		child = q;
		for (i=k-1; i>=0; i--) {
			granny = ancestors[i];
			if (granny->children[0] == child)
				break;
			child = granny;
		}
		if (i < 0) {
			// no next node.
			nextnode = NULL;
		} else {
			// -next, find the leftmost leaf.
			nextnode = granny;
			while (nextnode->children[0])
				nextnode = nextnode->children[0];
		}
		if (nextnode && (nextnode->node->N < tree->blocksize)) {
			// there's room; insert the element!
			rtn = bt_node_insert(tree, nextnode, overflow, unique, compare, NULL);
			increment_nleftright(ancestors, k, nextnode);
		}
	}

	// create a new node to hold this element.
	n = q->children[dir] = bt_new_leaf(tree);
	if (!n)
		return FALSE;

	// create a new node to hold q's data.
	{
		bt_node* nq = bt_new_node(tree);
		q->children[1-dir] = nq;
		nq->node = q->node;
		q->node = NULL;
		q->Nleft = nq->node->N;
	}

	bt_node_insert(tree, n, overflow, unique, compare, NULL);
	increment_nleftright(ancestors, k, n);

	if (!y)
		return TRUE;

	for (p = y, k = 0; p != n; p = p->children[da[k]], k++)
		if (da[k] == 0)
			p->balance--;
		else
			p->balance++;

	if (y->balance == -2) {
		bt_node *x = y->children[0];
		if (x->balance == -1) {
			w = x;
			y->children[0] = x->children[1];
			x->children[1] = y;
			x->balance = y->balance = 0;

			y->Nleft  = x->Nright;
			x->Nright = y->Nleft + y->Nright;
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
		return TRUE;

	//z->children[y != z->children[0]] = w;
	if (y == z->children[0]) {
		z->children[0] = w;
		z->Nleft = w->Nleft + w->Nright;
	} else {
		z->children[1] = w;
		z->Nright = w->Nleft + w->Nright;
	}

	return TRUE;
}

void* bt_access(bt* tree, int index) {
	bt_node* n = tree->root;
	int offset = index;
	while (!n->node) {
		if (offset > n->Nleft) {
			// right child
			offset -= n->Nleft;
			n = n->children[1];
		} else
			// left child
			n = n->children[0];
	}
	return NODE_CHARDATA(n->node) + offset * tree->datasize;
}

static void bt_print_node(bt* tree, bt_node* node, char* indent,
						  void (*print_element)(void* val)) {
	if (node->children[0] || node->children[1]) {
		char* subind;
		printf("%sNleft=%i, Nright=%i (sum=%i), balance %i.\n",
			   indent, node->Nleft, node->Nright, node->Nleft + node->Nright, node->balance);
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
	} else {
		int i;
		if (node->node) {
			printf("%sLeaf: N=%i.\n", indent, node->node->N);
			printf("%s[ ", indent);
			for (i=0; i<node->node->N; i++) {
				print_element(get_element(tree, node, i));
			}
			printf("]\n");
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
