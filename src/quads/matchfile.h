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

#ifndef MATCHFILE_H
#define MATCHFILE_H

#include <stdio.h>

#include "matchobj.h"
#include "starutil.h"
#include "qfits.h"
#include "bl.h"
#include "ioutils.h"

#define MATCHFILE_AN_FILETYPE "MATCH"

#define MATCHFILE_FITS_COLUMNS 32

struct matchfile {
	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;

	// the current matchfile_entry
	qfits_table* table;
	int nrows;

	// when reading:
	char* fn;
	int columns[MATCHFILE_FITS_COLUMNS];

	// for buffered reading:
	bread br;
};
typedef struct matchfile matchfile;

void matchobj_compute_overlap(MatchObj* mo);


pl* matchfile_get_matches_for_field(matchfile* m, uint field);

matchfile* matchfile_open_for_writing(char* fn);

int matchfile_write_header(matchfile* m);

int matchfile_fix_header(matchfile* m);

int matchfile_write_match(matchfile* m, MatchObj* mo);


matchfile* matchfile_open(char* fn);

int matchfile_read_matches(matchfile* m, MatchObj* mo, uint offset, uint n);

MatchObj* matchfile_buffered_read_match(matchfile* m);

int matchfile_buffered_read_pushback(matchfile* m);

int matchfile_close(matchfile* m);

#endif
