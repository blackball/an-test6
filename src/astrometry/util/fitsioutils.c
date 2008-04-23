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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>

#include "qfits.h"
#include "fitsioutils.h"
#include "ioutils.h"
#include "keywords.h"
#include "an-endian.h"
#include "errors.h"
#include "qfits_error.h"

static void errfunc(char* errstr) {
    report_error("qfits", -1, "%s", errstr);
}

void fits_use_error_system() {
    qfits_err_remove_all();
    qfits_err_register(errfunc);
    qfits_err_statset(1);
}

int fits_convert_data(void* vdest, int deststride, tfits_type desttype,
                      const void* vsrc, int srcstride, tfits_type srctype,
                      int arraysize, int N) {
    int i, j;
    char* dest = vdest;
    const char* src = vsrc;
    int destatomsize = fits_get_atom_size(desttype);
    int srcatomsize = fits_get_atom_size(srctype);

    // this loop is over rows of data
    for (i=0; i<N; i++) {
        // store local pointers to we can stride over the array, without
        // affecting the row stride.
        char* adest = dest;
        const char* asrc = src;
        int64_t ival = 0;
        double  dval = 0;
        bool src_is_int = TRUE;

        // this loop is over elements of the array, if the column contains an array.
        // (ie, for scalar columns, arraysize is 1.)
        for (j=0; j<arraysize; j++) {
            switch (srctype) {
            case TFITS_BIN_TYPE_A:
            case TFITS_BIN_TYPE_X:
            case TFITS_BIN_TYPE_B:
                ival = *((uint8_t*)asrc);
                break;
            case TFITS_BIN_TYPE_L:
                // these are actually the characters 'T' and 'F'.
                ival = *((uint8_t*)asrc);
                if (ival == 'T')
                    ival = 1;
                else
                    ival = 0;
                break;
            case TFITS_BIN_TYPE_I:
                ival = *((int16_t*)asrc);
                break;
            case TFITS_BIN_TYPE_J:
                ival = *((int32_t*)asrc);
                break;
            case TFITS_BIN_TYPE_K:
                ival = *((int64_t*)asrc);
                break;
            case TFITS_BIN_TYPE_E:
                dval = *((float*)asrc);
                src_is_int = FALSE;
                break;
            case TFITS_BIN_TYPE_D:
                dval = *((double*)asrc);
                src_is_int = FALSE;
                break;
            default:
                fprintf(stderr, "fits_convert_data: unknown source type %i\n", srctype);
                assert(0);
                return -1;
            }

            switch (desttype) {
            case TFITS_BIN_TYPE_A:
            case TFITS_BIN_TYPE_X:
            case TFITS_BIN_TYPE_B:
                *((uint8_t*)adest) = (src_is_int ? ival : dval);
                break;
            case TFITS_BIN_TYPE_L:
                *((char*)adest) = (src_is_int ? ival : dval) ? 'T' : 'F';
                break;
            case TFITS_BIN_TYPE_I:
                *((int16_t*)adest) = (src_is_int ? ival : dval);
                break;
            case TFITS_BIN_TYPE_J:
                *((int32_t*)adest) = (src_is_int ? ival : dval);
                break;
            case TFITS_BIN_TYPE_K:
                *((int64_t*)adest) = (src_is_int ? ival : dval);
                break;
            case TFITS_BIN_TYPE_E:
                *((float*)adest) = (src_is_int ? ival : dval);
                break;
            case TFITS_BIN_TYPE_D:
                *((double*)adest) = (src_is_int ? ival : dval);
                break;
            default:
                fprintf(stderr, "fits_convert_data: unknown destination type %i\n", desttype);
                assert(0);
                return -1;
            }

            asrc  += srcatomsize;
            adest += destatomsize;
        }

        dest += deststride;
        src  +=  srcstride;
    }
    return 0;
}

double fits_get_double_val(const qfits_table* table, int column,
                           const void* rowdata) {
    const unsigned char* cdata = rowdata;
    double dval;
    float fval;

    // oh, the insanity of qfits...
    cdata += (table->col[column].off_beg - table->col[0].off_beg);
    if (table->col[column].atom_type == TFITS_BIN_TYPE_E) {
        memcpy(&fval, cdata, sizeof(fval));
        v32_ntoh(&fval);
        dval = fval;
        return fval;
    } else if (table->col[column].atom_type == TFITS_BIN_TYPE_D) {
        memcpy(&dval, cdata, sizeof(dval));
        v64_ntoh(&dval);
        return dval;
    } else {
        fprintf(stderr, "Invalid column type %i.\n", table->col[column].atom_type);
    }
    return HUGE_VAL;
}

