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
#include "fitstable.h"
#include "fitsioutils.h"
#include "an-bool.h"

void field_free_data(field_t* f) {
    if (!f) return;
    free(f->x);
    free(f->y);
    free(f->flux);
    free(f->background);
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
		fprintf(stderr, "Failed to open FITS table %s.\n", fn);
        free(ls);
        return NULL;
    }
    ls->table->extension = 0;

    hdr = fitstable_get_primary_header(ls->table);
	ls->antype = fits_get_dupstring(hdr, "AN_FILE");
	ls->nfields = qfits_query_n_ext(fn);
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

void xylist_set_antype(xylist_t* ls, const char* type) {
    free(ls->antype);
    ls->antype = strdup(type);
}

int xylist_close(xylist_t* ls) {
    int rtn = 0;
    if (ls->table) {
        if (fitstable_close(ls->table)) {
			fprintf(stderr, "Failed to close xylist table\n");
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

int xylist_write_field(xylist_t* ls, field_t* fld) {
    int i;
    assert(fld);
    for (i=0; i<fld->N; i++) {
        if (fitstable_write_row(ls->table, fld->x + i, fld->y + i,
                                ls->include_flux ? fld->flux + i : NULL,
                                ls->include_background ? fld->background + i : NULL))
            return -1;
        /*
         int ret = 0;
         if (ls->include_flux && ls->include_background)
         ret = fitstable_write_row(
         if (ls->include_flux)
         if (ls->include_background)
         else
         */
    }
    return 0;
}

field_t* xylist_read_field(xylist_t* ls, field_t* fld) {
    bool freeit = FALSE;
    tfits_type dubl = fitscolumn_double_type();

    if (!ls->table->table) {
        // FITS table not found.
        return NULL;
    }

    if (!fld) {
        fld = calloc(1, sizeof(field_t));
        freeit = TRUE;
    }

    fld->N = fitstable_nrows(ls->table);
    fld->x = fitstable_read_column(ls->table, ls->xname, dubl);
    fld->y = fitstable_read_column(ls->table, ls->yname, dubl);
    // umm, HACK
    fld->flux = fitstable_read_column(ls->table, "FLUX", dubl);
    fld->background = fitstable_read_column(ls->table, "BACKGROUND", dubl);

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

// Used when writing: start a new field.  Set up the table and header
// structures so that they can be added to before writing the field header.
void xylist_next_field(xylist_t* ls) {
    fitstable_next_extension(ls->table);
    if (is_writing(ls)) {
        fitstable_clear_table(ls->table);
    } else {
        fitstable_open_next_extension(ls->table);
    }
	ls->nfields++;
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
}

int xylist_write_header(xylist_t* ls) {
    // ensure we've added our columns to the table...
    xylist_get_header(ls);
    return fitstable_write_header(ls->table);
}

int xylist_fix_header(xylist_t* ls) {
    return fitstable_fix_header(ls->table);
    /*
     qfits_header* hdr = ls->table->primheader;
     qfits_header_mod(hdr, "AN_FILE", ls->antype, "Astrometry.net file type");
     return fitstable_fix_header(ls->table);
     */
}

