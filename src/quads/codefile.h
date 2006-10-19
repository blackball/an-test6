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

#ifndef CODEFILE_H_
#define CODEFILE_H_

#include <sys/types.h>
#include <stdio.h>

#include "starutil.h"
#include "qfits.h"

struct codefile {
	uint numcodes;
	uint numstars;
	// upper bound
	double index_scale;
	// lower bound
	double index_scale_lower;
	// unique ID of this index
	int indexid;
	// healpix covered by this index
	int healpix;

	// when reading:
	void*  mmap_code;
	size_t mmap_code_size;
	double* codearray;

	// when writing:
	FILE* fid;
	off_t header_end;

	qfits_header* header;
};
typedef struct codefile codefile;

int codefile_close(codefile* qf);

void codefile_get_code(codefile* qf, uint codeid,
                       double* cx, double* cy, double* dx, double* dy);

codefile* codefile_open(char* codefname, int modifiable);

codefile* codefile_open_for_writing(char* codefname);

int codefile_write_header(codefile* qf);

int codefile_write_code(codefile* qf, double cx, double cy, double dx, double dy);

int codefile_fix_header(codefile* qf);

#endif
