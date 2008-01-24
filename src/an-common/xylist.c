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

#include "xylist.h"
#include "fitsioutils.h"

#define X_INDEX 0
#define Y_INDEX 1

static void add_cols(fitstable_t* t, int parity, tfits_type type) {
    fitscol_t col;

    memset(&col, 0, sizeof(fitscol_t));
    col.target_fitstype = type;
    col.target_arraysize = 1;
    col.arraysize = 1;
    col.fitstype = type;
    col.ctype = fitscolumn_double_type();
    col.required = TRUE;
    col.in_struct = TRUE;

    col.colname = "X";
    col.coffset = (parity ? sizeof(double) : 0);
    fitstable_add_column(t, &col);
    col.colname = "Y";
    col.coffset = (parity ? 0 : sizeof(double));
    fitstable_add_column(t, &col);
}

// Is the given filename an xylist?
int xylist_is_file_xylist(const char* fn,
                          const char* xcolumn, const char* ycolumn,
                          const char** reason) {
    fitstable_t* t;
    int rtn = 0;
    t = fitstable_open(fn);
    if (!t) {
        if (reason) *reason = "Not a FITS table?";
        return rtn;
    }
    add_cols(t, 0, fitscolumn_any_type());
    if (xcolumn)
        fitstable_get_column(t, X_INDEX)->colname = xcolumn;
    if (ycolumn)
        fitstable_get_column(t, Y_INDEX)->colname = ycolumn;
    if (fitstable_read_extension(t, 1)) {
        if (reason) *reason = "Failed to find matching columns.";
    } else {
        // Got it!

        rtn = 1;
    }
    fitstable_close(t);
    return rtn;
}

qfits_header* xylist_get_header(xylist* ls) {
    return ls->table->primheader;
}

static fitscol_t* xcolumn(xylist* ls) {
    return fitstable_get_column(ls->table, X_INDEX);
}
static fitscol_t* ycolumn(xylist* ls) {
    return fitstable_get_column(ls->table, Y_INDEX);
}

void xylist_set_xname(xylist* ls, const char* name) {
    fitscol_t* col = xcolumn(ls);
    col->colname = name;
}
void xylist_set_yname(xylist* ls, const char* name) {
    fitscol_t* col = ycolumn(ls);
    col->colname = name;
}
void xylist_set_xtype(xylist* ls, tfits_type type) {
    fitscol_t* col = xcolumn(ls);
    col->target_fitstype = col->fitstype = type;
}
void xylist_set_ytype(xylist* ls, tfits_type type) {
    fitscol_t* col = ycolumn(ls);
    col->target_fitstype = col->fitstype = type;
}
void xylist_set_xunits(xylist* ls, const char* units) {
    fitscol_t* col = xcolumn(ls);
    col->units = units;
}
void xylist_set_yunits(xylist* ls, const char* units) {
    fitscol_t* col = ycolumn(ls);
    col->units = units;
}

static xylist* xylist_new() {
	xylist* ls;
	ls = calloc(1, sizeof(xylist));
	if (!ls) {
		fprintf(stderr, "Couldn't allocate an xylist.\n");
		return NULL;
	}
	return ls;
}

xylist* xylist_open(const char* fn) {
	xylist* ls = NULL;
	ls = xylist_new();
	if (!ls)
		return NULL;
    ls->table = fitstable_open(fn);
    if (!ls->table) {
		fprintf(stderr, "Failed to open FITS table %s.\n", fn);
        free(ls);
        return NULL;
    }
    add_cols(ls->table, ls->parity, fitscolumn_any_type());
	ls->antype = qfits_pretty_string(qfits_header_getstr(ls->table->primheader, "AN_FILE"));
	ls->nfields = qfits_query_n_ext(fn);
    ls->field = -1;
	return ls;
}

