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

#ifndef SOLVER_H
#define SOLVER_H

#include "starutil.h"
#include "xylist.h"
#include "kdtree.h"
#include "bl.h"
#include "matchobj.h"
#include "idfile.h"
#include "quadfile.h"
#include "starkd.h"
#include "codekd.h"

struct solver_params;

typedef int  (*handle_hit)(struct solver_params*, MatchObj*);

enum {
	PARITY_NORMAL,
	PARITY_FLIP,
	PARITY_BOTH
};

// Per-index parameters.
struct index_params {
	// name of the current index.
	char *indexname;

	// unique id for this index.
	int indexid;
	int healpix;

	// The index
	codetree* codekd;
	idfile* idfile;
	quadfile* quads;
	startree* starkd;

	// Jitter in the index, in arcseconds.
	double index_jitter;

	// Does the index have the CIRCLE header - (codes live in the circle, not the box)?
	bool circle;
	double cxdx_margin;

	// Limits of the size of quads in the index.
	double index_scale_upper;
	double index_scale_lower;
};
typedef struct index_params index_params;

struct solver_params {

	// the index we're currently dealing with.
	index_params* sips;

	// the set of indices.
	bl* indexes;

	// the extreme limits of quad size, for all indexes.
	double minminAB;
	double maxmaxAB;

	// for printing "index %i of %i"
	int indexnum;
	int nindexes;

	// Inputs:

	char* cancelfname;
	bool cancelled;

	char* solved_in;
	bool do_solvedserver;

	bool quitNow;

	// one of PARITY_*
	int parity;
	double codetol;
	int startobj;
	int endobj;

	// Limits on the quad size, in field scale units.
	double funits_lower;
	double funits_upper;

	// The field
	double* field;
	int nfield;

	// Field limits.
	double field_minx, field_maxx, field_miny, field_maxy;
	// distance in pixels across the diagonal of the field
	double field_diag;

	// number of field quads to try
	int maxquads;
	// number of quad matches to try
	int maxmatches;
	int fieldid;
	int fieldnum;

	// number of fields we're going to try.
	int nfields;

	bool quiet;

	// the MatchObj template: if non-NULL, every MatchObj will be a memcpy
	// of this one before the other fields are set.
	MatchObj* mo_template;

	handle_hit handlehit;

	// internal:
	double starttime;
	double timeused;

	// Outputs:
	// NOTE: these are only incremented, not initialized.  It's up to
	// you to set them to zero before calling, if you're starting from scratch.
	int numtries;
	int nummatches;
	int numscaleok;
	// this is set to an absolute value.
	int objsused;
	// number of quads skipped because of cxdx constraints.
	int numcxdxskipped;

	void* userdata;
};
typedef struct solver_params solver_params;

void solver_default_params(solver_params* params);

void default_index_params(index_params* sips);

void solver_compute_quad_range(solver_params* sp, index_params* sips, double*, double*);

void solve_field(solver_params* params);

#endif
