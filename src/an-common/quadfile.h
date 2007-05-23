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

#ifndef QUADFILE_H
#define QUADFILE_H

#include <sys/types.h>

#include "qfits.h"

struct quadfile {
	uint numquads;
	uint numstars;
	// upper bound
	double index_scale;
	// lower bound
	double index_scale_lower;
	// unique ID of this index
	int indexid;
	// healpix covered by this index
	int healpix;

	qfits_header* header;

	// when reading:
	void*  mmap_quad;
	size_t mmap_quad_size;
	uint*   quadarray;

	// when writing:
	FILE* fid;
	off_t header_end;
};
typedef struct quadfile quadfile;

int quadfile_close(quadfile* qf);

void quadfile_get_starids(quadfile* qf, uint quadid,
						  uint* starA, uint* starB,
						  uint* starC, uint* starD);

int quadfile_write_quad(quadfile* qf, uint iA, uint iB, uint iC, uint iD);

int quadfile_fix_header(quadfile* qf);

int quadfile_write_header(quadfile* qf);

double quadfile_get_index_scale_arcsec(quadfile* qf);

double quadfile_get_index_scale_lower_arcsec(quadfile* qf);

quadfile* quadfile_open(char* fname, int modifiable);

quadfile* quadfile_open_for_writing(char* quadfname);

#endif
