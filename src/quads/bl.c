#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "bl.h"

static inline bl_node* bl_find_node(bl* list, int n, int* rtn_nskipped);
static bl_node* bl_new_node(bl* list);
static void bl_free_node(bl_node* node);

static void bl_sort_rec(bl* list, void* pivot,
						int (*compare)(const void* v1, const void* v2)) {
	bl* less;
	bl* equal;
	bl* greater;
	int i; //, iless;
    bl_node* node;
	bl_node* next;

	less = bl_new(list->blocksize, list->datasize);
	equal = bl_new(list->blocksize, list->datasize);
	greater = bl_new(list->blocksize, list->datasize);
	//iless = 0;
	for (node=list->head; node;) {
		char* data = node->data;
		for (i=0; i<node->N; i++) {
			int val = compare(data, pivot);
			if (val < 0) {
				/*
				  bl_set(list, iless, data);
				  iless++;
				*/
				bl_append(less, data);
			} else if (val > 0) {
				bl_append(greater, data);
			} else {
				bl_append(equal, data);
			}
			data += list->datasize;
		}
		next = node->next;
		bl_free_node(node);
		node = next;
	}
	list->head = NULL;
	list->tail = NULL;
	list->N = 0;

	bl_sort(less, compare);
	bl_sort(greater, compare);

	if (less->N) {
		list->head = less->head;
		list->tail = less->tail;
		list->N = less->N;
	}
	//*iequal = list->N;
	if (equal->N) {
		if (list->N) {
			list->tail->next = equal->head;
			list->tail = equal->tail;
		} else {
			list->head = equal->head;
			list->tail = equal->tail;
		}
		list->N += equal->N;
	}
	//*igreater = list->N;
	if (greater->N) {
		if (list->N) {
			list->tail->next = greater->head;
			list->tail = greater->tail;
		} else {
			list->head = greater->head;
			list->tail = greater->tail;
		}
		list->N += greater->N;
	}
	free(less);
	free(equal);
	free(greater);
}

void bl_sort(bl* list, int (*compare)(const void* v1, const void* v2)) {
	int ind;
	int N = list->N;
	if (N <= 1)
		return;
	//int iequal, igreater;
	//printf("bl_sort: implement me!\n");
	// should do median-of-3/5/... to select pivot when N is large.
	ind = rand() % N;
	bl_sort_rec(list, bl_access(list, ind), compare);
	//assert(0);
}

void bl_split(bl* src, bl* dest, int split) {
    bl_node* node;
    int nskipped;
    int ind;
    int ntaken = src->N - split;
    node = bl_find_node(src, split, &nskipped);
    if (!node) {
        printf("Error: in bl_split: node is null.\n");
        return;
    }
    ind = split - nskipped;
    if (ind == 0) {
        // this whole node belongs to "dest".
        if (split) {
            // we need to get the previous node...
            bl_node* last = bl_find_node(src, split-1, NULL);
            last->next = NULL;
            src->tail = last;
        } else {
            // we've removed everything from "src".
            src->head = NULL;
            src->tail = NULL;
        }
    } else {
        // create a new node to hold the second half of the items in "node".
        bl_node* newnode = bl_new_node(dest);
        newnode->N = (node->N - ind);
        newnode->next = node->next;
        memcpy((char*)newnode->data,
               (char*)node->data + (ind * src->datasize),
               newnode->N * src->datasize);
        node->N -= (node->N - ind);
        node->next = NULL;
        src->tail = node;
        // to make the code outside this block work...
        node = newnode;
    }

    // append it to "dest".
    if (dest->tail) {
        dest->tail->next = node;
        dest->N += ntaken;
    } else {
        dest->head = node;
        dest->tail = node;
        dest->N += ntaken;
    }

    // adjust "src".
    src->N -= ntaken;
    src->last_access = NULL;
}