int fits_is_table_header(const char* key) {
    return (!strcasecmp(key, "XTENSION") ||
            !strcasecmp(key, "BITPIX") ||
            !strncasecmp(key, "NAXIS...", 5) ||
            !strcasecmp(key, "PCOUNT") ||
            !strcasecmp(key, "GCOUNT") ||
            !strcasecmp(key, "TFIELDS") ||
            !strncasecmp(key, "TFORM...", 5) ||
            !strncasecmp(key, "TTYPE...", 5) ||
            !strncasecmp(key, "TUNIT...", 5) ||
            !strncasecmp(key, "TNULL...", 5) ||
            !strncasecmp(key, "TSCAL...", 5) ||
            !strncasecmp(key, "TZERO...", 5) ||
            !strncasecmp(key, "TDISP...", 5) ||
            !strncasecmp(key, "THEAP...", 5) ||
            !strncasecmp(key, "TDIM...", 4) ||
            !strcasecmp(key, "END")) ? 1 : 0;
}

int fits_is_primary_header(const char* key) {
    return (!strcasecmp(key, "SIMPLE") ||
            !strcasecmp(key, "BITPIX") ||
            !strncasecmp(key, "NAXIS...", 5) ||
            !strcasecmp(key, "EXTEND") ||
            !strcasecmp(key, "END")) ? 1 : 0;
}

void fits_copy_non_table_headers(qfits_header* dest, const qfits_header* src) {
    char key[FITS_LINESZ+1];
    char val[FITS_LINESZ+1];
    char com[FITS_LINESZ+1];
    char lin[FITS_LINESZ+1];
    int i;
    for (i=0; i<src->n; i++) {
        qfits_header_getitem(src, i, key, val, com, lin);
        if (fits_is_table_header(key))
            continue;
        qfits_header_add(dest, key, val, com, lin);
    }
}

char* fits_get_dupstring(qfits_header* hdr, const char* key) {
	return strdup_safe(qfits_pretty_string(qfits_header_getstr(hdr, key)));
}

void fits_header_addf(qfits_header* hdr, const char* key, const char* comment,
                      const char* format, ...) {
    char buf[FITS_LINESZ + 1];
    va_list lst;
    va_start(lst, format);
    vsnprintf(buf, sizeof(buf), format, lst);
    qfits_header_add(hdr, key, buf, comment, NULL);
    va_end(lst);
}

void fits_header_modf(qfits_header* hdr, const char* key, const char* comment,
                      const char* format, ...) {
    char buf[FITS_LINESZ + 1];
    va_list lst;
    va_start(lst, format);
    vsnprintf(buf, sizeof(buf), format, lst);
    qfits_header_mod(hdr, key, buf, comment);
    va_end(lst);
}

void fits_header_add_double(qfits_header* hdr, const char* key, double val,
                            const char* comment) {
    fits_header_addf(hdr, key, comment, "%.12G", val);
}

void fits_header_mod_double(qfits_header* hdr, const char* key, double val,
                            const char* comment) {
    fits_header_modf(hdr, key, comment, "%.12G", val);
}

void fits_header_mod_int(qfits_header* hdr, const char* key, int val,
                         const char* comment) {
    fits_header_modf(hdr, key, comment, "%i", val);
}

void fits_header_add_int(qfits_header* hdr, const char* key, int val,
                         const char* comment) {
    fits_header_addf(hdr, key, comment, "%i", val);
}

int fits_update_value(qfits_header* hdr, const char* key, const char* newvalue) {
  char oldcomment[FITS_LINESZ + 1];
  char* c = qfits_header_getcom(hdr, key);
  if (!c) {
    return -1;
  }
  // hmm, not sure I need to copy this...
  strncpy(oldcomment, c, FITS_LINESZ);
  qfits_header_mod(hdr, key, newvalue, oldcomment);
  return 0;
}

static int add_long_line(qfits_header* hdr, const char* keyword, const char* indent, int append, const char* format, va_list lst) {
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
        if (append)
            qfits_header_append(hdr, keyword, copy, NULL, NULL);
        else
            qfits_header_add(hdr, keyword, copy, NULL, NULL);
		len -= nchars;
		str += nchars;
	} while (len > 0);
	free(origstr);
	return 0;
}

