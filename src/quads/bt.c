#include "bt.h"
#include "bl.h"

typedef int (*compare_func)(const void* v1, const void* v2);

struct bt_node {
	/*
	  union {
	  struct {
	  struct bt_node* left;
	  struct bt_node* right;
	  } lr;
	  struct bt_node* children[2];
	  };
	*/
	struct bt_node* children[2];
	// the leftmost block owned by this node.
	bl_node* node;
	// number of elements in the left subtree.
	int Nleft;
	// AVL balance
	signed char balance;
};
typedef struct bt_node bt_node;

struct bt {
	bt_node root;
	int datasize;
	int blocksize;
	int N;
};
typedef struct bt bt;

// data follows the bl_node*.
#define NODE_DATA(node) ((void*)(((bl_node*)(node)) + 1))
#define NODE_CHARDATA(node) ((char*)(((bl_node*)(node)) + 1))
//#define NODE_INTDATA(node) ((int*)(((bl_node*)(node)) + 1))

static void* get_element(bt_tree* tree, bt_node* node, int index) {
	return NODE_CHARDATA(node) + index * tree->datasize;
}

static void* first_element(bt_node* n) {
	return NODE_DATA(n->node);
}


static bt_node* bt_new_node() {
}

static bool bt_node_insert(bt* tree, bt_node* node, void* data, bool unique,
						   comparefunc* compare, void* overflow) {
	int lower, upper;
	int nshift;
	int index;

	// binary search...
	lower = -1;
	upper = node->N;
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
			return false;

	// shift...
	nshift = node->N - index;
	if (node->N == tree->blocksize) {
		// this node is full.  insert the element and put the overflowing
		// element in "overflow".
		if (index == node->N)
			memcpy(overflow, data, tree->datasize);
		else {
			memcpy(overflow, get_element(tree, node, node->N-1), tree->datasize);
			nshift--;
		}
	} else {
		node->N++;
		list->N++;
	}
	memmove(get_element(tree, node, index+1), get_element(tree, node, index),
			nshift * tree->datasize);
	// insert...
	memcpy(get_element(tree, node, index), data, tree->datasize);
	return true;
}

#define AVL_MAX_HEIGHT 32

bool bt_insert(bt* tree, void* data, bool unique, comparefunc* compare) {
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
	bt_node* nextnode;

	z = tree->root;
	y = tree->root;
	dir = 0;
	for (q = z, p = y; p; q = p, p = p->children[dir]) {
		int cmp = compare(item, first_element(p));
		/*
		  if (cmp == 0)
		  return p;
		*/
		/*
		  if ((cmp == 0) && unique)
		  return false;
		*/

		if (p->balance != 0) {
			z = q, y = p, k = 0;
		}
		ancestors[k] = p;
		da[k++] = dir = (cmp > 0);
    }

	///////// we've also gotta be careful to update the "Nleft" members.


	// will this element fit in the current node?
	rtn = bt_node_insert(tree, q, data, unique, compare, overflow);
	if (q->N <= tree->blocksize)
		return rtn;

	// is there room in the next node?
	{
		// find the next node:
		// -first, find the first ancestor of whom we are a left
		//  (grand^n)-child.
		bt_node* granny;
		bt_node* child = q;
		bt_node* nextnode;
		int i;
		for (i=k-1; i>=0; i--) {
			granny = ancestors[k-1];
			if (granny->children[0] == child)
				break;
			child = granny;
		}
		if (i < 0) {
			// no next node.
			nextnode = NULL;
		} else {
			// -next, find the leftmost leaf child
			nextnode = granny;
			while (nextnode->children[0])
				nextnode = nextnode->children[0];
		}
		if (nextnode && (nextnode->N < tree->blocksize)) {
			// there's room; insert the element!
			return bt_node_insert(tree, nextnode, overflow, unique, compare, NULL);
		}
	}

	// create a new node to hold this element.

	n = q->children[dir] = bt_new_node();
	if (!n)
		return NULL;


	n->avl_data = item;
	if (y == NULL)
		return &n->avl_data;

  for (p = y, k = 0; p != n; p = p->avl_link[da[k]], k++)
    if (da[k] == 0)
      p->avl_balance--;
    else
      p->avl_balance++;

  if (y->avl_balance == -2)
    {
      bt_node *x = y->avl_link[0];
      if (x->avl_balance == -1)
        {
          w = x;
          y->avl_link[0] = x->avl_link[1];
          x->avl_link[1] = y;
          x->avl_balance = y->avl_balance = 0;
        }
      else
        {
          assert (x->avl_balance == +1);
          w = x->avl_link[1];
          x->avl_link[1] = w->avl_link[0];
          w->avl_link[0] = x;
          y->avl_link[0] = w->avl_link[1];
          w->avl_link[1] = y;
          if (w->avl_balance == -1)
            x->avl_balance = 0, y->avl_balance = +1;
          else if (w->avl_balance == 0)
            x->avl_balance = y->avl_balance = 0;
          else /* |w->avl_balance == +1| */
            x->avl_balance = -1, y->avl_balance = 0;
          w->avl_balance = 0;
        }
    }
  else if (y->avl_balance == +2)
    {
      bt_node *x = y->avl_link[1];
      if (x->avl_balance == +1)
        {
          w = x;
          y->avl_link[1] = x->avl_link[0];
          x->avl_link[0] = y;
          x->avl_balance = y->avl_balance = 0;
        }
      else
        {
          assert (x->avl_balance == -1);
          w = x->avl_link[0];
          x->avl_link[0] = w->avl_link[1];
          w->avl_link[1] = x;
          y->avl_link[1] = w->avl_link[0];
          w->avl_link[0] = y;
          if (w->avl_balance == +1)
            x->avl_balance = 0, y->avl_balance = -1;
          else if (w->avl_balance == 0)
            x->avl_balance = y->avl_balance = 0;
          else /* |w->avl_balance == -1| */
            x->avl_balance = +1, y->avl_balance = 0;
          w->avl_balance = 0;
        }
    }
  else
    return &n->avl_data;
  z->avl_link[y != z->avl_link[0]] = w;

  tree->avl_generation++;
  return &n->avl_data;
}

void* bt_access(bt* tree, int index) {
	bt_node* n = tree->root;
	int offset = index;
	while (!n->node) {
		if (offset > n->Nleft) {
			// right child
			offset -= n->Nleft;
			//n = n->right;
			n = n->children[1];
		} else
			// left child
			//n = n->left;
			n = n->children[0];
	}
	return NODE_CHARDATA(n->node) + offset * tree->datasize;
}