void bl_init(bl* list, int blocksize, int datasize) {
	list->head = NULL;
	list->tail = NULL;
	list->N = 0;
	list->blocksize = blocksize;
	list->datasize  = datasize;
	list->last_access = NULL;
	list->last_access_n = 0;
}

bl* bl_new(int blocksize, int datasize) {
	bl* rtn;
	rtn = malloc(sizeof(bl));
	if (!rtn) {
		printf("Couldn't allocate memory for a bl.\n");
		return NULL;
	}
	bl_init(rtn, blocksize, datasize);
	return rtn;
}

void bl_free(bl* list) {
	bl_remove_all(list);
	free(list);
}

void bl_remove_all(bl* list) {
	bl_node *n, *lastnode;
	lastnode = NULL;
	for (n=list->head; n; n=n->next) {
		if (lastnode)
			bl_free_node(lastnode);
		lastnode = n;
	}
	if (lastnode)
		bl_free_node(lastnode);
	list->head = NULL;
	list->tail = NULL;
	list->N = 0;
	list->last_access = NULL;
	list->last_access_n = 0;
}

void bl_remove_all_but_first(bl* list) {
	bl_node *n, *lastnode;
	lastnode = NULL;

	if (list->head) {
		for (n=list->head->next; n; n=n->next) {
			if (lastnode)
				bl_free_node(lastnode);
			lastnode = n;
		}
		if (lastnode)
			bl_free_node(lastnode);
		list->head->next = NULL;
		list->head->N = 0;
		list->tail = list->head;
	} else {
		list->head = NULL;
		list->tail = NULL;
	}
	list->N = 0;
	list->last_access = NULL;
	list->last_access_n = 0;
}

static void bl_remove_from_node(bl* list, bl_node* node,
								bl_node* prev, int index_in_node) {
	// if we're removing the last element at this node, then
	// remove this node from the linked list.
	if (node->N == 1) {
		// if we're removing the first node...
		if (prev == NULL) {
			list->head = node->next;
			// if it's the first and only node...
			if (list->head == NULL) {
				list->tail = NULL;
			}
		} else {
			// if we're removing the last element from
			// the tail node...
			if (node == list->tail) {
				list->tail = prev;
			}
			prev->next = node->next;
		}
		bl_free_node(node);
	} else {
		int ncopy;
		// just remove this element...
		ncopy = node->N - index_in_node - 1;
		if (ncopy > 0) {
			memmove((char*)node->data + index_in_node * list->datasize,
					(char*)node->data + (index_in_node+1) * list->datasize,
					ncopy * list->datasize);
		}
		node->N--;
	}
	list->N--;
}

void bl_remove_index(bl* list, int index) {
	// find the node (and previous node) at which element 'index'
	// can be found.
	bl_node *node, *prev;
	int nskipped = 0;
	for (node=list->head, prev=NULL;
		 node;
		 prev=node, node=node->next) {

		if (index < (nskipped + node->N))
			break;

		nskipped += node->N;
	}

	if (!node) {
		fprintf(stderr, "Warning: bl_remove_index %i, but list is empty, or index is larger than the list.\n", index);
		return;
	}

	bl_remove_from_node(list, node, prev, index-nskipped);

	list->last_access = NULL;
	list->last_access_n = 0;
}

