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

#ifndef HITSFILE_H
#define HITSFILE_H

#include <stdio.h>

#include "starutil.h"
#include "solver.h"
#include "matchfile.h"

struct hitsfileheader {
	float agreetol;
	unsigned int min_matches_to_agree;
	float overlap_needed;
	unsigned int field_objs_needed;
};
typedef struct hitsfileheader hits_header;

struct hitsfieldentry {
	bool failed;
	unsigned int field;
};
typedef struct hitsfieldentry hits_field;

void hits_header_init(hits_header* h);

void hits_write_header(FILE* fid, hits_header* h);

void hits_write_tailer(FILE* fid);


void hits_field_init(hits_field* h);

void hits_write_field_header(FILE* fid, hits_field* h);

void hits_start_hits_list(FILE* fid);

void hits_write_hit(FILE* fid, MatchObj* mo);

void hits_end_hits_list(FILE* fid);

void hits_write_field_tailer(FILE* fid);


void hits_write_correspondences(FILE* fid, uint* starids, uint* fieldids,
								int Nids, int ok);

#endif
