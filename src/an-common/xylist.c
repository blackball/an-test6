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
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "xylist.h"
#include "fitstable.h"
#include "fitsioutils.h"
#include "an-bool.h"
#include "keywords.h"

ATTRIB_FORMAT(printf,1,2)
static void error(const char* format, ...) {
    va_list lst;
    va_start(lst, format);
    vfprintf(stderr, format, lst);
    va_end(lst);
}

double xy_getx(xy_t* f, int i) {
    assert(i < f->N);
    return f->x[i];
}

double xy_gety(xy_t* f, int i) {
    assert(i < f->N);
    return f->y[i];
}

void xy_setx(xy_t* f, int i, double val) {
    assert(i < f->N);
    f->x[i] = val;
}

void xy_sety(xy_t* f, int i, double val) {
    assert(i < f->N);
    f->y[i] = val;
}

void xy_set(xy_t* f, int i, double x, double y) {
    assert(i < f->N);
    f->x[i] = x;
    f->y[i] = y;
}

int xy_n(xy_t* f) {
    return f->N;
}

void xy_free_data(xy_t* f) {
    if (!f) return;
    free(f->x);
    free(f->y);
    free(f->flux);
    free(f->background);
}

void xy_free(xy_t* f) {
    xy_free_data(f);
    free(f);
}

xy_t* xy_alloc(int N, bool flux, bool back) {
    xy_t* xy = calloc(1, sizeof(xy_t));
    xy_alloc_data(xy, N, flux, back);
    return xy;
}

void xy_alloc_data(xy_t* f, int N, bool flux, bool back) {
    f->x = malloc(N * sizeof(double));
    f->y = malloc(N * sizeof(double));
    if (flux)
        f->flux = malloc(N * sizeof(double));
    if (back)
        f->background = malloc(N * sizeof(double));
    f->N = N;
}

double* xy_to_flat_array(xy_t* xy, double* arr) {
    int nr = 2;
    int i, ind;
    if (xy->flux)
        nr++;
    if (xy->background)
        nr++;

    if (!arr)
        arr = malloc(nr * xy_n(xy) * sizeof(double));

    ind = 0;
    for (i=0; i<xy->N; i++) {
        arr[ind] = xy->x[i];
        ind++;
        arr[ind] = xy->y[i];
        ind++;
        if (xy->flux) {
            arr[ind] = xy->flux[i];
            ind++;
        }
        if (xy->background) {
            arr[ind] = xy->background[i];
            ind++;
        }
    }
    return arr;
}

void xy_from_dl(xy_t* xy, dl* l, bool flux, bool back) {
    int i;
    int nr = 2;
    int ind;
    if (flux)
        nr++;
    if (back)
        nr++;

    xy_alloc_data(xy, dl_size(l)/nr, flux, back);
    ind = 0;
    for (i=0; i<xy->N; i++) {
        xy->x[i] = dl_get(l, ind);
        ind++;
        xy->y[i] = dl_get(l, ind);
        ind++;
        if (flux) {
            xy->flux[i] = dl_get(l, ind);
            ind++;
        }
        if (back) {
            xy->background[i] = dl_get(l, ind);
            ind++;
        }
    }
}


bool xylist_is_file_xylist(const char* fn, const char* xcolumn, const char* ycolumn,
                           char** reason) {
    int rtn;
    xylist_t* xyls = xylist_open(fn);
    if (!xyls) {
        if (reason) asprintf(reason, "Failed to open file %s", fn);
        return FALSE;
    }
    if (xcolumn)
        xylist_set_xname(xyls, xcolumn);
    if (ycolumn)
        xylist_set_yname(xyls, ycolumn);

    fitstable_add_read_column_struct(xyls->table, fitscolumn_double_type(),
                                     1, 0, fitscolumn_any_type(), xyls->xname, TRUE);
    fitstable_add_read_column_struct(xyls->table, fitscolumn_double_type(),
                                     1, 0, fitscolumn_any_type(), xyls->yname, TRUE);

    rtn = fitstable_read_extension(xyls->table, 1);
    xylist_close(xyls);
    if (rtn) {
        if (reason) asprintf(reason, "Failed to find a matching FITS table in extension 1 of file %s\n", fn);
        return FALSE;
    }

    return TRUE;
}

