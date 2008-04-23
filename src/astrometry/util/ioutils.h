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
#include <stdint.h>
#include <sys/types.h>

#include "an-bool.h"
#include "bl.h"

extern uint32_t ENDIAN_DETECTOR;

/*
 Copy data from "fin" to "fout", starting at offset "offset"
 (from the beginning of "fin"), and length "length".
 Returns 0 on success.
 */
int pipe_file_offset(FILE* fin, int offset, int length, FILE* fout);

/*
 It's not really _safe_ as such, it just prints an error message if it fails...
 */
void
ATTRIB_FORMAT(printf,2,3)
asprintf_safe(char** strp, const char* format, ...);

int run_command_get_outputs(const char* cmd, sl** outlines, sl** errlines, const char** errormsg);

void get_mmap_size(int start, int size, off_t* mapstart, size_t* mapsize, int* pgap);

char* find_executable(const char* progname, const char* sibling);

bool file_exists(const char* fn);

bool file_readable(const char* fn);

bool file_executable(const char* fn);

void* file_get_contents(const char* fn, size_t* len, bool addzero);

char* file_get_contents_offset(const char* fn, int offset, int length);

sl* file_get_lines(const char* fn, bool include_newlines);

sl* fid_get_lines(FILE* fid, bool include_newlines);

sl* dir_get_contents(const char* path, sl* result, bool filesonly, bool recursive);

/**
   If "cmdline" starts with "keyword", returns 1 and places the address of
   the start of the next word in "p_next_word".
 */
int is_word(const char* cmdline, const char* keyword, char** p_next_word);

int starts_with(const char* str, const char* prefix);

int ends_with(const char* str, const char* prefix);

char* strdup_safe(const char* str);

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
char* read_string_terminated(FILE* fin, const char* terminators, int nterminators,
							 bool include_terminator);

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
typedef struct buffered_read_data bread_t;

bread_t* buffered_read_new(int elementsize, int Nbuffer, int Ntotal,
                           int (*refill_buffer)(void* userdata, void* buffer, unsigned int offs, unsigned int nelems),
                           void* userdata);

void* buffered_read(bread_t* buff);

void buffered_read_pushback(bread_t* br);

void buffered_read_reset(bread_t* br);

void buffered_read_free(bread_t* br);

void buffered_read_resize(bread_t* br, int newsize);

#endif