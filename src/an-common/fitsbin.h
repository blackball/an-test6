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

struct fitsbin_chunk_t {
	char* tablename;

	// Extra FITS headers to add to the extension header containing the table.
	qfits_header* header;

	// The data (NULL if the table was not found)
	void* data;

	// The size of a single row in bytes.
	int itemsize;

	// The number of items (rows)
	int nrows;

	// abort if this table isn't found?
	int required;

    // Reading:
    int (*callback_read_header)(qfits_header* primheader, qfits_header* header, size_t* expected, char** errstr, void* userdata);
    void* userdata;

    // Writing:
	off_t header_start;
	off_t header_end;

	// Internal use:
	// The mmap'ed address
	char* map;
	// The mmap'ed size.
	size_t mapsize;
};
typedef struct fitsbin_chunk_t fitsbin_chunk_t;


struct fitsbin_t {
	char* filename;

    fitsbin_chunk_t* chunks;
    int nchunks;

	// The primary FITS header
	qfits_header* primheader;

    // Error string in which to report errors.
    char** errstr;

	// Writing:
	FILE* fid;
	off_t primheader_end;
};
typedef struct fitsbin_t fitsbin_t;


fitsbin_t* fitsbin_new(int nchunks);

int fitsbin_read(fitsbin_t* fb);

int fitsbin_close(fitsbin_t* fb);

int fitsbin_write_primary_header(fitsbin_t* fb);

int fitsbin_fix_primary_header(fitsbin_t* fb);

int fitsbin_write_chunk_header(fitsbin_t* fb, int chunk);

int fitsbin_fix_chunk_header(fitsbin_t* fb, int chunk);

int fitsbin_start_write(fitsbin_t* fb);


// Single-chunk shortcuts:

fitsbin_t* fitsbin_open(const char* fn, const char* tablename,
						char** errstr, 
						int (*callback_read_header)(qfits_header* primheader, qfits_header* header, size_t* expected, char** errstr, void* userdata),
						void* userdata);
int fitsbin_write_header(fitsbin_t* fb);
int fitsbin_fix_header(fitsbin_t* fb);

fitsbin_t* fitsbin_open_for_writing(const char* fn, const char* tablename,
									char** errstr);





#endif
