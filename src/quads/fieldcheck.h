/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef FIELD_CHECK_H
#define FIELD_CHECK_H

#include "qfits.h"

#define FIELDCHECK_AN_FILETYPE "FC"

#define FIELDCHECK_FITS_COLUMNS 5

struct fieldcheck_file {
	// when writing:
	FILE* fid;
	off_t header_end;

	// when reading:
	char* fn;
	int columns[FIELDCHECK_FITS_COLUMNS];

	qfits_table* table;
	qfits_header* header;
	int nrows;
};
typedef struct fieldcheck_file fieldcheck_file;

struct fieldcheck {
	int filenum;
	int fieldnum;
	// in degrees
	double ra;
	double dec;
	// in arcsec
	float radius;
};
typedef struct fieldcheck fieldcheck;

fieldcheck_file* fieldcheck_file_open_for_writing(char* fn);

int fieldcheck_file_write_header(fieldcheck_file* f);

int fieldcheck_file_fix_header(fieldcheck_file* f);

int fieldcheck_file_write_entry(fieldcheck_file* f, fieldcheck* fc);

fieldcheck_file* fieldcheck_file_open(char* fn);

int fieldcheck_file_read_entries(fieldcheck_file* f, fieldcheck* fc,
								 uint offset, uint n);

int fieldcheck_file_close(fieldcheck_file* f);

#endif
