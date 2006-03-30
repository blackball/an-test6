#ifndef INTMAP_H
#define INTMAP_H

#include "bl.h"

struct intmap {
	il fromlist;
	il tolist;
};
typedef struct intmap intmap;

int intmap_length(intmap* m);

intmap* intmap_new();

void intmap_init(intmap* m);

void intmap_clear(intmap* m);

void intmap_free(intmap* map);

int intmap_conflicts(intmap* map, int from, int to);

// returns 0 if it was added okay;
//         1 if the mapping already existed.
//        -1 if a conflict would have been created.
int intmap_add(intmap* map, int from, int to);

// adds all elements in "map2" into "map1".
// returns 0 if it worked okay, -1 if a conflict would have been created.
int intmap_merge(intmap* map1, intmap* map2);

void intmap_get_entry(intmap* map, int indx, int* pfrom, int* pto);

// look up the value "from" in the mapping; return "fail" if it's not found.
int intmap_get(intmap* map, int from, int fail);

#endif