static int 
ATTRIB_FORMAT(printf,4,5)
add_long_line_b(qfits_header* hdr, const char* keyword,
                const char* indent, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
	rtn = add_long_line(hdr, keyword, indent, 0, format, lst);
	va_end(lst);
	return rtn;
}

int 
fits_add_long_comment(qfits_header* dst, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
	rtn = add_long_line(dst, "COMMENT", "  ", 0, format, lst);
	va_end(lst);
	return rtn;
}

int 
fits_append_long_comment(qfits_header* dst, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
	rtn = add_long_line(dst, "COMMENT", "  ", 1, format, lst);
	va_end(lst);
	return rtn;
}

int 
fits_add_long_history(qfits_header* dst, const char* format, ...) {
	va_list lst;
	int rtn;
	va_start(lst, format);
	rtn = add_long_line(dst, "HISTORY", "  ", 0, format, lst);
	va_end(lst);
	return rtn;
}

int fits_add_args(qfits_header* hdr, char** args, int argc) {
    int i;
	for (i=0; i<argc; i++) {
		const char* str = args[i];
		int rtn;
		rtn = add_long_line_b(hdr, "HISTORY", "  ", "%s", str);
		if (rtn)
			return rtn;
	}
	return 0;
}

int fits_copy_header(const qfits_header* src, qfits_header* dest, char* key) {
	char* str = qfits_header_getstr(src, key);
	if (!str) {
		// header not found, or other problem.
		return -1;
	}
	qfits_header_add(dest, key, str,
						qfits_header_getcom(src, key), NULL);
	return 0;
}