xylist* xylist_open_for_writing(char* fn) {
	xylist* ls;
	ls = xylist_new();
	if (!ls)
		goto bailout;
    ls->table = fitstable_open_for_writing(fn);
    if (!ls->table) {
        free(ls);
        return NULL;
    }
    add_cols(ls->table, ls->parity, fitscolumn_double_type());
	ls->antype = AN_FILETYPE_XYLS;
	//qfits_header_add(ls->table->primheader, "NFIELDS", "0", NULL, NULL);
	qfits_header_add(ls->table->primheader, "AN_FILE", ls->antype, NULL, NULL);
    ls->field = 1;
	return ls;

 bailout:
	if (ls)
		free(ls);
	return NULL;
}

int xylist_close(xylist* ls) {
    int rtn = 0;
    if (ls->table) {
        if (fitstable_close(ls->table)) {
			fprintf(stderr, "Failed to close xylist table\n");
            rtn = -1;
        }
    }
	free(ls);
	return rtn;
}

// Used when reading.  Opens a new field, if necessary.
static int xylist_open_field(xylist* ls, uint field) {
    assert(field > 0);
	if (ls->field == field)
		return 0;
    fitstable_close_table(ls->table);
    if (fitstable_read_extension(ls->table, field)) {
        fitstable_print_missing(ls->table, stderr);
        fitstable_close_table(ls->table);
        return -1;
    }
	ls->field = field;
	return 0;
}

// Used when writing: start a new field.  Set up the table and header
// structures so that they can be added to before writing the field header.
int xylist_new_field(xylist* ls) {
    fitstable_new_table(ls->table);
	//fits_header_add_int(ls->fieldheader, "FIELDNUM", ls->field, "This table is field number...");
	ls->nfields++;
	return 0;
}

int xylist_write_field_header(xylist* ls) {
    return fitstable_write_table_header(ls->table);
}

int xylist_fix_field(xylist* ls) {
    return fitstable_fix_table_header(ls->table);
}

int xylist_n_fields(xylist* ls) {
	return ls->nfields;
}

xy* xylist_get_field(xylist* ls, uint field) {
	int i, nobjs;
	double* vals;
	xy* rtn;
    assert(field > 0);
	nobjs = xylist_n_entries(ls, field);
	if (nobjs == -1) {
		fprintf(stderr, "Couldn't find number of entries in field %i.\n", field);
		return NULL;
	}
	rtn = mk_xy(nobjs);
	vals = malloc(2 * nobjs * sizeof(double));
	xylist_read_entries(ls, field, 0, nobjs, vals);
	for (i=0; i<nobjs; i++) {
		xy_setx(rtn, i, vals[i*2]);
		xy_sety(rtn, i, vals[i*2 + 1]);
	}
	free(vals);
	return rtn;
}

qfits_header* xylist_get_field_header(xylist* ls, uint field) {
    if (xylist_open_field(ls, field)) {
        fprintf(stderr, "Failed to open extension (field) %i\n", field);
        return NULL;
    }
    return ls->table->header;
}

int xylist_n_entries(xylist* ls, uint field) {
    assert(field > 0);
	if (xylist_open_field(ls, field)) {
		return -1;
	}
	return fitstable_nrows(ls->table);
}

int xylist_read_entries(xylist* ls, uint field, uint offset, uint n,
						double* vals) {
    assert(field > 0);
	if (xylist_open_field(ls, field)) {
		return -1;
	}
    if (fitstable_read_array(ls->table, offset, n, vals, 2 * sizeof(double))) {
        fprintf(stderr, "Failed to read XY data: field %i, offset %i, N %i.\n", field, offset, n);
        return -1;
    }
	return 0;
}

int xylist_write_header(xylist* ls) {
    return fitstable_write_header(ls->table);
}

int xylist_fix_header(xylist* ls) {
    qfits_header* hdr = ls->table->primheader;
	//fits_header_mod_int(hdr, "NFIELDS", ls->nfields, "Number of fields");
	qfits_header_mod(hdr, "AN_FILE", ls->antype, "Astrometry.net file type");
    return fitstable_fix_header(ls->table);
}

int xylist_write_new_field(xylist* ls) {
  return xylist_new_field(ls) || xylist_write_field_header(ls);
}

int xylist_write_entries(xylist* ls, double* vals, uint nvals) {
    fitstable_write_array(ls->table, 0, nvals, vals, 2*sizeof(double));
	return 0;
}

