#ifndef BT_H
#define BT_H

#include "starutil.h"
#include "keywords.h"

#define NODENUM 0

struct bt_leaf {
	// always 1; must be the first element in the struct.
	unsigned char isleaf;
	// number of data elements.
	short N;
	// data follows implicitly.
};
typedef struct bt_leaf bt_leaf;

struct bt_branch {
	// always 0; must be the first element in the struct.
	unsigned char isleaf;
	// AVL balance
	signed char balance;

	struct bt_node* children[2];

	// the leftmost leaf node in this subtree.
	bt_leaf* firstleaf;

	// number of element in this subtree.
	int N;

#if NODENUM
	int nodenum;
#endif
};
typedef struct bt_branch bt_branch;

/*
  We distinguish between "branch" (ie, internal) nodes and "leaf" nodes,
  because leaf nodes don't need most of the entries in the "branch"
  struct, and since there are a lot of leaves, this space savings can be
  quite considerable.

  The data owned by a leaf node follows right after the leaf struct
  itself.
 */

struct bt_node {
	union {
		bt_leaf leaf;
		bt_branch branch;
	};
};
typedef struct bt_node bt_node;

struct bt {
	bt_node* root;
	int datasize;
	int blocksize;
	int N;
};
typedef struct bt bt;

typedef int (*compare_func)(const void* v1, const void* v2);

Malloc bt* bt_new(int datasize, int blocksize);

void bt_free(bt* tree);

Pure Inline int bt_size(bt* tree);

bool bt_insert(bt* tree, void* data, bool unique, compare_func compare);

bool bt_contains(bt* tree, void* data, compare_func compare);

void* bt_access(bt* tree, int index);

void bt_print(bt* tree, void (*print_element)(void* val));

void bt_print_structure(bt* tree, void (*print_element)(void* val));

int bt_height(bt* tree);

#endif