void bl_remove_index_range(bl* list, int start, int length) {
	// find the node (and previous node) at which element 'start'
	// can be found.
	bl_node *node, *prev;
	int nskipped = 0;
	list->last_access = NULL;
	list->last_access_n = 0;
	for (node=list->head, prev=NULL;
		 node;
		 prev=node, node=node->next) {

		if (start < (nskipped + node->N))
			break;

		nskipped += node->N;
	}

	// begin by removing any indices that are at the end of a block.
	if (start > nskipped) {
		// we're not removing everything at this node.
		int istart;
		int n;
		istart = start - nskipped;
		if ((istart + length) < node->N) {
			// we're removing a chunk of elements from the middle of this
			// block.  move elements from the end into the removed chunk.
			memmove((char*)node->data + istart * list->datasize,
					(char*)node->data + (istart + length) * list->datasize,
					(node->N - (istart + length)) * list->datasize);
			// we're done!
			node->N -= length;
			list->N -= length;
			return;
		} else {
			// we're removing everything from 'istart' to the end of this
			// block.  just change the "N" values.
			n = (node->N - istart);
			node->N -= n;
			list->N -= n;
			length -= n;
			start += n;
			nskipped = start;
			prev = node;
			node = node->next;
		}
	}

	// remove complete blocks.
	for (;;) {
		int n;
		bl_node* todelete;
		if (length == 0 || length < node->N)
			break;
		// we're skipping this whole block.
		n = node->N;
		length -= n;
		start += n;
		list->N -= n;
		nskipped += n;
		todelete = node;
		node = node->next;
		bl_free_node(todelete);
	}
	if (prev)
		prev->next = node;
	else
		list->head = node;

	if (!node)
		list->tail = prev;

	// remove indices from the beginning of the last block.
	// note that we may have removed everything from the tail of the list,
	// no "node" may be null.
	if (node && length>0) {
		//printf("removing %i from end.\n", length);
		memmove((char*)node->data,
				(char*)node->data + length * list->datasize,
				(node->N - length) * list->datasize);
		node->N -= length;
		list->N -= length;
	}
}

static void clear_list(bl* list) {
	list->head = NULL;
	list->tail = NULL;
	list->N = 0;
	list->last_access = NULL;
	list->last_access_n = 0;
}

void bl_append_list(bl* list1, bl* list2) {
	list1->last_access = NULL;
	list1->last_access_n = 0;
	if (list1->datasize != list2->datasize) {
		printf("Error: cannot append bls with different data sizes!\n");
		exit(0);
	}
	if (list1->blocksize != list2->blocksize) {
		printf("Error: cannot append bls with different block sizes!\n");
		exit(0);
	}

	// if list1 is empty, then just copy over list2's head and tail.
	if (list1->head == NULL) {
		list1->head = list2->head;
		list1->tail = list2->tail;
		list1->N = list2->N;
		// remove everything from list2 (to avoid sharing nodes)
		clear_list(list2);
		return;
	}

	// if list2 is empty, then do nothing.
	if (list2->head == NULL)
		return;

	// otherwise, append list2's head to list1's tail.
	list1->tail->next = list2->head;
	list1->tail = list2->tail;
	list1->N += list2->N;
	// remove everything from list2 (to avoid sharing nodes)
	clear_list(list2);
}

int bl_count(bl* list) {
	return list->N;
}

int bl_size(bl* list) {
	return bl_count(list);
}

static void bl_free_node(bl_node* node) {
	free(node);
}

static bl_node* bl_new_node(bl* list) {
	bl_node* rtn;
	// merge the mallocs for the node and its data into one malloc.
	rtn = malloc(sizeof(bl_node) + list->datasize * list->blocksize);
	if (!rtn) {
		printf("Couldn't allocate memory for a bl node!\n");
		return NULL;
	}
	rtn->data = (char*)rtn + sizeof(bl_node);
	rtn->N = 0;
	rtn->next = NULL;
	return rtn;
}

/*
 * Append an item to this bl node.  If this node is full, then create a new
 * node and insert it into the list.
 */
void bl_node_append(bl* list, bl_node* node, void* data) {
	if (node->N == list->blocksize) {
		// create new node and insert it.
		bl_node* newnode;
		newnode = bl_new_node(list);
		newnode->next = node->next;
		node->next = newnode;
		node = newnode;
	}
	// space remains at this node.  add item.
	memcpy((char*)node->data + node->N * list->datasize, data, list->datasize);
	node->N++;
	list->N++;
}

void bl_append(bl* list, void* data) {
	bl_node* tail;
	tail = list->tail;
	if (!tail) {
		// previously empty list.  create a new head-and-tail node.
		tail = bl_new_node(list);
		list->head = tail;
		list->tail = tail;
	}
	// append the item to the tail.  if the tail node is full, a new tail node may be created.
	bl_node_append(list, tail, data);
	if (list->tail->next)
		list->tail = list->tail->next;
}