int fits_copy_all_headers(const qfits_header* src, qfits_header* dest, char* targetkey) {
	int i;
	char key[FITS_LINESZ+1];
	char val[FITS_LINESZ+1];
	char com[FITS_LINESZ+1];
	char lin[FITS_LINESZ+1];

	for (i=0; i<src->n; i++) {
		qfits_header_getitem(src, i, key, val, com, lin);
		if (targetkey && strcasecmp(key, targetkey))
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
					int ncopies, const char* units, const char* label) {
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

int fits_write_data_D(FILE* fid, double value) {
	assert(sizeof(double) == 8);
	v64_hton(&value);
	if (fwrite(&value, 8, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a double to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_E(FILE* fid, float value) {
	assert(sizeof(float) == 4);
	v32_hton(&value);
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

int fits_write_data_L(FILE* fid, char value) {
    return fits_write_data_A(fid, value);
}

int fits_write_data_A(FILE* fid, char value) {
	return fits_write_data_B(fid, value);
}

int fits_write_data_X(FILE* fid, unsigned char value) {
	return fits_write_data_B(fid, value);
}

int fits_write_data_I(FILE* fid, int16_t value) {
	v16_hton(&value);
	if (fwrite(&value, 2, 1, fid) != 1) {
		fprintf(stderr, "Failed to write a short to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_J(FILE* fid, int32_t value) {
	v32_hton(&value);
	if (fwrite(&value, 4, 1, fid) != 1) {
		fprintf(stderr, "Failed to write an int to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_K(FILE* fid, int64_t value) {
	v64_hton(&value);
	if (fwrite(&value, 8, 1, fid) != 1) {
		fprintf(stderr, "Failed to write an int64 to FITS file: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int fits_write_data_array(FILE* fid, const void* vvalue, tfits_type type,
                          int N) {
    int i;
    int rtn = 0;
    const char* pvalue = (const char*)vvalue;

    if (pvalue == NULL) {
        if (fseeko(fid, fits_get_atom_size(type) * N, SEEK_CUR)) {
            fprintf(stderr, "Failed to skip %i bytes in fits_write_data_array: %s\n",
                    fits_get_atom_size(type) * N, strerror(errno));
            return -1;
        }
        return 0;
    }

    for (i=0; i<N; i++) {
        switch (type) {
        case TFITS_BIN_TYPE_A:
            rtn = fits_write_data_A(fid, *(unsigned char*)pvalue);
            pvalue += sizeof(unsigned char);
            break;
        case TFITS_BIN_TYPE_B:
            rtn = fits_write_data_B(fid, *(unsigned char*)pvalue);
            pvalue += sizeof(unsigned char);
            break;
        case TFITS_BIN_TYPE_L:
            rtn = fits_write_data_L(fid, *(bool*)pvalue);
            pvalue += sizeof(bool);
            break;
        case TFITS_BIN_TYPE_D:
            rtn = fits_write_data_D(fid, *(double*)pvalue);
            pvalue += sizeof(double);
            break;
        case TFITS_BIN_TYPE_E:
            rtn = fits_write_data_E(fid, *(float*)pvalue);
            pvalue += sizeof(float);
            break;
        case TFITS_BIN_TYPE_I:
            rtn = fits_write_data_I(fid, *(int16_t*)pvalue);
            pvalue += sizeof(int16_t);
            break;
        case TFITS_BIN_TYPE_J:
            rtn = fits_write_data_J(fid, *(int32_t*)pvalue);
            pvalue += sizeof(int32_t);
            break;
        case TFITS_BIN_TYPE_K:
            rtn = fits_write_data_K(fid, *(int64_t*)pvalue);
            pvalue += sizeof(int64_t);
            break;
        case TFITS_BIN_TYPE_X:
            rtn = fits_write_data_X(fid, *(unsigned char*)pvalue);
            pvalue += sizeof(unsigned char);
            break;
        default:
            fprintf(stderr, "fitsioutils: fits_write_data: unknown data type %i.\n", type);
            rtn = -1;
            break;
        }
        if (rtn)
            break;
    }
    return rtn;
}

int fits_write_data(FILE* fid, void* pvalue, tfits_type type) {
    return fits_write_data_array(fid, pvalue, type, 1);
}

int fits_bytes_needed(int size) {
	size += FITS_BLOCK_SIZE - 1;
	return size - (size % FITS_BLOCK_SIZE);
}

int fits_blocks_needed(int size) {
	return (size + FITS_BLOCK_SIZE - 1) / FITS_BLOCK_SIZE;
}

static char fits_endian_string[16];
static int  fits_endian_string_inited = 0;

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

qfits_table* fits_get_table_column(const char* fn, const char* colname, int* pcol) {
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

int fits_find_table_column(const char* fn, const char* colname, int* pstart, int* psize, int* pext) {
    int i, nextens;
	nextens = qfits_query_n_ext(fn);
	for (i=1; i<=nextens; i++) {
        qfits_table* table;
        int c;
        table = qfits_table_open(fn, i);
		if (!table) {
			//fprintf(stderr, "Couldn't read FITS table from file %s, extension %i.\n", fn, i);
			continue;
		}
		c = fits_find_column(table, colname);
		qfits_table_close(table);
		if (c == -1) {
			continue;
		}
		if (qfits_get_datinfo(fn, i, pstart, psize) == -1) {
			fprintf(stderr, "error getting start/size for ext %i.\n", i);
            return -1;
        }
		if (pext) *pext = i;
		return 0;
    }
    return -1;
}

int fits_find_column(const qfits_table* table, const char* colname) {
	int c;
	for (c=0; c<table->nc; c++) {
		qfits_col* col = table->col + c;
		if (strcasecmp(col->tlabel, colname) == 0)
			return c;
	}
	return -1;
}

void fits_add_uint_size(qfits_header* header) {
	fits_header_add_int(header, "UINT_SZ", sizeof(uint), "sizeof(uint)");
}

void fits_add_double_size(qfits_header* header) {
	fits_header_add_int(header, "DUBL_SZ", sizeof(double), "sizeof(double)");
}

int fits_check_uint_size(const qfits_header* header) {
  int uintsz;
  uintsz = qfits_header_getint(header, "UINT_SZ", -1);
  if (sizeof(uint) != uintsz) {
    fprintf(stderr, "File was written with sizeof(uint)=%i, but currently sizeof(uint)=%u.\n",
	    uintsz, (uint)sizeof(uint));
        return -1;
	}
    return 0;
}

int fits_check_double_size(const qfits_header* header) {
    int doublesz;
    doublesz = qfits_header_getint(header, "DUBL_SZ", -1);
    if (sizeof(double) != doublesz) {
      fprintf(stderr, "File was written with sizeof(double)=%i, but currently sizeof(double)=%u.\n",
	      doublesz, (uint)sizeof(double));
      return -1;
	}
    return 0;
}

int fits_check_endian(const qfits_header* header) {
    char* filestr;
    char* localstr;

	filestr = qfits_pretty_string(qfits_header_getstr(header, "ENDIAN"));
    if (!filestr) {
        // No ENDIAN header found.
        return 1;
    }
    localstr = fits_get_endian_string();
	if (strcmp(filestr, localstr)) {
		fprintf(stderr, "File was written with endianness %s, this machine has endianness %s.\n", filestr, localstr);
		return -1;
	}
    return 0;
}