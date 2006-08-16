#ifndef TWOMASS_FITS_H
#define TWOMASS_FITS_H

#include <stdio.h>

#include "qfits.h"
#include "ioutils.h"
#include "2mass.h"

#define TWOMASS_FITS_COLUMNS 107

struct twomass_catalog {
	qfits_table* table;
	int columns[TWOMASS_FITS_COLUMNS];
	uint nentries;
	// buffered reading
	bread br;
	// when writing:
	qfits_header* header;
	FILE* fid;
	off_t header_end;
};
typedef struct twomass_catalog twomass_catalog;

twomass_catalog* twomass_catalog_open(char* fn);

twomass_catalog* twomass_catalog_open_for_writing(char* fn);

int twomass_catalog_write_headers(twomass_catalog* cat);

int twomass_catalog_fix_headers(twomass_catalog* cat);

int twomass_catalog_read_entries(twomass_catalog* cat, uint offset,
								 uint count, twomass_entry* entries);

twomass_entry* twomass_catalog_read_entry(twomass_catalog* cat);

int twomass_catalog_count_entries(twomass_catalog* cat);

int twomass_catalog_close(twomass_catalog* cat);

int twomass_catalog_write_entry(twomass_catalog* cat, twomass_entry* entry);

#endif
