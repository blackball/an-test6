#ifndef BT_H
#define BT_H

#include "bl.h"

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
	// the data block owned by this leaf node.
	bl_node* node;
	// number of elements in the left subtree.
	int Nleft;
	// number of elements in the right subtree.
	int Nright;
	// AVL balance
	signed char balance;
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

typedef unsigned char bool;

bt* bt_new(int datasize, int blocksize);

bool bt_insert(bt* tree, void* data, bool unique, compare_func compare);

void* bt_access(bt* tree, int index);

void bt_print(bt* tree, void (*print_element)(void* val));

#endif
