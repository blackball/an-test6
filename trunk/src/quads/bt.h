#ifndef BT_H
#define BT_H

#include "starutil.h"
#include "keywords.h"

#define NODENUM 0

struct bt_datablock {
	// number of data elements.
	int N;
	// data follows implicitly.
};
typedef struct bt_datablock bt_datablock;

struct bt_node {
	struct bt_node* children[2];
	// if leaf: the data block owned by this leaf node.
	// else: the leftmost data block in this subtree.
	bt_datablock* data;
	// number of elements in the left subtree.
	int Nleft;
	// number of elements in the right subtree.
	int Nright;
	// AVL balance
	signed char balance;

#if defined(NODENUM)
	int nodenum;
#endif
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