void bl_print_structure(bl* list) {
	bl_node* n;
	printf("bl: head %p, tail %p, N %i\n", list->head, list->tail, list->N);
	for (n=list->head; n; n=n->next) {
		printf("[N=%i] ", n->N);
	}
	printf("\n");
}

void bl_get(bl* list, int n, void* dest) {
	char* src;
	src = (char*)bl_access(list, n);
	memcpy(dest, src, list->datasize);
}

/* find the node in which element "n" can be found. */
static inline bl_node* bl_find_node(bl* list, int n, int* rtn_nskipped) {
	bl_node* node;
	int nskipped;
	if (list->last_access && n >= list->last_access_n) {
		// take the shortcut!
		nskipped = list->last_access_n;
		node = list->last_access;
	} else {
		node = list->head;
		nskipped = 0;
	}

	for (; node;) {
		if (n < (nskipped + node->N))
			break;
		nskipped += node->N;
		node = node->next;
	}

	if (!node) {
		printf("in bl_find_node: node is null!\n");
	}

	if (rtn_nskipped)
		*rtn_nskipped = nskipped;

	return node;
}

static void bl_find_ind_and_element(bl* list, void* data,
									int (*compare)(const void* v1, const void* v2),
									void** presult, int* pindex) {
	int lower, upper;
	int cmp = -2;
	void* result;
	lower = -1;
	upper = list->N;
	while (lower < (upper-1)) {
		int mid;
		mid = (upper + lower) / 2;
		cmp = compare(data, bl_access(list, mid));
		if (cmp >= 0) {
			lower = mid;
		} else {
			upper = mid;
		}
	}
	if (lower == -1 || compare(data, (result = bl_access(list, lower)))) {
		*presult = NULL;
		*pindex = -1;
		return;
	}
	*presult = result;
	*pindex = lower;
}

/**
 * Finds a node for which the given compare() function
 * returns zero when passed the given 'data' pointer
 * and elements from the list.
 */
void* bl_find(bl* list, void* data,
			  int (*compare)(const void* v1, const void* v2)) {
	void* rtn;
	int ind;
	bl_find_ind_and_element(list, data, compare, &rtn, &ind);
	return rtn;
}

int bl_find_index(bl* list, void* data,
				  int (*compare)(const void* v1, const void* v2)) {
	void* val;
	int ind;
	bl_find_ind_and_element(list, data, compare, &val, &ind);
	return ind;
}

int bl_insert_sorted(bl* list, void* data,
					 int (*compare)(const void* v1, const void* v2)) {
	int lower, upper;
	lower = -1;
	upper = list->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, bl_access(list, mid));
		if (cmp >= 0) {
			lower = mid;
		} else {
			upper = mid;
		}
	}
	bl_insert(list, lower+1, data);
	return lower+1;
}

int bl_insert_unique_sorted(bl* list, void* data,
							int (*compare)(const void* v1, const void* v2)) {
	// This is just straightforward binary search - really should
	// use the block structure...
	int lower, upper;
	lower = -1;
	upper = list->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, bl_access(list, mid));
		if (cmp >= 0) {
			lower = mid;
		} else {
			upper = mid;
		}
	}

	if (lower >= 0) {
		if (compare(data, bl_access(list, lower)) == 0) {
			return -1;
		}
	}
	bl_insert(list, lower+1, data);
	return lower+1;
}

void bl_set(bl* list, int index, void* data) {
	bl_node* node;
	int nskipped;
	void* dataloc;

	node = bl_find_node(list, index, &nskipped);
	dataloc = (char*)node->data + (index - nskipped) * list->datasize;
	memcpy(dataloc, data, list->datasize);
	// update the last_access member...
	list->last_access = node;
	list->last_access_n = nskipped;
}

