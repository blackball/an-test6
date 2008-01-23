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
#include "bl.h"
#include "fitstable.h"

typedef dl xy;
#define mk_xy(n) dl_new((n)*2)
#define free_xy(x) dl_free(x)
#define xy_ref(x, i) dl_get(x, i)
#define xy_refx(x, i) dl_get(x, 2*(i))
#define xy_refy(x, i) dl_get(x, (2*(i))+1)
#define xy_size(x) (dl_size(x)/2)
#define xy_set(x,i,v) dl_set(x,i,v)
#define xy_setx(x,i,v) dl_set(x,2*(i),v)
#define xy_sety(x,i,v) dl_set(x,2*(i)+1,v)

#define AN_FILETYPE_XYLS "XYLS"

/*
  One table per field.
  One row per star.
*/
struct xylist {
	int parity;

    fitstable_t* table;

	const char* antype; // Astrometry.net filetype string.

    // When reading: total number of fields in this file.
    int nfields;

	// field we're currently reading/writing
	unsigned int field;
};
typedef struct xylist xylist;


// add a FITS column that will piggy-back with the X,Y data.
// returns the index of the column.
int xylist_add_column(xylist* ls, fitscol_t* col);

// retrieves an extra column previously added to this xylist.
fitscol_t* xylist_get_column(const xylist* ls, int col);


// Is the given filename an xylist?
int xylist_is_file_xylist(const char* fn, const char* xcolumn, const char* ycolumn,
                          const char** reason);

void xylist_set_xname(xylist* ls, const char* name);
void xylist_set_yname(xylist* ls, const char* name);
void xylist_set_xtype(xylist* ls, tfits_type type);
void xylist_set_ytype(xylist* ls, tfits_type type);
void xylist_set_xunits(xylist* ls, const char* units);
void xylist_set_yunits(xylist* ls, const char* units);

qfits_header* xylist_get_header(xylist* ls);

// you can change the parameters (ie, xname, yname) 
// after opening but before calling xylist_get_field.
xylist* xylist_open(const char* fn);

int xylist_n_fields(xylist* ls);

// it's your responsibility to free_xy() this.
xy* xylist_get_field(xylist* ls, unsigned int field);

// it's your responsibility to call qfits_header_destroy().
qfits_header* xylist_get_field_header(xylist* ls, unsigned int field);

int xylist_n_entries(xylist* ls, unsigned int field);

int xylist_read_entries(xylist* ls, unsigned int field,
						unsigned int offset, unsigned int n,
						double* vals);

xylist* xylist_open_for_writing(char* fn);

// Write the header for the whole file
int xylist_write_header(xylist* ls);

// Fix the header for the whole file
int xylist_fix_header(xylist* ls);

// Start a new field and write its header.
int xylist_write_new_field(xylist* ls);

/*
  Just start a new field (you might want to use this function plus
  xylist_write_field_header instead of xylist_write_new_field
  if you want to add something to the header before writing it out).
*/
int xylist_new_field(xylist* ls);

// Write the current field's header.
int xylist_write_field_header(xylist* ls);

// Fix the header for the current field.  Call this after writing all data points.
int xylist_fix_field(xylist* ls);

// Write a set of data points
int xylist_write_entries(xylist* ls, double* vals, unsigned int nvals);

int xylist_close(xylist* ls);

#endif
