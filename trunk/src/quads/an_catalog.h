#ifndef AN_CATALOG_H
#define AN_CATALOG_H

#include <stdio.h>

#include "qfits.h"

int an_write_entry(FILE* fid, usnob_entry* entry);

qfits_table* an_get_table();

#define AN_FITS_COLUMNS 55

struct an_catalog {
	qfits_table* table;
};
typedef struct an_catalog an_catalog;

an_catalog* an_catalog_open(char* fn);

int an_catalog_read_entries(an_catalog* usnob, uint offset,
							uint count, an_entry* entries);

int an_catalog_count_entries(an_catalog* usnob);

void an_catalog_close(an_catalog* usnob);

#endif
