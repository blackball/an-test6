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

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>

#include "qfits.h"
#include "fitsioutils.h"

#define Inline inline

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define IS_LITTLE_ENDIAN 1
#else
#define IS_LITTLE_ENDIAN 0
#endif

static uint32_t ENDIAN_DETECTOR = 0x01020304;

int fits_update_value(qfits_header* hdr, const char* key, const char* newvalue) {
  char oldcomment[FITS_LINESZ + 1];
  char* c = qfits_header_getcom(hdr, key);
  if (!c) {
    return -1;
  }
  // hmm, not sure I need to copy this...
  strncpy(oldcomment, c, FITS_LINESZ);
  qfits_header_mod(hdr, key, newvalue, oldcomment);
}

static int add_long_line(qfits_header* hdr, char* keyword, const char* indent, const char* format, va_list lst) {
	const int charsperline = 60;
	char* origstr = NULL;
	char* str;
	int len;
	int indlen = (indent ? strlen(indent) : 0);
	len = vasprintf(&origstr, format, lst);
	if (len == -1) {
		fprintf(stderr, "vasprintf failed: %s\n", strerror(errno));
		return -1;
	}
	str = origstr;
	do {
		char copy[80];
		int doindent = (indent && (str != origstr));
		int nchars = charsperline - (doindent ? indlen : 0);
		int brk;
		if (nchars > len)
			nchars = len;
		else {
			// look for a space to break the line.
			for (brk=nchars-1; (brk>=0) && (str[brk] != ' '); brk--);
			if (brk > 0) {
				// found a place to break the line.
				nchars = brk + 1;
			}
		}
		sprintf(copy, "%s%.*s", (doindent ? indent : ""), nchars, str);
		qfits_header_add(hdr, keyword, copy, NULL, NULL);
		len -= nchars;
		str += nchars;
	} while (len > 0);
	free(origstr);
	return 0;
}

static int 
add_long_line_b(qfits_header* hdr, char* keyword, const char* indent, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
	rtn = add_long_line(hdr, keyword, indent, format, lst);
	va_end(lst);
	return rtn;
}

int 
fits_add_long_comment(qfits_header* dst, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
	rtn = add_long_line(dst, "COMMENT", "  ", format, lst);
	va_end(lst);
	return rtn;
}

int 
fits_add_long_history(qfits_header* dst, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
	rtn = add_long_line(dst, "HISTORY", "  ", format, lst);
	va_end(lst);
	return rtn;
}

int fits_add_args(qfits_header* hdr, char** args, int argc) {
	int i;
	for (i=0; i<argc; i++) {
		char* str = args[i];
		int rtn;
		rtn = add_long_line_b(hdr, "HISTORY", "  ", "%s", str);
		if (rtn)
			return rtn;
	}
	return 0;
}

int fits_copy_header(qfits_header* src, qfits_header* dest, char* key) {
	char* str = qfits_header_getstr(src, key);
	if (!str) {
		// header not found, or other problem.
		return -1;
	}
	qfits_header_add(dest, key, str,
						qfits_header_getcom(src, key), NULL);
	return 0;
}

int fits_copy_all_headers(qfits_header* src, qfits_header* dest, char* targetkey) {
	int i;
	char key[FITS_LINESZ+1];
	char val[FITS_LINESZ+1];
	char com[FITS_LINESZ+1];
	char lin[FITS_LINESZ+1];

	for (i=0; i<src->n; i++) {
		qfits_header_getitem(src, i, key, val, com, lin);
		if (strcasecmp(key, targetkey))
			continue;
		qfits_header_add(dest, key, val, com, lin);
	}
	return 0;
}

int fits_pad_file(FILE* fid) {
	off_t offset;
	int npad;
	
	// pad with zeros up to a multiple of 2880 bytes.
	offset = ftello(fid);
	npad = (offset % FITS_BLOCK_SIZE);
	if (npad) {
		char nil = '\0';
		int i;
		npad = FITS_BLOCK_SIZE - npad;
		for (i=0; i<npad; i++)
			if (fwrite(&nil, 1, 1, fid) != 1) {
				fprintf(stderr, "Failed to pad FITS file: %s\n", strerror(errno));
				return -1;
			}
	}
	return 0;
}

int fits_get_atom_size(tfits_type type) {
	int atomsize = -1;
	switch (type) {
	case TFITS_BIN_TYPE_A:
	case TFITS_BIN_TYPE_X:
	case TFITS_BIN_TYPE_L:
	case TFITS_BIN_TYPE_B:
		atomsize = 1;
		break;
	case TFITS_BIN_TYPE_I:
		atomsize = 2;
		break;
	case TFITS_BIN_TYPE_J:
	case TFITS_BIN_TYPE_E:
		atomsize = 4;
		break;
	case TFITS_BIN_TYPE_K:
	case TFITS_BIN_TYPE_D:
		atomsize = 8;
		break;
	default:
		break;
	}
	return atomsize;
}