static xylist_t* xylist_new() {
    xylist_t* xy = calloc(1, sizeof(xylist_t));
    xy->xname = "X";
    xy->yname = "Y";
    xy->xtype = TFITS_BIN_TYPE_D;
    xy->ytype = TFITS_BIN_TYPE_D;
    return xy;
}

static bool is_writing(xylist_t* ls) {
    return (ls->table && ls->table->fid) ? TRUE : FALSE;
}

xylist_t* xylist_open(const char* fn) {
    qfits_header* hdr;
	xylist_t* ls = NULL;
	ls = xylist_new();
	if (!ls)
		return NULL;
    ls->table = fitstable_open(fn);
    if (!ls->table) {
		error("Failed to open FITS table %s.\n", fn);
        free(ls);
        return NULL;
    }
    ls->table->extension = 0;

    hdr = fitstable_get_primary_header(ls->table);
	ls->antype = fits_get_dupstring(hdr, "AN_FILE");
	ls->nfields = qfits_query_n_ext(fn);
    ls->include_flux = TRUE;
    ls->include_background = TRUE;
    //ls->field = -1;
	return ls;
}

xylist_t* xylist_open_for_writing(const char* fn) {
	xylist_t* ls;
    qfits_header* hdr;
	ls = xylist_new();
	if (!ls)
		goto bailout;
    ls->table = fitstable_open_for_writing(fn);
    if (!ls->table) {
        free(ls);
        return NULL;
    }
    // since we have to call xylist_next_field() before writing the first one...
    ls->table->extension = 0;

    xylist_set_antype(ls, AN_FILETYPE_XYLS);
    hdr = fitstable_get_primary_header(ls->table);
    qfits_header_add(hdr, "AN_FILE", ls->antype, "Astrometry.net file type", NULL);
    //ls->field = 1;
	return ls;

 bailout:
	if (ls)
		free(ls);
	return NULL;
}

int xylist_add_tagalong_column(xylist_t* ls, tfits_type c_type,
                               int arraysize, tfits_type fits_type,
                               const char* name, const char* units) {
    fitstable_add_write_column_struct(ls->table, c_type, arraysize,
                                      0, fits_type, name, units);
    return fitstable_ncols(ls->table) - 1;
}

int xylist_write_tagalong_column(xylist_t* ls, int colnum,
                                 int offset, int N,
                                 void* data, int datastride) {
    return fitstable_write_one_column(ls->table, colnum, offset, N,
                                      data, datastride);
}

void* xylist_read_tagalong_column(xylist_t* ls, const char* colname,
                                  tfits_type c_type) {
    return fitstable_read_column_array(ls->table, colname, c_type);
}

void xylist_set_antype(xylist_t* ls, const char* type) {
    free(ls->antype);
    ls->antype = strdup(type);
}

int xylist_close(xylist_t* ls) {
    int rtn = 0;
    if (ls->table) {
        if (fitstable_close(ls->table)) {
			error("Failed to close xylist table\n");
            rtn = -1;
        }
    }
    free(ls->antype);
	free(ls);
	return rtn;
}

void xylist_set_xname(xylist_t* ls, const char* name) {
    ls->xname = name;
}
void xylist_set_yname(xylist_t* ls, const char* name) {
    ls->yname = name;
}
void xylist_set_xtype(xylist_t* ls, tfits_type type) {
    ls->xtype = type;
}
void xylist_set_ytype(xylist_t* ls, tfits_type type) {
    ls->ytype = type;
}
void xylist_set_xunits(xylist_t* ls, const char* units) {
    ls->xunits = units;
}
void xylist_set_yunits(xylist_t* ls, const char* units) {
    ls->yunits = units;
}

void xylist_set_include_flux(xylist_t* ls, bool inc) {
    ls->include_flux = inc;
}

void xylist_set_include_background(xylist_t* ls, bool inc) {
    ls->include_background = inc;
}

int xylist_n_fields(xylist_t* ls) {
    return ls->nfields;
}

int xylist_write_one_row(xylist_t* ls, xy_t* fld, int row) {
    return fitstable_write_row(ls->table, fld->x + row, fld->y + row,
                               ls->include_flux ? fld->flux + row : NULL,
                               ls->include_background ? fld->background + row : NULL);
}

