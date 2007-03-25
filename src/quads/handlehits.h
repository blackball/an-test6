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

#ifndef HANDLEHITS_H
#define HANDLEHITS_H

#include "hitlist.h"
#include "matchobj.h"
#include "bl.h"
#include "kdtree.h"

struct handlehits {
	// params
	double agreetol; // in arcsec.
	int nagree_toverify;
	// we assume the "tokeep" parameters are weaker than (or equal to)
	// the "tosolve" ones, ie, a match must pass "tokeep" before it is
	// even checked against "tosolve".
	double overlap_tokeep;
	int ninfield_tokeep;
	double overlap_tosolve;
	int ninfield_tosolve;

	// callback to run verification.
	void (*run_verify)(struct handlehits* me, MatchObj* mo);

	// state:

	// used when nagree_toverify > 1
	hitlist* hits;

	// number of times we've run verification
	int nverified;

	// best hit that surpasses the "solve" requirements.
	MatchObj* bestmo;
	// MatchObj*s with overlap above the keep threshold
	pl* keepers;

	void* userdata;
};
typedef struct handlehits handlehits;

void handlehits_init(handlehits* hh);

void handlehits_uninit(handlehits* hh);

handlehits* handlehits_new();

bool handlehits_add(handlehits* hh, MatchObj* mo);

// goes through the hitlist and frees MatchObjs.
void handlehits_free_matchobjs(handlehits* hh);

// reset state: empty the lists, etc, in preparation for starting a new field.
void handlehits_clear(handlehits* hh);

// pack it in!
void handlehits_free(handlehits* hh);

#endif

