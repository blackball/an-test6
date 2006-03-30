#include <assert.h>

#include "intmap.h"

int intmap_get(intmap* map, int from, int fail) {
	int indx = il_index_of(&map->fromlist, from);
	if (indx == -1) return fail;
	return il_get(&map->tolist, indx);
}

void intmap_get_entry(intmap* map, int indx, int* pfrom, int* pto) {
	int from, to;
	assert(indx < il_size(&map->fromlist));
	assert(pfrom);
	assert(pto);
	from = il_get(&map->fromlist, indx);
	to = il_get(&map->tolist, indx);
	*pfrom = from;
	*pto = to;
}

int intmap_length(intmap* m) {
	return il_size(&m->fromlist);
}

intmap* intmap_new() {
	intmap* c = malloc(sizeof(intmap));
	intmap_init(c);
	return c;
}

void intmap_init(intmap* c) {
	il_new_existing(&c->fromlist, 4);
	il_new_existing(&c->tolist, 4);
}

void intmap_clear(intmap* m) {
	il_remove_all(&m->fromlist);
	il_remove_all(&m->tolist);
}

void intmap_free(intmap* map) {
	intmap_clear(map);
	free(map);
}

int intmap_conflicts(intmap* c, int from, int to) {
	int i, len;
	len = il_size(&c->fromlist);
	for (i=0; i<len; i++) {
		int f, t;
		f = il_get(&c->fromlist, i);
		t = il_get(&c->tolist  , i);
		if ((from == f) && (to == t)) {
			// okay.
			continue;
		} else if ((from == f) || (to == t)) {
			// conflict!
			return 1;
		}
	}
	return 0;
}

int intmap_add(intmap* c, int from, int to) {
	int i, len;
	len = il_size(&c->fromlist);
	for (i=0; i<len; i++) {
		int f, t;
		f = il_get(&c->fromlist, i);
		t = il_get(&c->tolist  , i);
		if ((from == f) && (to == t)) {
			// mapping exists.
			return 1;
		} else if ((from == f) || (to == t)) {
			// conflict!
			return -1;
		}
	}
	il_append(&c->fromlist, from);
	il_append(&c->tolist, to);
	return 0;
}

int intmap_merge(intmap* map1, intmap* map2) {
	int i, len;
	int okay = 1;
	len = il_size(&map2->fromlist);
	for (i=0; i<len; i++) {
		int from, to;
		from = il_get(&map2->fromlist, i);
		to   = il_get(&map2->tolist  , i);
		if (intmap_add(map1, from, to) == -1) {
			okay = 0;
		}
	}
	if (okay)
		return 0;
	return -1;
}
