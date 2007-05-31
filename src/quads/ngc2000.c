#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ngc2000.h"

struct ngc_name {
	bool is_ngc;
	int id;
	char* name;
};
typedef struct ngc_name ngc_name;

ngc_name ngc_names[] = {
#include "ngc2000names.c"
};

ngc_entry ngc_entries[] = {
#include "ngc2000entries.c"
};

int ngc_num_entries() {
	return sizeof(ngc_entries) / sizeof(ngc_entry);
}

ngc_entry* ngc_get_entry(int i) {
	if (i < 0)
		return NULL;
	if (i >= (sizeof(ngc_entries) / sizeof(ngc_entry)))
		return NULL;
	return ngc_entries + i;
}

char* ngc_get_name(ngc_entry* entry) {
	int i;
	for (i=0; i<sizeof(ngc_names)/sizeof(ngc_name); i++) {
		if ((entry->is_ngc == ngc_names[i].is_ngc) &&
			(entry->id == ngc_names[i].id)) {
			return ngc_names[i].name;
		}
	}
	return NULL;
}

