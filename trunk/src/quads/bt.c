#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "bt.h"

/*
  The AVL tree portion of this code was adapted from GNU libavl.
*/
#define AVL_MAX_HEIGHT 32

// data follows the bt_datablock*.
/*
  #define NODE_DATA     (leaf) ((void*)(((bt_leaf*)(node)) + 1))
  #define NODE_CHARDATA (leaf) ((char*)(((bt_leaf*)(node)) + 1))
*/
static Const void* NODE_DATA(bt_leaf* leaf) {
	return leaf+1;
}
static Const char* NODE_CHARDATA(bt_leaf* leaf) {
	return (char*)(leaf+1);
}

static Pure bool isleaf(bt_node* node) {
	return node->leaf.isleaf;
}

static Pure int node_N(bt_node* node) {
	if (isleaf(node))
		return node->leaf.N;
	return node->branch.N;
}

static Pure int sum_childN(bt_branch* branch) {
	return node_N(branch->children[0]) + node_N(branch->children[1]);
}

static Pure bt_node* getchild(bt_node* node, int i) {
	if (isleaf(node)) return NULL;
	return node->branch.children[i];
}

static Pure bt_node* getleftchild(bt_node* node) {
	if (isleaf(node)) return NULL;
	return node->branch.children[0];
}

static Pure bt_node* getrightchild(bt_node* node) {
	if (isleaf(node)) return NULL;
	return node->branch.children[1];
}

static Pure bt_leaf* firstleaf(bt_node* node) {
	if (isleaf(node)) return &node->leaf;
	return node->branch.firstleaf;
}

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
	bt_node* n;
	int h = 0;
	n = tree->root;
	if (!n) return 0;
	if (isleaf(n)) return 1;
	for (; n; h++) {
		if (n->branch.balance > 0)
			n = getrightchild(n);
		else
			n = getleftchild(n);
	}
	return h;
}

static int bt_count_leaves_rec(bt_node* node) {
	if (isleaf(node)) return 1;
	else return
			 bt_count_leaves_rec(node->branch.children[0]) +
			 bt_count_leaves_rec(node->branch.children[1]);
}

int bt_count_leaves(bt* tree) {
	return bt_count_leaves_rec(tree->root);
}

static void bt_free_node(bt_node* node) {
	if (!isleaf(node)) {
		bt_free_node(getleftchild(node));
		bt_free_node(getrightchild(node));
	}
	free(node);
}

void bt_free(bt* tree) {
	if (tree->root)
		bt_free_node(tree->root);
	free(tree);
}

static Pure void* get_element(bt* tree, bt_leaf* leaf, int index) {
	return NODE_CHARDATA(leaf) + index * tree->datasize;
}

static Pure void* first_element(bt_node* n) {
	if (isleaf(n)) return NODE_DATA(&n->leaf);
	else return NODE_DATA(n->branch.firstleaf);
}

static Malloc bt_node* bt_new_branch(bt* tree) {
#if NODENUM
	static int nodenum = 1;
#endif
	bt_node* n = calloc(1, sizeof(bt_node));
	if (!n) {
		fprintf(stderr, "Failed to allocate a new bt_node: %s\n", strerror(errno));
		return NULL;
	}
#if NODENUM
	n->nodenum = nodenum++;
#endif
	return n;
}

static Malloc bt_node* bt_new_leaf(bt* tree) {
	bt_node* n = malloc(sizeof(bt_leaf) + tree->datasize * tree->blocksize);
	if (!n) {
		fprintf(stderr, "Failed to allocate a new bt_node: %s\n", strerror(errno));
		return NULL;
	}
	n->leaf.isleaf = 1;
	n->leaf.N = 0;
	return n;
}

