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

#ifndef FITSFILE_H
#define FITSFILE_H

#include <stdio.h>

#include "qfits.h"

struct fitsextension_t {
	// Extra FITS headers to add to the extension header containing the table.
	qfits_header* header;

    // Writing:
	off_t header_start;
	off_t header_end;
};
typedef struct fitsextension_t fitsextension_t;

// frees the qfits_header*.
void fitsfile_extension_close(fitsextension_t* ext);

void fitsfile_extension_set_header(fitsextension_t* ext,
                                   qfits_header* hdr);

qfits_header* fitsfile_extension_get_header(fitsextension_t* ext);

int fitsfile_extension_write_header(fitsextension_t* ext, FILE* fid);

int fitsfile_extension_fix_header(fitsextension_t* ext, FILE* fid);


struct fitsfile_t {
	char* filename;

	// Writing:
	FILE* fid;

    fitsextension_t primary;

    /*
     // The primary FITS header
     qfits_header* primheader;
     off_t primheader_end;
     */
};
typedef struct fitsfile_t fitsfile_t;


char* fitsfile_filename(fitsfile_t* ff);

fitsfile_t* fitsfile_open_for_writing(const char* fn);

fitsfile_t* fitsfile_open(const char* fn);

int fitsfile_close(fitsfile_t* fb);

qfits_header* fitsfile_get_primary_header(fitsfile_t* ff);

//fitsfile_t* fitsfile_new(int nchunks);

int fitsfile_write_primary_header(fitsfile_t* fb);

int fitsfile_fix_primary_header(fitsfile_t* fb);



int fitsfile_read(fitsfile_t* fb);

/*
 int fitsfile_write_extension_header(fitsfile_t* fb, int extension);
 int fitsfile_fix_extension_header(fitsfile_t* fb, int extension);
 */

int fitsfile_start_write(fitsfile_t* fb);

void fitsfile_set_filename(fitsfile_t* fb, const char* fn);

int fitsfile_write_header(fitsfile_t* fb);
int fitsfile_fix_header(fitsfile_t* fb);




#endif