/**
 * Insert the element "data" into the list, such that its index is "index".
 * All elements that previously had indices "index" and above are moved
 * one position to the right.
 */
void bl_insert(bl* list, int index, void* data) {
	bl_node* node;
	int nskipped;

	if (list->N == index) {
		bl_append(list, data);
		return;
	}

	// this may fail if we try to insert an element past the last element in the list
	// (ie, append).  could detect this condition and pass the call the "append". [we do this above]
	node = bl_find_node(list, index, &nskipped);

	list->last_access = node;
	list->last_access_n = nskipped;

	// if the node is full:
	//   if we're inserting at the end of this node, then create a new node.
	//   else, shift all but the last element, add in this element, and 
	//     add the last element to a new node.
	if (node->N == list->blocksize) {
		int localindex, nshift;
		localindex = index - nskipped;

		// if the next node exists and is not full, then insert the overflowing
		// element at the front.  otherwise, create a new node.
		if (node->next && (node->next->N < list->blocksize)) {
			bl_node* next;
			next = node->next;
			// shift...
			memmove((char*)next->data + list->datasize,
					(char*)next->data,
					next->N * list->datasize);

			next->N++;
			list->N++;

			if (localindex == node->N) {
				// the new element is inserted into the next node.
				memcpy((char*)next->data, data, list->datasize);
			} else {
				// the last element in this node is added to the next node.
				memcpy((char*)next->data, (char*)node->data + (node->N-1)*list->datasize, list->datasize);
				// shift
				nshift = node->N - localindex - 1;
				memmove((char*)node->data + (localindex+1) * list->datasize,
						(char*)node->data + localindex * list->datasize,
						nshift * list->datasize);
				// insert...
				memcpy((char*)node->data + localindex * list->datasize,
					   data, list->datasize);
			}
		} else {
			bl_node* newnode;
			newnode = bl_new_node(list);
			newnode->next = node->next;
			node->next = newnode;
			if (newnode->next == NULL) {
				list->tail = newnode;
			}
			if (localindex == node->N) {
				// the new element is added to the new node.
				memcpy((char*)newnode->data, data, list->datasize);
			} else {
				// the last element in this node is added to the new node.
				memcpy((char*)newnode->data, (char*)node->data + (node->N-1)*list->datasize, list->datasize);
				// shift and insert.
				nshift = node->N - localindex - 1;
				memmove((char*)node->data + (localindex+1) * list->datasize,
						(char*)node->data + localindex * list->datasize,
						nshift * list->datasize);
				// insert...
				memcpy((char*)node->data + localindex * list->datasize,
					   data, list->datasize);
			}
			newnode->N++;
			list->N++;
		}
	} else {
		// shift...
		int localindex, nshift;
		localindex = index - nskipped;
		nshift = node->N - localindex;
		memmove((char*)node->data + (localindex+1) * list->datasize,
				(char*)node->data + localindex * list->datasize,
				nshift * list->datasize);
		// insert...
		memcpy((char*)node->data + localindex * list->datasize,
			   data, list->datasize);
		node->N++;
		list->N++;
	}
}

void* bl_access(bl* list, int n) {
	bl_node* node;
	int nskipped;
	void* rtn;

	node = bl_find_node(list, n, &nskipped);

	// grab the element.
	rtn = (char*)node->data + (n - nskipped) * list->datasize;
	// update the last_access member...
	list->last_access = node;
	list->last_access_n = nskipped;
	return rtn;
}

