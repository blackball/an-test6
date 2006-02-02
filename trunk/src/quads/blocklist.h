/**
   A hybrid linked-list/array data structure.
   It's a linked list of arrays, which allows
   more rapid traversal of the list, and fairly
   efficient insertions and deletions.
*/

#ifndef BLOCKLIST_H
#define BLOCKLIST_H

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
void blocklist_free(blocklist* list);
void blocklist_append(blocklist* list, void* data);

void blocklist_append_list(blocklist* list1, blocklist* list2);

void blocklist_insert(blocklist* list, int indx, void* data);

void blocklist_insert_sorted(blocklist* list, void* data,
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
blocklist* blocklist_int_new(int blocksize);
void blocklist_int_free(blocklist* list);
void blocklist_int_append(blocklist* list, int data);
int blocklist_int_access(blocklist* list, int n);
void blocklist_int_copy(blocklist* list, int start, int length, int* vdest);
void blocklist_int_print(blocklist* list);
void blocklist_int_insert_ascending(blocklist* list, int n);
void blocklist_int_insert_descending(blocklist* list, int n);

///////////////////////////////////////////////
// special-case functions for pointer lists. //
///////////////////////////////////////////////
blocklist* blocklist_pointer_new(int blocksize);
void blocklist_pointer_free(blocklist* list);
void blocklist_pointer_append(blocklist* list, void* data);
void* blocklist_pointer_access(blocklist* list, int n);
void blocklist_pointer_copy(blocklist* list, int start, int length, void** dest);
void blocklist_pointer_print(blocklist* list);

///////////////////////////////////////////////
// special-case functions for double lists. //
///////////////////////////////////////////////
blocklist* blocklist_double_new(int blocksize);
void blocklist_double_free(blocklist* list);
void blocklist_double_append(blocklist* list, double data);
double blocklist_double_access(blocklist* list, int n);
void blocklist_double_copy(blocklist* list, int start, int length, double* vdest);

#endif
