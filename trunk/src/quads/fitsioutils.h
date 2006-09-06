#ifndef FITSIO_UTILS_H
#define FITSIO_UTILS_H

#include "qfits.h"

int fits_copy_header(qfits_header* src, qfits_header* dest, char* key);

int fits_copy_all_headers(qfits_header* src, qfits_header* dest, char* targetkey);

int fits_add_args(qfits_header* src, char** args, int argc);

// how many FITS blocks are required to hold 'size' bytes?
int fits_blocks_needed(int size);

int fits_pad_file(FILE* fid);

void fits_fill_endian_string(char* str);

char* fits_get_endian_string();

int fits_check_endian(qfits_header* header);

int fits_check_uint_size(qfits_header* header);

int fits_check_double_size(qfits_header* header);

void fits_add_endian(qfits_header* header);

void fits_add_uint_size(qfits_header* header);

void fits_add_double_size(qfits_header* header);

int fits_find_column(qfits_table* table, char* colname);

int fits_get_atom_size(tfits_type type);

int fits_find_table_column(char* fn, char* colname, int* start, int* size);

qfits_table* fits_get_table_column(char* fn, char* colname, int* pcol);

int fits_add_column(qfits_table* table, int column, tfits_type type,
					int ncopies, char* units, char* label);

// write single column fields:
int fits_write_data_A(FILE* fid, unsigned char value);
int fits_write_data_B(FILE* fid, unsigned char value);
int fits_write_data_D(FILE* fid, double value);
int fits_write_data_E(FILE* fid, float value);
int fits_write_data_I(FILE* fid, int16_t value);
int fits_write_data_J(FILE* fid, int32_t value);
int fits_write_data_K(FILE* fid, int64_t value);
int fits_write_data_X(FILE* fid, unsigned char value);

int fits_write_data(FILE* fid, void* pvalue, tfits_type type);

#endif