void bl_copy(bl* list, int start, int length, void* vdest) {
	bl_node* node;
	int nskipped;
	char* dest;

	if (length <= 0)
		return;

	if (list->last_access && start >= list->last_access_n) {
		// take the shortcut!
		nskipped = list->last_access_n;
		node = list->last_access;
	} else {
		node = list->head;
		nskipped = 0;
	}

	for (; node;) {
		if (start < (nskipped + node->N))
			break;
		nskipped += node->N;
		node = node->next;
	}

	// we've found the node containing "start".  keep copying elements and
	// moving down the list until we've copied all "length" elements.

	dest = (char*)vdest;
	while (length > 0) {
		int take, avail;
		char* src;
		// number of elements we want to take.
		take = length;
		// number of elements available at this node.
		avail = node->N - (start - nskipped);
		if (take > avail)
			take = avail;
		src = (char*)node->data + (start - nskipped) * list->datasize;
		memcpy(dest, src, take * list->datasize);

		dest += take * list->datasize;
		start += take;
		length -= take;
		nskipped += node->N;
		node = node->next;
	}

	// update the last_access member...
	list->last_access = node;
	list->last_access_n = nskipped;
}

int bl_check_consistency(bl* list) {
	bl_node* node;
	int N;
	int tailok = 1;
	int nempty = 0;
	int nnull = 0;
	
	// if one of head or tail is NULL, they had both better be NULL!
	if (!list->head)
		nnull++;
	if (!list->tail)
		nnull++;
	if (nnull == 1) {
		fprintf(stderr, "bl_check_consistency: head is %p, and tail is %p.\n",
				list->head, list->tail);
		return 1;
	}

	N = 0;
	for (node=list->head; node; node=node->next) {
		N += node->N;
		if (!node->N) {
			// this block is empty.
			nempty++;
		}
		// are we at the last node?
		if (!node->next) {
			tailok = (list->tail == node) ? 1 : 0;
		}
	}
	if (!tailok) {
		fprintf(stderr, "bl_check_consistency: tail pointer is wrong.\n");
		return 1;
	}
	if (nempty) {
		fprintf(stderr, "bl_check_consistency: %i empty blocks.\n", nempty);
		return 1;
	}
	if (N != list->N) {
		fprintf(stderr, "bl_check_consistency: list->N is %i, but sum of blocks is %i.\n",
				list->N, N);
		return 1;
	}
	return 0;
}

int bl_check_sorted(bl* list,
					int (*compare)(const void* v1, const void* v2),
					int isunique) {
	int i, N;
	int nbad = 0;
	void* v2 = NULL;
	N = bl_count(list);
	if (N)
		v2 = bl_access(list, 0);
	for (i=1; i<N; i++) {
		void* v1;
		int cmp;
		v1 = v2;
		v2 = bl_access(list, i);
		cmp = compare(v1, v2);
		if (isunique) {
			if (cmp >= 0) {
				nbad++;
			}
		} else {
			if (cmp > 0) {
				nbad++;
			}
		}
	}
	if (nbad) {
		fprintf(stderr, "bl_check_sorted: %i are out of order.\n", nbad);
		return 1;
	}
	return 0;
}

int bl_compare_ints_ascending(const void* v1, const void* v2) {
    int i1 = *(int*)v1;
    int i2 = *(int*)v2;
    if (i1 > i2) return 1;
    else if (i1 < i2) return -1;
    else return 0;
}

int bl_compare_ints_descending(const void* v1, const void* v2) {
    int i1 = *(int*)v1;
    int i2 = *(int*)v2;
    if (i1 > i2) return -1;
    else if (i1 < i2) return 1;
    else return 0;
}

// special-case integer list accessors...

void il_remove_all_reuse(il* list) {
	bl_remove_all_but_first(list);
}

int  il_find_index_ascending(il* list, int value) {
	return bl_find_index(list, &value, bl_compare_ints_ascending);
}

int il_check_consistency(il* list) {
	return bl_check_consistency(list);
}

int il_check_sorted_ascending(il* list,
							  int isunique) {
	return bl_check_sorted(list, bl_compare_ints_ascending, isunique);
}

int il_check_sorted_descending(il* list,
							   int isunique) {
	return bl_check_sorted(list, bl_compare_ints_descending, isunique);
}

int il_size(il* list) {
    return bl_count(list);
}

void il_remove(il* ilist, int index) {
    bl_remove_index(ilist, index);
}

