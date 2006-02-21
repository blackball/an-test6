#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "blocklist.h"

inline blnode* blocklist_find_node(blocklist* list, int n, int* rtn_nskipped);
blnode* blocklist_new_node(blocklist* list);


void blocklist_split(blocklist* src, blocklist* dest, int split) {
    blnode* node;
    int nskipped;
    int ind;
    int ntaken = src->N - split;
    node = blocklist_find_node(src, split, &nskipped);
    if (!node) {
        printf("Error: in blocklist_split: node is null.\n");
        return;
    }
    ind = split - nskipped;
    if (ind == 0) {
        // this whole node belongs to "dest".
        if (split) {
            // we need to get the previous node...
            blnode* last = blocklist_find_node(src, split-1, NULL);
            last->next = NULL;
            src->tail = last;
        } else {
            // we've removed everything from "src".
            src->head = NULL;
            src->tail = NULL;
        }
    } else {
        // create a new node to hold the second half of the items in "node".
        blnode* newnode = blocklist_new_node(dest);
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

blocklist* blocklist_new(int blocksize, int datasize) {
	blocklist* rtn;
	rtn = (blocklist*)malloc(sizeof(blocklist));
	if (!rtn) {
		printf("Couldn't allocate memory for a blocklist.\n");
		return NULL;
	}
	rtn->head = NULL;
	rtn->tail = NULL;
	rtn->N = 0;
	rtn->blocksize = blocksize;
	rtn->datasize  = datasize;
	rtn->last_access = NULL;
	rtn->last_access_n = 0;
	return rtn;
}

void blocklist_free(blocklist* list) {
	blnode *n, *lastnode;
	lastnode = NULL;
	for (n=list->head; n; n=n->next) {
		// see new_node - we merge data and node malloc requests, so they don't have
		// to be freed separately.
		if (lastnode)
			free(lastnode);
		lastnode = n;
	}
	if (lastnode)
		free(lastnode);
	free(list);
	list = NULL;
}

void blocklist_remove_all(blocklist* list) {
	blnode *n, *lastnode;
	lastnode = NULL;
	for (n=list->head; n; n=n->next) {
		if (lastnode)
			free(lastnode);
		lastnode = n;
	}
	if (lastnode)
		free(lastnode);

	list->head = NULL;
	list->tail = NULL;
	list->N = 0;
	list->last_access = NULL;
	list->last_access_n = 0;
}

void blocklist_remove_all_but_first(blocklist* list) {
	blnode *n, *lastnode;
	lastnode = NULL;

	if (list->head) {
		for (n=list->head->next; n; n=n->next) {
			if (lastnode)
				free(lastnode);
			lastnode = n;
		}
		if (lastnode)
			free(lastnode);
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


void blocklist_remove_index(blocklist* list, int index) {
	// find the node (and previous node) at which element 'index'
	// can be found.
	blnode *node, *prev;
	int nskipped = 0;
	for (node=list->head, prev=NULL;
		 node;
		 prev=node, node=node->next) {

		if (index < (nskipped + node->N))
			break;

		nskipped += node->N;
	}

	if (!node) {
		fprintf(stderr, "Warning: blocklist_remove_index %i, but list is empty, or index is larger than the list.\n", index);
		return;
	}

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
		free(node);
	} else {
		int i;
		int ncopy;
		// just remove this element...
		i = index - nskipped;
		ncopy = node->N - i - 1;
		if (ncopy > 0) {
			memmove((char*)node->data + i * list->datasize,
					(char*)node->data + (i+1) * list->datasize,
					ncopy * list->datasize);
		}
		node->N--;
	}
	list->N--;
	list->last_access = NULL;
	list->last_access_n = 0;
}


void blocklist_remove_index_range(blocklist* list, int start, int length) {
	// find the node (and previous node) at which element 'start'
	// can be found.
	blnode *node, *prev;
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
			//printf("removing %i from start (middle).\n", length);
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
			//printf("removing %i from start (mid to end).\n", n);
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
		blnode* todelete;
		if (length == 0 || length < node->N)
			break;
		//printf("removing block of %i...\n", node->N);
		// we're skipping this whole block.
		n = node->N;
		length -= n;
		start += n;
		list->N -= n;
		nskipped += n;
		todelete = node;
		node = node->next;
		//free(todelete->data);
		free(todelete);
	}
	//printf("node=%p, prev=%p.  length %i\n", node, prev, list->N);
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


void clear_list(blocklist* list) {
	list->head = NULL;
	list->tail = NULL;
	list->N = 0;
	list->last_access = NULL;
	list->last_access_n = 0;
}



/*
 * Appends "list2" to the end of "list1", and removes all elements
 * from "list2".
 */
void blocklist_append_list(blocklist* list1, blocklist* list2) {
	list1->last_access = NULL;
	list1->last_access_n = 0;
	if (list1->datasize != list2->datasize) {
		printf("Error: cannot append blocklists with different data sizes!\n");
		exit(0);
	}
	if (list1->blocksize != list2->blocksize) {
		printf("Error: cannot append blocklists with different block sizes!\n");
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


int blocklist_count(blocklist* list) {
	return list->N;
}

blnode* blocklist_new_node(blocklist* list) {
	blnode* rtn;

	// merge the mallocs for the node and its data into one malloc.
	rtn = (blnode*)malloc(sizeof(blnode) + list->datasize * list->blocksize);
	if (!rtn) {
		printf("Couldn't allocate memory for a blocklist node!\n");
		return NULL;
	}
	rtn->data = (char*)rtn + sizeof(blnode);

	rtn->N = 0;
	rtn->next = NULL;
	return rtn;
}

/*
 * Append an item to this blocklist node.  If this node is full, then create a new
 * node and insert it into the list.
 */
void blocklist_node_append(blocklist* list, blnode* node, void* data) {
	if (node->N == list->blocksize) {
		// create new node and insert it.
		blnode* newnode;
		newnode = blocklist_new_node(list);
		newnode->next = node->next;
		node->next = newnode;
		node = newnode;
	}
	// space remains at this node.  add item.
	memcpy((char*)node->data + node->N * list->datasize, data, list->datasize);
	node->N++;
	list->N++;
}

void blocklist_append(blocklist* list, void* data) {
	blnode* tail;
	tail = list->tail;
	if (!tail) {
		// previously empty list.  create a new head-and-tail node.
		tail = blocklist_new_node(list);
		list->head = tail;
		list->tail = tail;
	}
	// append the item to the tail.  if the tail node is full, a new tail node may be created.
	blocklist_node_append(list, tail, data);
	if (list->tail->next)
		list->tail = list->tail->next;
}


void blocklist_print_structure(blocklist* list) {
	blnode* n;
	printf("blocklist: head %p, tail %p, N %i\n", list->head, list->tail, list->N);
	for (n=list->head; n; n=n->next) {
		printf("[N=%i] ", n->N);
	}
	printf("\n");
}

void blocklist_get(blocklist* list, int n, void* dest) {
	char* src;
	src = (char*)blocklist_access(list, n);
	memcpy(dest, src, list->datasize);
}

/* internal use only - find the node in which element "n" can be found. */
inline blnode* blocklist_find_node(blocklist* list, int n, int* rtn_nskipped) {
	blnode* node;
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
		printf("in blocklist_find_node: node is null!\n");
	}

	if (rtn_nskipped)
		*rtn_nskipped = nskipped;

	return node;
}

/**
 * Finds a node for which the given compare() function
 * returns zero when passed the given 'data' pointer
 * and elements from the list.
 */
void* blocklist_find(blocklist* list, void* data,
					 int (*compare)(void* v1, void* v2)) {
	int lower, upper;
	int cmp = -2;
	void* result;
	lower = -1;
	upper = list->N;
	while (lower < (upper-1)) {
		int mid;
		mid = (upper + lower) / 2;
		cmp = compare(data, blocklist_access(list, mid));
		if (cmp > 0) {
			upper = mid;
		} else {
			lower = mid;
		}
	}
  
	if (lower == -1 || compare(data, (result = blocklist_access(list, lower))))
		return NULL;
	return result;
}



/**
 * Inserts the given datum into the list in such a way that the list
 * stays sorted according to the given comparison function (assuming
 * it was sorted to begin with!).
 */
void blocklist_insert_sorted(blocklist* list, void* data,
							 int (*compare)(void* v1, void* v2)) {
	int lower, upper;
	lower = -1;
	upper = list->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, blocklist_access(list, mid));
		if (cmp > 0) {
			upper = mid;
		} else {
			lower = mid;
		}
	}
	blocklist_insert(list, lower+1, data);
}

int blocklist_insert_unique_sorted(blocklist* list, void* data,
								   int (*compare)(void* v1, void* v2)) {
	// This is just straightforward binary search - really should
	// use the block structure...
	int lower, upper;
	lower = -1;
	upper = list->N;
	while (lower < (upper-1)) {
		int mid;
		int cmp;
		mid = (upper + lower) / 2;
		cmp = compare(data, blocklist_access(list, mid));
		if (cmp > 0) {
			upper = mid;
		} else {
			lower = mid;
		}
	}

	/*
	  printf("lower=%i.\n", lower);
	  if (lower > 0)
	  printf("[lower-1] = %i\n", blocklist_int_access(list, lower-1));
	  if (lower >= 0)
	  printf("[lower] = %i\n", blocklist_int_access(list, lower));
	  if (lower < list->N-1)
	  printf("[lower+1] = %i\n", blocklist_int_access(list, lower+1));
	  if (lower < list->N-2)
	  printf("[lower+2] = %i\n", blocklist_int_access(list, lower+2));
	*/

	if (lower >= 0) {
		if (compare(data, blocklist_access(list, lower)) == 0) {
			return -1;
		}
	}
	blocklist_insert(list, lower+1, data);
	return lower+1;
}


void blocklist_set(blocklist* list, int index, void* data) {
	blnode* node;
	int nskipped;
	void* dataloc;

	node = blocklist_find_node(list, index, &nskipped);
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
void blocklist_insert(blocklist* list, int index, void* data) {
	blnode* node;
	int nskipped;

	if (list->N == index) {
		blocklist_append(list, data);
		return;
	}

	// this may fail if we try to insert an element past the last element in the list
	// (ie, append).  could detect this condition and pass the call the "append". [we do this above]
	node = blocklist_find_node(list, index, &nskipped);

	list->last_access = node;
	list->last_access_n = nskipped;

#if 0
	if (index <= list->last_access_n) {
		list->last_access_n++;
		/*
		  list->last_access = NULL;
		  list->last_access_n = 0;
		*/
	}
#endif


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
			blnode* next;
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
			blnode* newnode;
			newnode = blocklist_new_node(list);
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


void* blocklist_access(blocklist* list, int n) {
	blnode* node;
	int nskipped;
	void* rtn;

	node = blocklist_find_node(list, n, &nskipped);

	/*
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
	  if (node == NULL) {
	  printf("in blocklist_access: node is null!\n");
	  exit(0);
	  }
	*/

	// grab the element.
	rtn = (char*)node->data + (n - nskipped) * list->datasize;
	// update the last_access member...
	list->last_access = node;
	list->last_access_n = nskipped;
	return rtn;
}

void blocklist_copy(blocklist* list, int start, int length, void* vdest) {
	blnode* node;
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

int compare_ints_ascending(void* v1, void* v2) {
    int i1 = *(int*)v1;
    int i2 = *(int*)v2;
    if (i1 < i2) return 1;
    else if (i1 > i2) return -1;
    else return 0;
}

int compare_ints_descending(void* v1, void* v2) {
    int i1 = *(int*)v1;
    int i2 = *(int*)v2;
    if (i1 < i2) return -1;
    else if (i1 > i2) return 1;
    else return 0;
}

// special-case integer list accessors...
void blocklist_int_set(blocklist* list, int index, int value) {
	blocklist_set(list, index, &value);
}

blocklist* blocklist_int_new(int blocksize) {
	return blocklist_new(blocksize, sizeof(int));
}
void blocklist_int_free(blocklist* list) {
	blocklist_free(list);
}
void blocklist_int_append(blocklist* list, int data) {
	blocklist_append(list, &data);
}
int blocklist_int_access(blocklist* list, int n) {
	int* ptr;
	ptr = (int*)blocklist_access(list, n);
	return *ptr;
}
void blocklist_int_insert_ascending(blocklist* list, int n) {
    blocklist_insert_sorted(list, &n, compare_ints_ascending);
}
void blocklist_int_insert_descending(blocklist* list, int n) {
    blocklist_insert_sorted(list, &n, compare_ints_descending);
}
int blocklist_int_insert_unique_ascending(blocklist* list, int n) {
    return blocklist_insert_unique_sorted(list, &n, compare_ints_ascending);
}
void blocklist_int_copy(blocklist* list, int start, int length, int* vdest) {
	blocklist_copy(list, start, length, vdest);
}

void blocklist_int_print(blocklist* list) {
	blnode* n;
	int i;
	for (n=list->head; n; n=n->next) {
		printf("[ ");
		for (i=0; i<n->N; i++)
			printf("%i ", ((int*)n->data)[i]);
		printf("] ");
	}
}

int blocklist_int_contains(blocklist* list, int data) {
	blnode* n;
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


// special-case pointer list accessors...
int compare_pointers_ascending(void* v1, void* v2) {
    void* p1 = *(void**)v1;
    void* p2 = *(void**)v2;
    if (p1 < p2) return 1;
    else if (p1 > p2) return -1;
    else return 0;
}

int blocklist_pointer_insert_unique_ascending(blocklist* list, void* p) {
    return blocklist_insert_unique_sorted(list, &p, compare_pointers_ascending);
}

blocklist* blocklist_pointer_new(int blocksize) {
    return blocklist_new(blocksize, sizeof(void*));
}
void blocklist_pointer_free(blocklist* list) {
    blocklist_free(list);
}
void blocklist_pointer_set(blocklist* list, int index, void* data) {
	blocklist_set(list, index, &data);
}
void blocklist_pointer_append(blocklist* list, void* data) {
    blocklist_append(list, &data);
}
void* blocklist_pointer_access(blocklist* list, int n) {
    void** ptr;
    ptr = (void**)blocklist_access(list, n);
    return *ptr;
}
void blocklist_pointer_copy(blocklist* list, int start, int length, void** vdest) {
    blocklist_copy(list, start, length, vdest);
}
void blocklist_pointer_print(blocklist* list) {
    blnode* n;
    int i;
    for (n=list->head; n; n=n->next) {
		printf("[ ");
		for (i=0; i<n->N; i++)
			printf("%p ", ((void**)n->data)[i]);
		printf("] ");
    }
}



// special-case double list accessors...
blocklist* blocklist_double_new(int blocksize) {
	return blocklist_new(blocksize, sizeof(double));
}
void blocklist_double_free(blocklist* list) {
	blocklist_free(list);
}
void blocklist_double_append(blocklist* list, double data) {
	blocklist_append(list, &data);
}
double blocklist_double_access(blocklist* list, int n) {
	double* ptr;
	ptr = (double*)blocklist_access(list, n);
	return *ptr;
}
void blocklist_double_copy(blocklist* list, int start, int length, double* vdest) {
	blocklist_copy(list, start, length, vdest);
}
