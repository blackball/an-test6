/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

/**
   A linked list of arrays, which allows
   more rapid traversal of the list, and fairly
   efficient insertions and deletions.
*/

#ifndef BL_H
#define BL_H

#include <stdlib.h>

#include "keywords.h"

struct bl_node {
	// number of elements filled.
	int N;
	struct bl_node* next;
	// (data block implicitly follows this struct).
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
Malloc bl*  bl_new(int blocksize, int datasize);
Pure Inline int  bl_size(const bl* list);
void bl_init(bl* l, int blocksize, int datasize);
void bl_free(bl* list);
/** Appends an element, returning the location whereto it was copied. */
void* bl_append(bl* list, void* data);

/*
 * Appends "list2" to the end of "list1", and removes all elements
 * from "list2".
 */
void bl_append_list(bl* list1, bl* list2);
void bl_insert(bl* list, int indx, void* data);
void bl_set(bl* list, int indx, void* data);
/**
 * Inserts the given datum into the list in such a way that the list
 * stays sorted in ascending order according to the given comparison
 * function (assuming it was sorted to begin with!).
 *
 * The inserted element will be placed _after_ existing elements with
 * the same value.
 *
 * The comparison function is the same as qsort's: it should return
 * 1 if the first arg is greater than the second arg
 * 0 if they're equal
 * -1 if the first arg is smaller.
 *
 * The index where the element was inserted is returned.
 */
int bl_insert_sorted(bl* list, void* data, int (*compare)(const void* v1, const void* v2));

/**
   If the item already existed in the list (ie, the compare function
   returned zero), then -1 is returned.  Otherwise, the index at which
   the item was inserted is returned.
 */
int bl_insert_unique_sorted(bl* list, void* data,
                            int (*compare)(const void* v1, const void* v2));

void bl_sort(bl* list, int (*compare)(const void* v1, const void* v2));

// Copies the nth element into the destination location.
void  bl_get(bl* list, int n, void* dest);
void  bl_print_structure(bl* list);
// Returns a pointer to the nth element.
void* bl_access(bl* list, int n);
void  bl_copy(bl* list, int start, int length, void* vdest);
void  bl_remove_all(bl* list);
/*
  Removes all the elements, but doesn't free the first block, which makes
  it slightly faster for the case when you're going to add more elements
  right away, since you don't have to free() the old block then immediately
  malloc() a new block.
*/
void  bl_remove_all_but_first(bl* list);
void  bl_remove_index(bl* list, int indx);
void  bl_remove_index_range(bl* list, int start, int length);
void* bl_find(bl* list, void* data, int (*compare)(const void* v1, const void* v2));
int   bl_find_index(bl* list, void* data, int (*compare)(const void* v1, const void* v2));

// returns 0 if okay, 1 if an error is detected.
int   bl_check_consistency(bl* list);

// returns 0 if okay, 1 if an error is detected.
int   bl_check_sorted(bl* list, int (*compare)(const void* v1, const void* v2), int isunique);

///////////////////////////////////////////////
// special-case functions for integer lists. //
///////////////////////////////////////////////
typedef bl il;
Malloc il*  il_new(int blocksize);
Pure Inline int il_size(const il* list);
void il_new_existing(il* list, int blocksize);
void il_init(il* list, int blocksize);
void il_remove_all(il* list);
void il_remove_all_reuse(il* list);
void il_free(il* list);
int* il_append(il* list, int data);
void il_merge_lists(il* list1, il* list2);
void il_push(il* list, int data);
int  il_pop(il* list);
int  il_contains(il* list, int data);
int  il_index_of(il* list, int data);
Inline int  il_get(il* list, const int n);
void il_copy(il* list, int start, int length, int* vdest);
il*  il_dupe(il* list);
void il_print(il* list);
void   il_insert(il* list, int indx, int data);
int il_insert_ascending(il* list, int n);
int il_insert_descending(il* list, int n);
int  il_insert_unique_ascending(il* list, int p);
void il_set(il* list, int ind, int value);
void il_remove(il* list, int ind);
void il_remove_index_range(il* list, int start, int length);
int  il_find_index_ascending(il* list, int value);

il* il_merge_ascending(il* list1, il* list2);

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
void  pl_free_elements(pl* list);
int   pl_size(pl* list);
void* pl_get(pl* list, int n);
void  pl_set(pl* list, int ind, void* data);
void  pl_insert(pl* list, int indx, void* data);
void  pl_append(pl* list, void* data);
void  pl_copy(pl* list, int start, int length, void** dest);
pl*   pl_dup(pl* list);
void  pl_print(pl* list);
int   pl_insert_unique_ascending(pl* list, void* p);
int   pl_insert_sorted(pl* list, void* data, int (*compare)(const void* v1, const void* v2));
void  pl_remove(pl* list, int ind);
int   pl_remove_value(pl* list, void* value);
void  pl_remove_all(pl* list);
void  pl_merge_lists(pl* list1, pl* list2);

///////////////////////////////////////////////
// special-case functions for double lists. //
///////////////////////////////////////////////
typedef bl dl;
dl*    dl_new(int blocksize);
void   dl_init(dl* list, int blocksize);
void   dl_free(dl* list);
int    dl_size(dl* list);
double* dl_append(dl* list, double data);
void   dl_push(dl* list, double data);
double dl_pop(dl* list);
double dl_get(dl* list, int n);
void   dl_set(dl* list, int n, double val);
void   dl_copy(dl* list, int start, int length, double* vdest);
dl*    dl_dupe(dl* list);
int    dl_check_consistency(dl* list);
void   dl_insert(dl* list, int indx, double data);
void   dl_remove_all(dl* list);
void   dl_merge_lists(dl* list1, dl* list2);
void   dl_print(dl* list);

#endif
