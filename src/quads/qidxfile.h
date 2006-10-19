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

#ifndef QIDXFILE_H
#define QIDXFILE_H

#include <sys/types.h>

#include "qfits.h"

struct qidxfile {
	uint numstars;
	uint numquads;

	// when reading:
	void*  mmap_base;
	size_t mmap_size;
	uint* index;
	uint* heap;

	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;

	uint cursor_index;
	uint cursor_heap;
};
typedef struct qidxfile qidxfile;

int qidxfile_close(qidxfile* qf);

int qidxfile_get_quads(qidxfile* qf, uint starid, uint** quads, uint* nquads);

int qidxfile_write_star(qidxfile* qf, uint* quads, uint nquads);

int qidxfile_write_header(qidxfile* qf);

qidxfile* qidxfile_open(char* fname, int modifiable);

qidxfile* qidxfile_open_for_writing(char* qidxfname,
									uint nstars, uint nquads);

#endif
