#ifndef USNOB_FITS_H
#define USNOB_FITS_H

#include <stdio.h>

#include "qfits.h"
#include "usnob.h"

int usnob_fits_write_entry(FILE* fid, usnob_entry* entry);

qfits_table* usnob_fits_get_table();

#endif
