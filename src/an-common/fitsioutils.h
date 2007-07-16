/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#ifndef FITSIO_UTILS_H
#define FITSIO_UTILS_H

#include "qfits.h"
#include "keywords.h"

char* fits_get_dupstring(qfits_header* hdr, const char* key);

void
ATTRIB_FORMAT(printf,4,5)
fits_header_addf(qfits_header* hdr, const char* key, const char* comment,
                 const char* format, ...);

void
ATTRIB_FORMAT(printf,4,5)
fits_header_modf(qfits_header* hdr, const char* key, const char* comment,
                 const char* format, ...);

void fits_header_add_int(qfits_header* hdr, const char* key, int val,
                         const char* comment);

void fits_header_add_double(qfits_header* hdr, const char* key, double val,
                            const char* comment);

void fits_header_mod_int(qfits_header* hdr, const char* key, int val,
                         const char* comment);

void fits_header_mod_double(qfits_header* hdr, const char* key, double val,
                            const char* comment);

int fits_update_value(qfits_header* hdr, const char* key, const char* newvalue);

int fits_copy_header(qfits_header* src, qfits_header* dest, char* key);

int fits_copy_all_headers(qfits_header* src, qfits_header* dest, char* targetkey);

int fits_add_args(qfits_header* src, char** args, int argc);

int 
ATTRIB_FORMAT(printf,2,3)
fits_add_long_comment(qfits_header* dst, const char* format, ...);

int 
ATTRIB_FORMAT(printf,2,3)
fits_add_long_history(qfits_header* dst, const char* format, ...);

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

