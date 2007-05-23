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

#ifndef TYCHO2_FITS_H
#define TYCHO2_FITS_H

#include <stdio.h>

#include "qfits.h"
#include "tycho2.h"
#include "ioutils.h"

#define TYCHO2_FITS_COLUMNS 35

struct tycho2_fits {
	qfits_table* table;
	int columns[TYCHO2_FITS_COLUMNS];
	// buffered reading
	bread br;
	// when writing:
	qfits_header* header;
	FILE* fid;
	off_t header_end;
	uint nentries;
};
typedef struct tycho2_fits tycho2_fits;

tycho2_fits* tycho2_fits_open(char* fn);

tycho2_fits* tycho2_fits_open_for_writing(char* fn);

int tycho2_fits_write_headers(tycho2_fits* tycho2);

int tycho2_fits_fix_headers(tycho2_fits* tycho2);

int tycho2_fits_count_entries(tycho2_fits* tycho2);

tycho2_entry* tycho2_fits_read_entry(tycho2_fits* t);

int tycho2_fits_read_entries(tycho2_fits* tycho2, uint offset,
							 uint count, tycho2_entry* entries);

int tycho2_fits_close(tycho2_fits* tycho2);

int tycho2_fits_write_entry(tycho2_fits* tycho2, tycho2_entry* entry);

#endif