int fits_add_column(qfits_table* table, int column, tfits_type type,
					int ncopies, char* units, char* label) {
	int atomsize;
	int colsize;

	atomsize = fits_get_atom_size(type);
	if (atomsize == -1) {
		fprintf(stderr, "Unknown atom size for type %i.\n", type);
		return -1;
	}
	if (type == TFITS_BIN_TYPE_X)
		// bit field: convert bits to bytes, rounding up.
		ncopies = (ncopies + 7) / 8;
	colsize = atomsize * ncopies;
	qfits_col_fill(table->col + column, ncopies, 0, atomsize, type, label, units,
				   "", "", 0, 0, 0, 0, table->tab_w);
	table->tab_w += colsize;
	return 0;
}

static Inline void dstn_swap_bytes(unsigned char* c1, unsigned char* c2) {
	unsigned char tmp;
	tmp = *c1;
	*c1 = *c2;
	*c2 = tmp;
}

static Inline void hton64(void* ptr) {
#if IS_LITTLE_ENDIAN
	unsigned char* c = (unsigned char*)ptr;
	dstn_swap_bytes(c+0, c+7);
	dstn_swap_bytes(c+1, c+6);
	dstn_swap_bytes(c+2, c+5);
	dstn_swap_bytes(c+3, c+4);
#endif
}

static Inline void hton32(void* ptr) {
#if IS_LITTLE_ENDIAN
	unsigned char* c = (unsigned char*)ptr;
	dstn_swap_bytes(c+0, c+3);
	dstn_swap_bytes(c+1, c+2);
#endif
}

static Inline void hton16(void* ptr) {
#if IS_LITTLE_ENDIAN
	unsigned char* c = (unsigned char*)ptr;
	dstn_swap_bytes(c+0, c+1);
#endif
}

