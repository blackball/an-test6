#ifndef USNOB_FITS_H
#define USNOB_FITS_H

#include <stdio.h>

#include "qfits.h"
#include "usnob.h"

#define USNOB_FITS_COLUMNS 55

struct usnob_fits {
	qfits_table* table;
	int columns[USNOB_FITS_COLUMNS];
	// when writing:
	qfits_header* header;
	//qfits_header* table_header;
	FILE* fid;
	off_t data_offset;
	uint nentries;
};
typedef struct usnob_fits usnob_fits;

usnob_fits* usnob_fits_open(char* fn);

usnob_fits* usnob_fits_open_for_writing(char* fn);

int usnob_fits_write_headers(usnob_fits* usnob);

int usnob_fits_fix_headers(usnob_fits* usnob);

int usnob_fits_read_entries(usnob_fits* usnob, uint offset,
							uint count, usnob_entry* entries);

int usnob_fits_count_entries(usnob_fits* usnob);

int usnob_fits_close(usnob_fits* usnob);

int usnob_fits_write_entry(usnob_fits* usnob, usnob_entry* entry);

//qfits_table* usnob_fits_get_table();

#endif
