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

#ifndef idfile_H
#define idfile_H

#include <sys/types.h>
#include <stdint.h>
#include <assert.h>

#include "qfits.h"

struct idfile {
	uint numstars;
	int healpix;

	// when reading:
	void*  mmap_base;
	size_t mmap_size;
	uint64_t* anidarray;

	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;
};
typedef struct idfile idfile;

idfile* idfile_open(char* fname, int modifiable);

idfile* idfile_open_for_writing(char* quadfname);

int idfile_close(idfile* qf);

uint64_t idfile_get_anid(idfile* qf, uint starid);

int idfile_write_anid(idfile* qf, uint64_t anid /* astrometry.net id */ );

int idfile_fix_header(idfile* qf);

int idfile_write_header(idfile* qf);


#endif