int xylist_write_field(xylist_t* ls, xy_t* fld) {
    int i;
    assert(fld);
    for (i=0; i<fld->N; i++) {
        if (fitstable_write_row(ls->table, fld->x + i, fld->y + i,
                                ls->include_flux ? fld->flux + i : NULL,
                                ls->include_background ? fld->background + i : NULL))
            return -1;
    }
    return 0;
}

xy_t* xylist_read_field(xylist_t* ls, xy_t* fld) {
    bool freeit = FALSE;
    tfits_type dubl = fitscolumn_double_type();

    if (!ls->table->table) {
        // FITS table not found.
        return NULL;
    }

    if (!fld) {
        fld = calloc(1, sizeof(xy_t));
        freeit = TRUE;
    }

    fld->N = fitstable_nrows(ls->table);
    fld->x = fitstable_read_column(ls->table, ls->xname, dubl);
    fld->y = fitstable_read_column(ls->table, ls->yname, dubl);
    if (ls->include_flux)
        fld->flux = fitstable_read_column(ls->table, "FLUX", dubl);
    else
        fld->flux = NULL;
    if (ls->include_background)
        fld->background = fitstable_read_column(ls->table, "BACKGROUND", dubl);
    else
        fld->background = NULL;

    if (!(fld->x && fld->y)) {
        free(fld->x);
        free(fld->y);
        free(fld->flux);
        free(fld->background);
        if (freeit)
            free(fld);
        return NULL;
    }
    return fld;
}

xy_t* xylist_read_field_num(xylist_t* ls, int ext, xy_t* fld) {
	xy_t* rtn;
	if (xylist_open_field(ls, ext)) {
		error("xylist_open_field(%i) failed\n", ext);
        return NULL;
    }
	rtn = xylist_read_field(ls, fld);
	if (!rtn)
		error("xylist_read_field() failed\n");
    return rtn;
}

int xylist_open_field(xylist_t* ls, int i) {
    return fitstable_open_extension(ls->table, i);
}

/*
 int xylist_start_field(xylist_t* ls) {
 return xylist_next_field(ls);
 }
 */

// Used when writing: start a new field.  Set up the table and header
// structures so that they can be added to before writing the field header.
int xylist_next_field(xylist_t* ls) {
    fitstable_next_extension(ls->table);
    if (is_writing(ls)) {
        fitstable_clear_table(ls->table);
        ls->nfields++;
    } else {
        int rtn = fitstable_open_next_extension(ls->table);
        if (rtn)
            return rtn;
    }
    return 0;
}

qfits_header* xylist_get_primary_header(xylist_t* ls) {
    qfits_header* hdr;
    hdr = fitstable_get_primary_header(ls->table);
    qfits_header_mod(hdr, "AN_FILE", ls->antype, "Astrometry.net file type");
    return hdr;
}

qfits_header* xylist_get_header(xylist_t* ls) {
    if (!ls->table->header) {
        fitstable_add_write_column_convert(ls->table, ls->xtype,
                                           fitscolumn_double_type(),
                                           ls->xname, ls->xunits);
        fitstable_add_write_column_convert(ls->table, ls->ytype,
                                           fitscolumn_double_type(),
                                           ls->yname, ls->yunits);

        if (ls->include_flux)
            fitstable_add_write_column_convert(ls->table,
                                               fitscolumn_double_type(),
                                               fitscolumn_double_type(),
                                               "FLUX", "fluxunits");
        if (ls->include_background)
            assert(0);

        fitstable_new_table(ls->table);
    }
    return fitstable_get_header(ls->table);
}

int xylist_write_primary_header(xylist_t* ls) {
    // ensure we've added the AN_FILE header...
    xylist_get_primary_header(ls);
    return fitstable_write_primary_header(ls->table);
}

int xylist_fix_primary_header(xylist_t* ls) {
    return fitstable_fix_primary_header(ls->table);
    /*
     qfits_header* hdr = ls->table->primheader;
     qfits_header_mod(hdr, "AN_FILE", ls->antype, "Astrometry.net file type");
     return fitstable_fix_header(ls->table);
     */
}

int xylist_write_header(xylist_t* ls) {
    // ensure we've added our columns to the table...
    xylist_get_header(ls);
    return fitstable_write_header(ls->table);
}

int xylist_fix_header(xylist_t* ls) {
    return fitstable_fix_header(ls->table);
}

