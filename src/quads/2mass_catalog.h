/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef TWOMASS_FITS_H
#define TWOMASS_FITS_H

#include <stdio.h>
#include <sys/types.h>

#include "qfits.h"
#include "ioutils.h"
#include "2mass.h"

#define TWOMASS_FITS_COLUMNS 76

struct twomass_catalog {
	qfits_table* table;
	int columns[TWOMASS_FITS_COLUMNS];
   unsigned int nentries;
	// buffered reading
	bread br;
	// when writing:
	qfits_header* header;
	FILE* fid;
	off_t header_end;
};
typedef struct twomass_catalog twomass_catalog;

twomass_catalog* twomass_catalog_open(char* fn);

twomass_catalog* twomass_catalog_open_for_writing(char* fn);

int twomass_catalog_write_headers(twomass_catalog* cat);

int twomass_catalog_fix_headers(twomass_catalog* cat);

int twomass_catalog_read_entries(twomass_catalog* cat, uint offset,
								 uint count, twomass_entry* entries);

twomass_entry* twomass_catalog_read_entry(twomass_catalog* cat);

int twomass_catalog_count_entries(twomass_catalog* cat);

int twomass_catalog_close(twomass_catalog* cat);

int twomass_catalog_write_entry(twomass_catalog* cat, twomass_entry* entry);

#endif
