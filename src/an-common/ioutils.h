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

#ifndef IOUTILS_H
#define IOUTILS_H

#include <stdio.h>

extern unsigned int ENDIAN_DETECTOR;

/**
   If "cmdline" starts with "keyword", returns 1 and places the address of
   the start of the next word in "p_next_word".
 */
int is_word(char* cmdline, char* keyword, char** p_next_word);

void add_sigbus_mmap_warning();
void reset_sigbus_mmap_warning();

int write_u8(FILE* fout, unsigned char val);
int write_u16(FILE* fout, unsigned int val);
int write_u32(FILE* fout, unsigned int val);
int write_uints(FILE* fout, unsigned int* val, int n);
int write_double(FILE* fout, double val);
int write_float(FILE* fout, float val);
int write_double(FILE* fout, double val);
int write_fixed_length_string(FILE* fout, char* s, int length);
int write_string(FILE* fout, char* s);

int write_u32_portable(FILE* fout, unsigned int val);
int write_u32s_portable(FILE* fout, unsigned int* val, int n);

int read_u8(FILE* fin, unsigned char* val);
int read_u16(FILE* fout, unsigned int* val);
int read_u32(FILE* fin, unsigned int* val);
int read_double(FILE* fin, double* val);
int read_fixed_length_string(FILE* fin, char* s, int length);
char* read_string(FILE* fin);
char* read_string_terminated(FILE* fin, char* terminators, int nterminators);

int read_u32_portable(FILE* fin, unsigned int* val);
int read_u32s_portable(FILE* fin, unsigned int* val, int n);

struct buffered_read_data {
	void* buffer;
	int blocksize;
	int elementsize;
	int ntotal;
	int nbuff;
	int off;
	int buffind;
	int (*refill_buffer)(void* userdata, void* buffer, unsigned int offs, unsigned int nelems);
	void* userdata;
};
typedef struct buffered_read_data bread;

void* buffered_read(bread* buff);

void buffered_read_pushback(bread* br);

void buffered_read_reset(bread* br);

void buffered_read_free(bread* br);

#endif