static bool bt_leaf_insert(bt* tree, bt_leaf* leaf, void* data, bool unique,
						   compare_func compare, void* overflow) {
	int lower, upper;
	int nshift;
	int index;

	// binary search...
	lower = -1;
	upper = leaf->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, get_element(tree, leaf, mid));
		if (!cmp && unique) return FALSE;
		if (cmp >= 0)
			lower = mid;
		else
			upper = mid;
	}
	// index to insert at:
	index = lower + 1;

	// duplicate value?
	if (unique && (index > 0))
		if (compare(data, get_element(tree, leaf, index-1)) == 0)
			return FALSE;

	// shift...
	nshift = leaf->N - index;
	if (leaf->N == tree->blocksize) {
		// this node is full.  insert the element and put the overflowing
		// element in "overflow".
		if (nshift) {
			memcpy(overflow, get_element(tree, leaf, leaf->N-1), tree->datasize);
			nshift--;
		} else {
			memcpy(overflow, data, tree->datasize);
			return TRUE;
		}
	} else {
		leaf->N++;
		tree->N++;
	}
	memmove(get_element(tree, leaf, index+1),
			get_element(tree, leaf, index),
			nshift * tree->datasize);
	// insert...
	memcpy(get_element(tree, leaf, index), data, tree->datasize);
	return TRUE;
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
		if (parent->branch.children[0] == child)
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
	child = parent->branch.children[1];
	while (!isleaf(child)) {
		nextancestors[(*nnextancestors)++] = child;
	    child = child->branch.children[0];
	}
	return child;
}

// Pure?
static bool bt_leaf_contains(bt* tree, bt_leaf* leaf, void* data,
							 compare_func compare) {
	int lower, upper;
	lower = -1;
	upper = leaf->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, get_element(tree, leaf, mid));
		if (cmp == 0) return TRUE;
		if (cmp > 0)
			lower = mid;
		else
			upper = mid;
	}
	// duplicate value?
	if (lower >= 0)
		if (compare(data, get_element(tree, leaf, lower)) == 0)
			return TRUE;
	return FALSE;
}

// Pure?
bool bt_contains(bt* tree, void* data, compare_func compare) {
	bt_node *n;
	int cmp;
	int dir;

	if (!tree->root)
		return FALSE;

	dir = 0;
	for (n = tree->root; !isleaf(n); n = getchild(n, dir)) {
		cmp = compare(data, first_element(n->branch.children[1]));
		if (cmp == 0)
			return TRUE;
		dir = (cmp > 0);
	}
	return bt_leaf_contains(tree, &n->leaf, data, compare);
}

static void increment_n(bt_node** ancestors, int nancestors) {
	int i;
	for (i=0; i<nancestors; i++)
		ancestors[i]->branch.N++;
}

static void update_firstleaf(bt_node** ancestors, int nancestors,
							 bt_node* child, bt_leaf* leaf) {
	int i;
	for (i=nancestors-1; i>=0; i--) {
		if (ancestors[i]->branch.children[0] != child)
			break;
		ancestors[i]->branch.firstleaf = leaf;
	}
}

