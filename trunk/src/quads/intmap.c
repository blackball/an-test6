#include "intmap.h"

int intmap_length(intmap* m) {
	return blocklist_count(&m->fromlist);
}

intmap* intmap_new() {
	intmap* c = malloc(sizeof(intmap));
	intmap_init(c);
	return c;
}

void intmap_init(intmap* c) {
	blocklist_int_init(&c->fromlist, 4);
	blocklist_int_init(&c->tolist, 4);
}

void intmap_clear(intmap* m) {
	blocklist_remove_all(&m->fromlist);
	blocklist_remove_all(&m->tolist);
}

void intmap_free(intmap* map) {
	intmap_clear(map);
	free(map);
}

int intmap_conflicts(intmap* c, int from, int to) {
	int i, len;
	len = blocklist_count(&c->fromlist);
	for (i=0; i<len; i++) {
		int f, t;
		f = blocklist_int_access(&c->fromlist, i);
		t = blocklist_int_access(&c->tolist  , i);
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
	len = blocklist_count(&c->fromlist);
	for (i=0; i<len; i++) {
		int f, t;
		f = blocklist_int_access(&c->fromlist, i);
		t = blocklist_int_access(&c->tolist  , i);
		if ((from == f) && (to == t)) {
			// mapping exists.
			return 1;
		} else if ((from == f) || (to == t)) {
			// conflict!
			return -1;
		}
	}
	blocklist_int_append(&c->fromlist, from);
	blocklist_int_append(&c->tolist, to);
	return 0;
}

int intmap_merge(intmap* map1, intmap* map2) {
	int i, len;
	int okay = 1;
	len = blocklist_count(&map2->fromlist);
	for (i=0; i<len; i++) {
		int from, to;
		from = blocklist_int_access(&map2->fromlist, i);
		to   = blocklist_int_access(&map2->tolist  , i);
		if (intmap_add(map1, from, to) == -1) {
			okay = 0;
		}
	}
	if (okay)
		return 0;
	return -1;
}
