/*
  This file is part of the Astrometry.net suite.
  Copyright 2007 Dustin Lang.

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

#ifndef FITSBIN_H
#define FITSBIN_H

#include <stdio.h>

#include "qfits.h"

struct fitsbin_t {
	char* filename;
	char* tablename;

	// The primary FITS header
	qfits_header* primheader;

	// Extra FITS headers to add to the extension header containing the table.
	qfits_header* header;

	// The data
	void* data;

	// The size of a single data item
	int itemsize;
	// The number of items (rows)
	int nrows;

	// Internal use:

	// The mmap'ed address
	char* map;
	// The mmap'ed size.
	size_t mapsize;

	// Writing:
	FILE* fid;
	off_t primheader_end;
	off_t header_start;
	off_t header_end;
};
typedef struct fitsbin_t fitsbin_t;


fitsbin_t* fitsbin_open(const char* fn, const char* tablename,
						char** errstr, 
						int (*callback_read_header)(qfits_header* primheader, qfits_header* header, size_t* expected, char** errstr, void* userdata),
						void* userdata);

fitsbin_t* fitsbin_open_for_writing(const char* fn, const char* tablename,
									char** errstr);

int fitsbin_close(fitsbin_t* fb);

int fitsbin_write_primary_header(fitsbin_t* fb);

int fitsbin_write_header(fitsbin_t* fb);

int fitsbin_fix_primary_header(fitsbin_t* fb);

int fitsbin_fix_header(fitsbin_t* fb);



#endif
