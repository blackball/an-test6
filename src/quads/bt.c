#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "bt.h"

#define FALSE 0
#define TRUE 1

// DEBUG
static void print_int(void* v1) {
	int i = *(int*)v1;
	printf("%i ", i);
}


// data follows the bl_node*.
#define NODE_DATA(node) ((void*)(((bl_node*)(node)) + 1))
#define NODE_CHARDATA(node) ((char*)(((bl_node*)(node)) + 1))

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
	static int nodenum = 1;
	bt_node* n = calloc(1, sizeof(bt_node));
	if (!n) {
		fprintf(stderr, "Failed to allocate a new bt_node: %s\n", strerror(errno));
		return NULL;
	}
	n->nodenum = nodenum++;
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
	int lastcmp;

	if (!tree->root) {
		// inserting the first element...
		n = bt_new_leaf(tree);
		tree->root = n;
		bt_node_insert(tree, n, data, unique, compare, NULL);
		tree->root->balance = 0;
		return TRUE;
	}

	z = y = tree->root;
	dir = 0;
	for (q = z, p = y; p; q = p, p = p->children[dir]) {
		if (p->children[1]) {
			cmp = compare(data, first_element(p->children[1]));
			lastcmp = cmp;
			lastcompared = p->children[1];
		} else
			cmp = -1;
		if (p->balance != 0) {
			z = q;
			y = p;
			k = 0;
		}
		ancestors[nancestors++] = p;
		//da[k++] = dir = (cmp >= 0);
		da[k++] = dir = (cmp > 0);
	}
	nancestors--;

	// will this element fit in the current node?
	willfit = (q->node->N < tree->blocksize);
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
		if (nextnode && (nextnode->node->N < tree->blocksize)) {
			// there's room; insert the element!

			printf("inserting element in next node (%i)\n", nextnode->nodenum);

			rtn = bt_node_insert(tree, nextnode, overflow, unique, compare, NULL);
			increment_nleftright(nextancestors, nnextancestors, nextnode);

			bt_print(tree, print_int);
			printf("\n");

			return TRUE;
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
	nq->node = q->node;
	q->node = q->children[0]->node;
	if (dir) {
		q->Nleft = nq->node->N;
		q->Nright = 1;
	} else {
		q->Nright = nq->node->N;
		q->Nleft = 1;
	}

	bt_node_insert(tree, n, data, unique, compare, NULL);
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
			y->node = y->children[0]->node;

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

			w->node = w->children[0]->node;
			y->node = y->children[0]->node;

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

			x->node = x->children[0]->node;

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

			x->node = x->children[0]->node;
			w->node = w->children[0]->node;

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

	//z->children[y != z->children[0]] = w;
	if (y == tree->root) {
		tree->root = w;
	} else {
		if (y == z->children[0]) {
			z->children[0] = w;
			z->Nleft = w->Nleft + w->Nright;
			z->node = w->node;
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
	return NODE_CHARDATA(n->node) + offset * tree->datasize;
}

static void bt_print_node(bt* tree, bt_node* node, char* indent,
						  void (*print_element)(void* val)) {
	if (node->children[0] || node->children[1]) {
		char* subind;
		printf("%sNode %i.  Nleft=%i, Nright=%i (sum=%i), balance %i.\n",
			   indent, 
			   node->nodenum,
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
	} else {
		int i;
		if (node->node) {
			printf("%sNode %i: Leaf: N=%i.\n", indent, node->nodenum, node->node->N);
			if (print_element) {
				printf("%s[ ", indent);
				for (i=0; i<node->node->N; i++) {
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
	printf("%s%i (%i)", indent, node->nodenum, node->balance);
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
		if (node->node) {
			if (print_element) {
				printf(" [ ");
				for (i=0; i<node->node->N; i++) {
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

