/**
   A hybrid linked-list/array data structure.
   It's a linked list of arrays, which allows
   more rapid traversal of the list, and fairly
   efficient insertions and deletions.
*/

#ifndef BL_H
#define BL_H

#include <stdlib.h>

struct bl_node {
  // number of elements filled.
  int N;
  void* data;
  struct bl_node* next;
};
typedef struct bl_node bl_node;

// the top-level data structure of a blocklist.
struct bl {
  bl_node* head;
  bl_node* tail;
  int N;
  int blocksize;
  int datasize;
  bl_node* last_access;
  int last_access_n;
};
typedef struct bl bl;


/**
   Removes elements from \c split
   to the end of the list from \c src and appends them to \c dest.
 */
void bl_split(bl* src, bl* dest, int split);
bl*  bl_new(int blocksize, int datasize);
int  bl_size(bl* list);
void bl_init(bl* l, int blocksize, int datasize);
void bl_free(bl* list);
void bl_append(bl* list, void* data);
void bl_append_list(bl* list1, bl* list2);
void bl_insert(bl* list, int indx, void* data);
void bl_set(bl* list, int indx, void* data);
void bl_insert_sorted(bl* list, void* data, int (*compare)(void* v1, void* v2));

/**
   If the item already existed in the list (ie, the compare function
   returned zero), then -1 is returned.  Otherwise, the index at which
   the item was inserted is returned.
 */
int bl_insert_unique_sorted(bl* list, void* data,
                            int (*compare)(void* v1, void* v2));

// Copies the nth element into the destination location.
void  bl_get(bl* list, int n, void* dest);
void  bl_print_structure(bl* list);
// Returns a pointer to the nth element.
void* bl_access(bl* list, int n);
int   bl_count(bl* list);
void  bl_copy(bl* list, int start, int length, void* vdest);
void  bl_remove_all(bl* list);
void  bl_remove_all_but_first(bl* list);
void  bl_remove_index(bl* list, int indx);
void  bl_remove_index_range(bl* list, int start, int length);
void* bl_find(bl* list, void* data, int (*compare)(void* v1, void* v2));

// returns 0 if okay, 1 if an error is detected.
int   bl_check_consistency(bl* list);

// returns 0 if okay, 1 if an error is detected.
int   bl_check_sorted(bl* list, int (*compare)(void* v1, void* v2), int isunique);

///////////////////////////////////////////////
// special-case functions for integer lists. //
///////////////////////////////////////////////
typedef bl il;
il*  il_new(int blocksize);
int  il_size(il* list);
void il_new_existing(il* list, int blocksize);
void il_remove_all(il* list);
void il_free(il* list);
void il_append(il* list, int data);
void il_merge_lists(il* list1, il* list2);
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
void il_set(il* list, int ind, int value);
void il_remove(il* list, int ind);
void il_remove_index_range(il* list, int start, int length);

// returns the index of the removed value, or -1 if it didn't
// exist in the list.
int il_remove_value(il* list, int value);

int il_check_consistency(il* list);
int il_check_sorted_ascending(il* list, int isunique);
int il_check_sorted_descending(il* list, int isunique);


///////////////////////////////////////////////
// special-case functions for pointer lists. //
///////////////////////////////////////////////
typedef bl pl;
pl*   pl_new(int blocksize);
void  pl_free(pl* list);
int   pl_size(pl* list);
void* pl_get(pl* list, int n);
void  pl_set(pl* list, int ind, void* data);
void  pl_append(pl* list, void* data);
void  pl_copy(pl* list, int start, int length, void** dest);
void  pl_print(pl* list);
int   pl_insert_unique_ascending(pl* list, void* p);
void  pl_remove(pl* list, int ind);
void  pl_remove_all(pl* list);

///////////////////////////////////////////////
// special-case functions for double lists. //
///////////////////////////////////////////////
typedef bl dl;
dl*    dl_new(int blocksize);
void   dl_free(dl* list);
int    dl_size(dl* list);
void   dl_append(dl* list, double data);
void   dl_push(dl* list, double data);
double dl_pop(dl* list);
double dl_get(dl* list, int n);
void   dl_set(dl* list, int n, double val);
void   dl_copy(dl* list, int start, int length, double* vdest);
dl*    dl_dupe(dl* list);
int    dl_check_consistency(dl* list);

#endif
