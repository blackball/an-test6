#ifndef USNOB_FITS_H
#define USNOB_FITS_H

#include <stdio.h>

#include "qfits.h"
#include "usnob.h"

int usnob_fits_write_entry(FILE* fid, usnob_entry* entry);

qfits_table* usnob_fits_get_table();


#define USNOB_FITS_COLUMNS 55

struct usnob_fits {
	qfits_table* table;
	int columns[USNOB_FITS_COLUMNS];
};
typedef struct usnob_fits usnob_fits;

usnob_fits* usnob_fits_open(char* fn);

int usnob_fits_read_entries(usnob_fits* usnob, uint offset,
							uint count, usnob_entry* entries);

int usnob_fits_count_entries(usnob_fits* usnob);

void usnob_fits_close(usnob_fits* usnob);

#endif
