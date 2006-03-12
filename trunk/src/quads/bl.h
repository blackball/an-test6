/**
   A hybrid linked-list/array data structure.
   It's a linked list of arrays, which allows
   more rapid traversal of the list, and fairly
   efficient insertions and deletions.
*/

#ifndef BL_H
#define BL_H

#include <stdlib.h>

struct blocklist_node {
  // number of elements filled.
  int N;
  void* data;
  struct blocklist_node* next;
};
typedef struct blocklist_node blnode;

// the top-level data structure of a blocklist.
struct blocklist {
  blnode* head;
  blnode* tail;
  int N;
  int blocksize;
  int datasize;
  blnode* last_access;
  int last_access_n;
};
typedef struct blocklist blocklist;


/**
   Removes elements from \c split
   to the end of the list from \c src and appends them to \c dest.
 */
void blocklist_split(blocklist* src, blocklist* dest, int split);

blocklist* blocklist_new(int blocksize, int datasize);

void blocklist_init(blocklist* l, int blocksize, int datasize);

void blocklist_free(blocklist* list);

void blocklist_append(blocklist* list, void* data);

void blocklist_append_list(blocklist* list1, blocklist* list2);

void blocklist_insert(blocklist* list, int indx, void* data);

void blocklist_set(blocklist* list, int indx, void* data);

void blocklist_insert_sorted(blocklist* list, void* data,
			     int (*compare)(void* v1, void* v2));

/**
   If the item already existed in the list (ie, the compare function
   returned zero), then -1 is returned.  Otherwise, the index at which
   the item was inserted is returned.
 */
int blocklist_insert_unique_sorted(blocklist* list, void* data,
								   int (*compare)(void* v1, void* v2));

// Copies the nth element into the destination location.
void blocklist_get(blocklist* list, int n, void* dest);

void blocklist_print_structure(blocklist* list);

// Returns a pointer to the nth element.
void* blocklist_access(blocklist* list, int n);

int blocklist_count(blocklist* list);

void blocklist_copy(blocklist* list, int start, int length, void* vdest);

void blocklist_remove_all(blocklist* list);

void blocklist_remove_all_but_first(blocklist* list);

void blocklist_remove_index(blocklist* list, int indx);

void blocklist_remove_index_range(blocklist* list, int start, int length);

void* blocklist_find(blocklist* list, void* data,
		     int (*compare)(void* v1, void* v2));



///////////////////////////////////////////////
// special-case functions for integer lists. //
///////////////////////////////////////////////
typedef blocklist il;
il*  il_new(int blocksize);
int  il_size(il* list);
void il_new_existing(il* list, int blocksize);
void il_free(il* list);
void il_push(il* list, int data);
int  il_pop(il* list);
int  il_contains(il* list, int data);
int  il_get(il* list, int n);
void il_copy(il* list, int start, int length, int* vdest);
il*  il_dupe(il* list);
void il_print(il* list);
void il_insert_ascending(il* list, int n);
void il_insert_descending(il* list, int n);
int  il_insert_unique_ascending(il* list, int p);
void il_set(il* list, int index, int value);
void il_remove(il* list, int index);

// returns the index of the removed value, or -1 if it didn't
// exist in the list.
int il_remove_value(blocklist* list, int value);


///////////////////////////////////////////////
// special-case functions for pointer lists. //
///////////////////////////////////////////////
typedef blocklist pl;
pl*   pl_new(int blocksize);
void  pl_free(pl* list);
void* pl_get(pl* list, int n);
void  pl_set(pl* list, int index, void* data);
void  pl_append(pl* list, void* data);
void  pl_copy(pl* list, int start, int length, void** dest);
void  pl_print(pl* list);
int   pl_insert_unique_ascending(pl* list, void* p);
void  pl_remove(pl* list, int index);

///////////////////////////////////////////////
// special-case functions for double lists. //
///////////////////////////////////////////////
typedef blocklist dl;
dl*    dl_new(int blocksize);
void   dl_free(dl* list);
void   dl_push(dl* list, double data);
double dl_pop(dl* list);
double dl_get(dl* list, int n);
void   dl_copy(dl* list, int start, int length, double* vdest);
dl*    dl_dupe(dl* list);

#endif