int il_pop(il* ilist) {
    int ret = il_get(ilist, ilist->N-1);
    bl_remove_index(ilist, ilist->N-1);
    return ret;
}

il* il_dupe(il* ilist) {
    il* ret = il_new(ilist->blocksize);
    int i;
    for (i=0; i<ilist->N; i++)
        il_push(ret, il_get(ilist, i));
    return ret;
}

int il_remove_value(il* ilist, int value) {
    bl* list = ilist;
	bl_node *node, *prev;
	int istart = 0;
	for (node=list->head, prev=NULL;
		 node;
		 prev=node, node=node->next) {
		int i;
		int* idat;
		idat = (int*)node->data;
		for (i=0; i<node->N; i++)
			if (idat[i] == value) {
				bl_remove_from_node(list, node, prev, i);
				list->last_access = prev;
				list->last_access_n = istart;
				return istart + i;
			}
		istart += node->N;
	}
	return -1;
}

void il_remove_all(il* list) {
	bl_remove_all(list);
}

void il_remove_index_range(il* list, int start, int length) {
	bl_remove_index_range(list, start, length);
}

void il_set(il* list, int index, int value) {
	bl_set(list, index, &value);
}

il* il_new(int blocksize) {
	return (il*) bl_new(blocksize, sizeof(int));
}

void il_new_existing(il* list, int blocksize) {
	bl_init(list, blocksize, sizeof(int));
}

void il_init(il* list, int blocksize) {
	bl_init(list, blocksize, sizeof(int));
}

void il_free(il* list) {
	bl_free(list);
}

void il_push(il* list, int data) {
	bl_append(list, &data);
}

void il_append(il* list, int data) {
	bl_append(list, &data);
}

void il_merge_lists(il* list1, il* list2) {
	bl_append_list(list1, list2);
}

int il_get(il* list, int n) {
	int* ptr;
	ptr = (int*)bl_access(list, n);
	return *ptr;
}

int il_insert_ascending(il* list, int n) {
	bl_node *node;
	int* iarray;
	int lower, upper;
	int nskipped;
	// find the first node for which n <= the last element.
	// we will insert n into that node.
	if (list->last_access && list->last_access->N &&
		(n >= ((int*)(list->last_access->data))[0])) {
		node = list->last_access;
		nskipped = list->last_access_n;
	} else {
		node = list->head;
		nskipped = 0;
	}
	for (; node && (n > ((int*)(node->data))[node->N-1]);
		 node=node->next)
		nskipped += node->N;
	if (!node) {
		// either we're adding the first element, or we're appending since
		// n is bigger than the largest element in the list.
		il_append(list, n);
		return list->N-1;
	}

	// find where in the node it should be inserted...
	iarray = (int*)node->data;
	lower = -1;
	upper = node->N;
	while (lower < (upper-1)) {
		int mid;
		mid = (upper + lower) / 2;
		if (n >= iarray[mid])
			lower = mid;
		else
			upper = mid;
	}

	// set the jump accessors...
	list->last_access = node;
	list->last_access_n = nskipped;
	// ... so that this runs in O(1).
	bl_insert(list, nskipped + lower + 1, &n);
	return nskipped + lower + 1;
}

int il_insert_descending(il* list, int n) {
    return bl_insert_sorted(list, &n, bl_compare_ints_descending);
}

int il_insert_unique_ascending(il* list, int n) {
    return bl_insert_unique_sorted(list, &n, bl_compare_ints_ascending);
}

void il_copy(il* list, int start, int length, int* vdest) {
	bl_copy(list, start, length, vdest);
}

void il_print(bl* list) {
	bl_node* n;
	int i;
	for (n=list->head; n; n=n->next) {
		printf("[ ");
		for (i=0; i<n->N; i++)
			printf("%i, ", ((int*)n->data)[i]);
		printf("] ");
	}
}

int il_contains(il* list, int data) {
	bl_node* n;
	int i;
	int* idata;
	for (n=list->head; n; n=n->next) {
		idata = (int*)n->data;
		for (i=0; i<n->N; i++)
			if (idata[i] == data)
				return 1;
	}
	return 0;
}

