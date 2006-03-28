#ifndef FITSIO_UTILS_H
#define FITSIO_UTILS_H

// how many FITS blocks are required to hold 'size' bytes?
int fits_blocks_needed(int size);

void fits_fill_endian_string(char* str);

char* fits_get_endian_string();

int fits_check_endian(qfits_header* header);

int fits_check_uint_size(qfits_header* header);

int fits_check_double_size(qfits_header* header);

void fits_add_endian(qfits_header* header);

void fits_add_uint_size(qfits_header* header);

void fits_add_double_size(qfits_header* header);

int fits_find_table_column(char* fn, char* colname, int* start, int* size);

#endif