int fits_write_data_D(FILE* fid, double value) {
	assert(sizeof(double) == 8);
	hton64(&value);
	if (fwrite(&value, 8, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a double to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_E(FILE* fid, float value) {
	assert(sizeof(float) == 4);
	hton32(&value);
	if (fwrite(&value, 4, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a float to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_B(FILE* fid, unsigned char value) {
	if (fwrite(&value, 1, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a bit array to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_A(FILE* fid, unsigned char value) {
	return fits_write_data_B(fid, value);
}

int fits_write_data_X(FILE* fid, unsigned char value) {
	return fits_write_data_B(fid, value);
}

int fits_write_data_I(FILE* fid, int16_t value) {
	hton16(&value);
	if (fwrite(&value, 2, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a short to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_J(FILE* fid, int32_t value) {
	hton32(&value);
	if (fwrite(&value, 4, 1, fid) != 1) {
		fprintf(stderr, "Failed to write an int to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_K(FILE* fid, int64_t value) {
	hton64(&value);
	if (fwrite(&value, 8, 1, fid) != 1) {
		fprintf(stderr, "Failed to write an int64 to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data(FILE* fid, void* pvalue, tfits_type type) {
	switch (type) {
	case TFITS_BIN_TYPE_A:
		return fits_write_data_A(fid, *(unsigned char*)pvalue);
	case TFITS_BIN_TYPE_B:
		return fits_write_data_B(fid, *(unsigned char*)pvalue);
	case TFITS_BIN_TYPE_D:
		return fits_write_data_D(fid, *(double*)pvalue);
	case TFITS_BIN_TYPE_E:
		return fits_write_data_E(fid, *(float*)pvalue);
	case TFITS_BIN_TYPE_I:
		return fits_write_data_I(fid, *(int16_t*)pvalue);
	case TFITS_BIN_TYPE_J:
		return fits_write_data_J(fid, *(int32_t*)pvalue);
	case TFITS_BIN_TYPE_K:
		return fits_write_data_K(fid, *(int64_t*)pvalue);
	case TFITS_BIN_TYPE_X:
		return fits_write_data_X(fid, *(unsigned char*)pvalue);
	default: break;
	}
	fprintf(stderr, "fitsioutils: fits_write_data: unknown data type %i.\n", type);
	return -1;
}

// how many FITS blocks are required to hold 'size' bytes?
int fits_blocks_needed(int size) {
	size += FITS_BLOCK_SIZE - 1;
	return size - (size % FITS_BLOCK_SIZE);
}

char fits_endian_string[16];
int  fits_endian_string_inited = 0;

static void fits_init_endian_string() {
    if (!fits_endian_string_inited) {
        uint32_t endian = ENDIAN_DETECTOR;
        unsigned char* cptr = (unsigned char*)&endian;
        fits_endian_string_inited = 1;
        sprintf(fits_endian_string, "%02x:%02x:%02x:%02x", (uint)cptr[0], (uint)cptr[1], (uint)cptr[2], (uint)cptr[3]);
    }
}

void fits_fill_endian_string(char* str) {
    fits_init_endian_string();
    strcpy(str, fits_endian_string);
}

char* fits_get_endian_string() {
    fits_init_endian_string();
    return fits_endian_string;
}

void fits_add_endian(qfits_header* header) {
	qfits_header_add(header, "ENDIAN", fits_get_endian_string(), "Endianness detector: u32 0x01020304 written ", NULL);
	qfits_header_add(header, "", NULL, " in the order it is stored in memory.", NULL);
	// (don't make this a COMMENT because that makes it get separated from the ENDIAN header line.)
}

qfits_table* fits_get_table_column(char* fn, char* colname, int* pcol) {
    int i, nextens, start, size;

	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
        qfits_table* table;
        int c;
		if (qfits_get_datinfo(fn, i, &start, &size) == -1) {
			fprintf(stderr, "error getting start/size for ext %i.\n", i);
            return NULL;
        }
		if (!qfits_is_table(fn, i))
            continue;
        table = qfits_table_open(fn, i);
		if (!table) {
			fprintf(stderr, "Couldn't read FITS table from file %s, extension %i.\n",
					fn, i);
			continue;
		}
		c = fits_find_column(table, colname);
		if (c != -1) {
			*pcol = c;
			return table;
		}
		qfits_table_close(table);
    }
	return NULL;
}

int fits_find_table_column(char* fn, char* colname, int* pstart, int* psize) {
    int i, nextens, start, size;

	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
        qfits_table* table;
        int c;
		if (qfits_get_datinfo(fn, i, &start, &size) == -1) {
			fprintf(stderr, "error getting start/size for ext %i.\n", i);
            return -1;
        }
		//fprintf(stderr, "ext %i starts at %i, has size %i.\n", i, start, size);
		if (!qfits_is_table(fn, i))
            continue;
        table = qfits_table_open(fn, i);
		if (!table) {
			fprintf(stderr, "Couldn't read FITS table from file %s, extension %i.\n",
					fn, i);
			continue;
		}
		c = fits_find_column(table, colname);
		qfits_table_close(table);
		if (c != -1) {
			*pstart = start;
			*psize = size;
			return 0;
		}
    }
    return -1;
}

int fits_find_column(qfits_table* table, char* colname) {
	int c;
	for (c=0; c<table->nc; c++) {
		qfits_col* col = table->col + c;
		if (strcasecmp(col->tlabel, colname) == 0)
			return c;
	}
	return -1;
}

void fits_add_uint_size(qfits_header* header) {
    char val[8];
	sprintf(val, "%u", (uint)sizeof(uint));
	qfits_header_add(header, "UINT_SZ", val, "sizeof(uint)", NULL);
}

void fits_add_double_size(qfits_header* header) {
    char val[8];
    sprintf(val, "%u", (uint)sizeof(double));
	qfits_header_add(header, "DUBL_SZ", val, "sizeof(double)", NULL);
}

int fits_check_uint_size(qfits_header* header) {
  int uintsz;
  uintsz = qfits_header_getint(header, "UINT_SZ", -1);
  if (sizeof(uint) != uintsz) {
    fprintf(stderr, "File was written with sizeof(uint)=%i, but currently sizeof(uint)=%u.\n",
	    uintsz, (uint)sizeof(uint));
        return -1;
	}
    return 0;
}

int fits_check_double_size(qfits_header* header) {
    int doublesz;
    doublesz = qfits_header_getint(header, "DUBL_SZ", -1);
    if (sizeof(double) != doublesz) {
      fprintf(stderr, "File was written with sizeof(double)=%i, but currently sizeof(double)=%u.\n",
	      doublesz, (uint)sizeof(double));
      return -1;
	}
    return 0;
}

int fits_check_endian(qfits_header* header) {
	char endian[256];
    char* str;
	snprintf(endian, sizeof(endian), "%s", qfits_header_getstr(header, "ENDIAN"));
	// "endian" contains ' characters around the string, like
	// '04:03:02:01', so we start at index 1, and only compare the length of
	// str.
    str = fits_get_endian_string();
	if (strncmp(str, endian+1, strlen(str)) != 0) {
		fprintf(stderr, "File was written with endianness %s, this machine has endianness %s.\n", endian, str);
		return -1;
	}
    return 0;
}



