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

#ifndef SOLVER_H
#define SOLVER_H

#include "starutil.h"
#include "xylist.h"
#include "kdtree.h"
#include "bl.h"
#include "matchobj.h"

struct solver_params;

typedef int (*handle_hit)(struct solver_params*, MatchObj*);

enum {
	PARITY_NORMAL,
	PARITY_FLIP,
	PARITY_BOTH
};

struct solver_params {

	// Inputs:
	double* field;
	int nfield;
	kdtree_t* codekd;
	int startobj;
	int endobj;
	// one of PARITY_*
	int parity;
	// number of field quads to try
	int maxtries;
	// number of quad matches to try
	int maxmatches;
	double codetol;
	bool quitNow;
	int fieldid;
	int fieldnum;
	char* solvedfn;
	bool do_solvedserver;
	double cxdx_margin;
	bool circle;
	bool quiet;

	// The limits on the size, in field coordinates,
	// of the quads to find.
	double minAB;
	double maxAB;
	// Limits on the quad size, in field scale units.
	double arcsec_per_pixel_lower;
	double arcsec_per_pixel_upper;

	// the MatchObj template: if non-NULL, every MatchObj will be a memcpy
	// of this one before the other fields are set.
	MatchObj* mo_template;

	handle_hit handlehit;
	
	// internal:
	double cornerpix[8];
	double fieldscale;
	double starscale_lower;
	double starscale_upper;
	double starttime;
	double timeused;

	// Outputs:
	// NOTE: these are only incremented, not initialized.  It's up to
	// you to set them to zero before calling, if you're starting from scratch.
	int numtries;
	int nummatches;
	int numscaleok;
	//int mostagree;
	// this is set to an absolute value.
	int objsused;
	// number of quads skipped because of cxdx constraints.
	int numcxdxskipped;

	void* userdata;
};
typedef struct solver_params solver_params;

void solver_default_params(solver_params* params);

void solve_field(solver_params* params);

#endif