int  il_index_of(il* list, int data) {
	bl_node* n;
	int i;
	int* idata;
	int npast = 0;
	for (n=list->head; n; n=n->next) {
		idata = (int*)n->data;
		for (i=0; i<n->N; i++)
			if (idata[i] == data)
				return npast + i;
		npast += n->N;
	}
	return -1;
}

// special-case pointer list accessors...
int bl_compare_pointers_ascending(const void* v1, const void* v2) {
    void* p1 = *(void**)v1;
    void* p2 = *(void**)v2;
    if (p1 > p2) return 1;
    else if (p1 < p2) return -1;
    else return 0;
}

int pl_insert_unique_ascending(bl* list, void* p) {
    return bl_insert_unique_sorted(list, &p, bl_compare_pointers_ascending);
}

pl* pl_new(int blocksize) {
    return bl_new(blocksize, sizeof(void*));
}

void pl_free(pl* list) {
    bl_free(list);
}

void  pl_remove(pl* list, int index) {
	bl_remove_index(list, index);
}

void  pl_remove_all(pl* list) {
	bl_remove_all(list);
}

void pl_set(pl* list, int index, void* data) {
	int i;
	int nadd = (index+1) - list->N;
	if (nadd > 0) {
		// enlarge the list to hold 'nadd' more entries.
		for (i=0; i<nadd; i++) {
			pl_append(list, NULL);
		}
	}
	bl_set(list, index, &data);
}

void pl_append(pl* list, void* data) {
    bl_append(list, &data);
}

void* pl_get(pl* list, int n) {
    void** ptr;
    ptr = (void**)bl_access(list, n);
    return *ptr;
}

void pl_copy(pl* list, int start, int length, void** vdest) {
    bl_copy(list, start, length, vdest);
}

void pl_print(pl* list) {
    bl_node* n;
    int i;
    for (n=list->head; n; n=n->next) {
		printf("[ ");
		for (i=0; i<n->N; i++)
			printf("%p ", ((void**)n->data)[i]);
		printf("] ");
    }
}

int   pl_size(pl* list) {
	return bl_count(list);
}

// special-case double list accessors...
void  dl_remove_all(dl* list) {
	bl_remove_all(list);
}

void dl_init(dl* list, int blocksize) {
	bl_init(list, blocksize, sizeof(double));
}

void   dl_insert(dl* list, int indx, double data) {
	bl_insert(list, indx, &data);
}

dl* dl_new(int blocksize) {
	return bl_new(blocksize, sizeof(double));
}

void dl_free(dl* list) {
	bl_free(list);
}

int   dl_size(dl* list) {
	return bl_count(list);
}

int dl_check_consistency(dl* list) {
	return bl_check_consistency(list);
}

void dl_push(dl* list, double data) {
	bl_append(list, &data);
}

void   dl_append(dl* list, double data) {
	bl_append(list, &data);
}

double dl_pop(dl* list) {
    double ret = dl_get(list, list->N-1);
    bl_remove_index(list, list->N-1);
    return ret;
}

double dl_get(dl* list, int n) {
	double* ptr;
	ptr = (double*)bl_access(list, n);
	return *ptr;
}

void dl_set(dl* list, int index, double value) {
	int i;
	int nadd = (index+1) - list->N;
	if (nadd > 0) {
		// enlarge the list to hold 'nadd' more entries.
		for (i=0; i<nadd; i++) {
			dl_append(list, 0.0);
		}
	}
	bl_set(list, index, &value);
}

void dl_copy(bl* list, int start, int length, double* vdest) {
	bl_copy(list, start, length, vdest);
}

dl* dl_dupe(dl* dlist) {
    dl* ret = dl_new(dlist->blocksize);
    int i;
    for (i=0; i<dlist->N; i++)
        dl_push(ret, dl_get(dlist, i));
    return ret; 
}

