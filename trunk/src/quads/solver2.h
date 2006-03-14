#ifndef SOLVER2_H
#define SOLVER2_H

#include "starutil.h"
#include "xylist.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#include "bl.h"
#include "matchobj.h"

struct solver_params;

typedef int (*handle_hit)(struct solver_params*, MatchObj*);

struct solver_params {

	// Inputs:
	xy* field;
	kdtree_t* codekd;
	int startobj;
	int endobj;
	int maxtries;
	int max_matches_needed;
	double codetol;
	bool quitNow;

	// The limits on the size, in field coordinates,
	// of the quads to find.
	double minAB;
	double maxAB;
	double arcsec_per_pixel_lower;
	double arcsec_per_pixel_upper;

	// Must be initialized by caller; will contain outputs:
	xy* cornerpix;
	handle_hit handlehit;

	// Outputs:
	// NOTE: these are only incremented, not initialized.  It's up to
	// you to set them to zero before calling, if you're starting from scratch.
	int numtries;
	int nummatches;
	int mostagree;
	// this is set to an absolute value.
	int objsused;
};
typedef struct solver_params solver_params;

void solver_default_params(solver_params* params);

void solve_field(solver_params* params);

#endif