bool bt_insert(bt* tree, void* data, bool unique, compare_func compare) {
	bt_node *y, *z; /* Top node to update balance factor, and parent. */
	bt_node *p, *q; /* Iterator, and parent. */
	bt_node *n;     /* Newly inserted node. */
	bt_node *w;     /* New root of rebalanced subtree. */
	int dir;                /* Direction to descend. */
	bt_node* np;

	bt_node* ancestors[AVL_MAX_HEIGHT];
	int nancestors = 0;
	unsigned char da[AVL_MAX_HEIGHT]; /* Cached comparison results. */
	int k = 0;              /* Number of cached results. */
	unsigned char overflow[tree->datasize];
	bool rtn;
	bool willfit;
	int cmp;

	if (!tree->root) {
		// inserting the first element...
		n = bt_new_leaf(tree);
		tree->root = n;
		bt_leaf_insert(tree, &n->leaf, data, unique, compare, NULL);
		return TRUE;
	}

	z = y = tree->root;
	dir = 0;

	for (q = z, p = y;
		 !isleaf(p);
		 q = p, p = p->branch.children[dir]) {
		cmp = compare(data, first_element(p->branch.children[1]));
		if (unique && (cmp == 0))
			return FALSE;
		if (p->branch.balance != 0) {
			z = q;
			y = p;
			k = 0;
		}
		ancestors[nancestors++] = p;
		da[k++] = dir = (cmp > 0);
	}
	cmp = compare(data, first_element(p));

	// will this element fit in the current node?
	willfit = (p->leaf.N < tree->blocksize);
	if (willfit) {
		rtn = bt_leaf_insert(tree, &p->leaf, data, unique, compare, overflow);
		// duplicate value?
		if (!rtn)
			return rtn;
		increment_n(ancestors, nancestors);
		return TRUE;
	}

	da[k++] = (cmp > 0);

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

		rtn = bt_leaf_insert(tree, &p->leaf, data, unique, compare, overflow);
		if (!rtn)
			// duplicate value.
			return rtn;
		nextnode = next_node(ancestors, nancestors, p, nextancestors, &nnextancestors);
		assert(!nextnode || isleaf(nextnode));
		if (nextnode && (nextnode->leaf.N < tree->blocksize)) {
			// there's room; insert the element!
			rtn = bt_leaf_insert(tree, &nextnode->leaf, overflow, unique, compare, NULL);
			increment_n(nextancestors, nnextancestors);
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

	// we have "q", the parent node, and "p", the existing leaf node which is
	// full.  we create a new branch node, "np", to take "p"'s place.
	// we move "p" down to be the child (1-dir) of "np", and create a new
	// leaf node, "n", to be child (dir) of "np".

	// create "n", a new leaf node to hold this element.
	n = bt_new_leaf(tree);
	if (!n)
		return FALSE;
	rtn = bt_leaf_insert(tree, &n->leaf, data, unique, compare, NULL);
	if (!rtn) {
		free(n);
		return FALSE;
	}

	// create "np", a new branch node to take p's place.
	np = bt_new_branch(tree);
	if (!np)
		return FALSE;
	np->branch.children[dir] = n;
	np->branch.children[1-dir] = p;
	np->branch.N = p->leaf.N + 1;
	np->branch.firstleaf = &(np->branch.children[0]->leaf);
	if (q->branch.children[0] == p) {
		q->branch.children[0] = np;
		if (!dir)
			update_firstleaf(ancestors, nancestors, np, &n->leaf);
	} else if (q->branch.children[1] == p) // need this because it could be that p = q = root.
		q->branch.children[1] = np;

	if (p == tree->root)
		tree->root = np;

	increment_n(ancestors, nancestors);

	if (!y || isleaf(y))
		return TRUE;

	for (p = y, k = 0;
		 p != np;
		 p = p->branch.children[da[k]], k++)
		if (da[k] == 0)
			p->branch.balance--;
		else
			p->branch.balance++;
	
	if (y->branch.balance == -2) {
		bt_node *x = y->branch.children[0];
		if (x->branch.balance == -1) {
			/*
			   y
			   |- x
			   |  |- x0
			   |  |- x1
			   |- y1

			   becomes

			   x
			   |- x0
			   |- y
			   .  |- x1
			   .  |- y1
			*/
			w = x;
			y->branch.children[0] = x->branch.children[1];
			x->branch.children[1] = y;
			x->branch.balance = y->branch.balance = 0;

			y->branch.firstleaf = firstleaf(y->branch.children[0]);
			y->branch.N = sum_childN(&y->branch);
			x->branch.N = sum_childN(&x->branch);

        } else {
			/*
			  y
			  |- x
			  |  |- x0
			  |  |- x1 (= w)
			  |     |- x10
			  |     |- x11
			  |- y1

			  becomes

			  x1
			  |- x
			  |  |- x0
			  |  |- x10
			  |- y
			  .  |- x11
			  .  |- y1
			*/

			assert (x->branch.balance == 1);
			w = x->branch.children[1];
			x->branch.children[1] = w->branch.children[0];
			w->branch.children[0] = x;
			y->branch.children[0] = w->branch.children[1];
			w->branch.children[1] = y;

			y->branch.firstleaf = firstleaf(y->branch.children[0]);
			w->branch.firstleaf = x->branch.firstleaf;

			y->branch.N = sum_childN(&y->branch);
			x->branch.N = sum_childN(&x->branch);
			w->branch.N = sum_childN(&w->branch);

			if (w->branch.balance == -1) {
				x->branch.balance = 0;
				y->branch.balance = 1;
			} else if (w->branch.balance == 0)
				x->branch.balance = y->branch.balance = 0;
			else {
				x->branch.balance = -1;
				y->branch.balance = 0;
			}
			w->branch.balance = 0;
        }
    } else if (y->branch.balance == 2) {
		bt_node *x = y->branch.children[1];
		if (x->branch.balance == 1) {
			/*
			  y
			  |- y0
			  |- x
			  .   |- x0
			  .   |- x1

			  becomes

			  x
			  |- y
			  |  |- y0
			  |  |- x0
			  |- x1
			*/
			w = x;
			y->branch.children[1] = x->branch.children[0];
			x->branch.children[0] = y;
			x->branch.balance = y->branch.balance = 0;

			x->branch.firstleaf = y->branch.firstleaf;

			y->branch.N = sum_childN(&y->branch);
			x->branch.N = sum_childN(&x->branch);

        } else {
			/*
			  y
			  |- y0
			  |- x
			  .  |- x0
			  .  |  |- x00
			  .  |  |- x01
			  .  |- x1

			  becomes

			  x0
			  |- y
			  |  |- y0
			  |  |- x00
			  |- x
			  .  |- x01
			  .  |- x1
			*/
			assert (x->branch.balance == -1);
			w = x->branch.children[0];
			x->branch.children[0] = w->branch.children[1];
			w->branch.children[1] = x;
			y->branch.children[1] = w->branch.children[0];
			w->branch.children[0] = y;

			y->branch.N = sum_childN(&y->branch);
			x->branch.N = sum_childN(&x->branch);
			w->branch.N = sum_childN(&w->branch);

			x->branch.firstleaf = firstleaf(x->branch.children[0]);
			w->branch.firstleaf = y->branch.firstleaf;

			if (w->branch.balance == 1) {
				x->branch.balance = 0;
				y->branch.balance = -1;
			} else if (w->branch.balance == 0)
				x->branch.balance = y->branch.balance = 0;
			else {
				x->branch.balance = 1;
				y->branch.balance = 0;
			}
			w->branch.balance = 0;
        }
    } else
		goto finished;

	if (y == tree->root)
		tree->root = w;
	else {
		if (y == z->branch.children[0]) {
			z->branch.children[0] = w;
			z->branch.firstleaf = w->branch.firstleaf;
		} else
			z->branch.children[1] = w;
	}

 finished:
	return TRUE;
}

// Pure?
void* bt_access(bt* tree, int index) {
	bt_node* n = tree->root;
	int offset = index;
	while (!isleaf(n)) {
		int Nleft = node_N(n->branch.children[0]);
		if (offset >= Nleft) {
			n = n->branch.children[1];
			offset -= Nleft;
		} else
			n = n->branch.children[0];
	}
	return get_element(tree, &n->leaf, offset);
}

static void bt_print_node(bt* tree, bt_node* node, char* indent,
						  void (*print_element)(void* val)) {
	printf("%s", indent);
#if NODENUM
	printf("Node %i.  ", node->nodenum);
#endif
	printf("N=%i", node_N(node));

	if (!isleaf(node)) {
		char* subind;
		char* addind = "  ";
		printf(", Nleft=%i, Nright=%i, balance %i.\n",
			   node_N(node->branch.children[0]),
			   node_N(node->branch.children[1]),
			   node->branch.balance);
		subind = malloc(strlen(indent) + strlen(addind) + 1);
		sprintf(subind, "%s%s", indent, addind);
		printf("%sLeft child:\n", indent);
		bt_print_node(tree, node->branch.children[0], subind, print_element);
		printf("%sRight child:\n", indent);
		bt_print_node(tree, node->branch.children[1], subind, print_element);
		free(subind);
	} else {
		int i;
		printf(".  Leaf.");
		if (print_element) {
			printf("  [ ");
			for (i=0; i<node_N(node); i++)
				print_element(get_element(tree, &node->leaf, i));
		}
		printf("]\n");
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
#if NODENUM
	printf("Node %i.  ", node->nodenum);
#endif
	if (!isleaf(node)) {
		char* subind;
		char* addind = "|--";
		printf("(bal %i)\n", node->branch.balance);
		subind = malloc(strlen(indent) + strlen(addind) + 1);
		sprintf(subind, "%s%s", indent, addind);
		bt_print_struct_node(tree, node->branch.children[0], subind, print_element);
		bt_print_struct_node(tree, node->branch.children[1], subind, print_element);
	} else {
		int i;
		printf("(leaf)");
		if (print_element) {
			printf(" [ ");
			for (i=0; i<node_N(node); i++)
				print_element(get_element(tree, &node->leaf, i));
			printf("]");
		}
		printf("\n");
	}
}

void bt_print_structure(bt* tree, void (*print_element)(void* val)) {
	bt_print_struct_node(tree, tree->root, "   ", print_element);
}

