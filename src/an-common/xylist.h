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

#ifndef XYLIST_H
#define XYLIST_H

#include <stdio.h>
#include <sys/types.h>

#include "starutil.h"
#include "qfits.h"
#include "fitstable.h"

struct xy_t {
    double* x;
    double* y;
    double* flux;
    double* background;
    int N;
};
typedef struct xy_t xy_t;

double xy_getx(xy_t* f, int i);
double xy_gety(xy_t* f, int i);

void xy_setx(xy_t* f, int i, double x);
void xy_sety(xy_t* f, int i, double y);

int xy_n(xy_t* f);

// Just free the data, not the field itself.
void xy_free_data(xy_t* f);

#define AN_FILETYPE_XYLS "XYLS"

/*
  One table per field.
  One row per star.
*/
struct xylist_t {
	int parity;

    fitstable_t* table;

	char* antype; // Astrometry.net filetype string.

    const char* xname;
    const char* yname;
    const char* xunits;
    const char* yunits;
    tfits_type xtype;
    tfits_type ytype;

    bool include_flux;
    bool include_background;

    // When reading: total number of fields in this file.
    int nfields;
};
typedef struct xylist_t xylist_t;

xylist_t* xylist_open(const char* fn);

xylist_t* xylist_open_for_writing(const char* fn);

void xylist_set_antype(xylist_t* ls, const char* type);

void xylist_set_xname(xylist_t* ls, const char* name);
void xylist_set_yname(xylist_t* ls, const char* name);
void xylist_set_xtype(xylist_t* ls, tfits_type type);
void xylist_set_ytype(xylist_t* ls, tfits_type type);
void xylist_set_xunits(xylist_t* ls, const char* units);
void xylist_set_yunits(xylist_t* ls, const char* units);

void xylist_set_include_flux(xylist_t* ls, bool inc);

int xylist_write_primary_header(xylist_t* ls);

void xylist_next_field(xylist_t* ls);

int xylist_write_header(xylist_t* ls);

int xylist_write_field(xylist_t* ls, xy_t* fld);

// (input xy_t* is optional; if not given, a new one is allocated and returned.)
xy_t* xylist_read_field(xylist_t* ls, xy_t* fld);

int xylist_fix_header(xylist_t* ls);

int xylist_close(xylist_t* ls);

qfits_header* xylist_get_primary_header(xylist_t* ls);

qfits_header* xylist_get_header(xylist_t* ls);

/*
 // Is the given filename an xylist?
 int xylist_is_file_xylist(const char* fn, const char* xcolumn, const char* ycolumn,
 const char** reason);
 */

#endif
