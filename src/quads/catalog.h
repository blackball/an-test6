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

#ifndef CATUTILS_H
#define CATUTILS_H

#include <sys/types.h>
#include "qfits.h"

#define AN_FILETYPE_CATALOG "OBJS"

struct catalog {
	uint numstars;
	double ramin;
	double ramax;
	double decmin;
	double decmax;
	double* stars;
	int healpix;

	FILE* fid;
	void* mmap_cat;
	size_t mmap_cat_size;
	qfits_header* header;
	off_t header_end;
};
typedef struct catalog catalog;

// if "modifiable" is non-zero, a private copy-on-write is made.
// (changes don't automatically get written to the file.)

catalog* catalog_open(char* catfn, int modifiable);

catalog* catalog_open_for_writing(char* catfn);

double* catalog_get_star(catalog* cat, uint sid);

double* catalog_get_base(catalog* cat);

int catalog_write_star(catalog* cat, double* star);

int catalog_close(catalog* cat);

void catalog_compute_radecminmax(catalog* cat);

int catalog_write_header(catalog* cat);

int catalog_fix_header(catalog* cat);

int catalog_write_to_file(catalog* cat, char* fn);

#endif
