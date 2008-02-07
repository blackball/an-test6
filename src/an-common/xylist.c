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
    ls->table = anfits_table_open(fn);
    if (!ls->table) {
		fprintf(stderr, "Failed to open FITS table %s.\n", fn);
        free(ls);
        return NULL;
    }
	ls->antype = qfits_pretty_string(qfits_header_getstr(ls->table->primheader, "AN_FILE"));
	ls->nfields = qfits_query_n_ext(fn);
    ls->field = -1;
	return ls;
}


