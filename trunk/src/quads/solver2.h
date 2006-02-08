#ifndef SOLVER2_H
#define SOLVER2_H

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#include "blocklist.h"
#include "hitlist.h"

struct solver_params {

	// Inputs:
	xy* field;
	kdtree_t* codekd;
	int startobj;
	int endobj;
	int maxtries;
	int max_matches_needed;
	//double agreetol;
	double codetol;
	bool quitNow;

	// Must be initialized by caller; will contain outputs:
	xy* cornerpix;
	struct hitlist* hits;

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

void solve_field(solver_params* params);

#endif
