#ifndef SOLVER2_H
#define SOLVER2_H

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#include "blocklist.h"

struct solver_params {

	// Inputs:
	xy* field;
	kdtree_t* codekd;
	int startobj;
	int endobj;
	int maxtries;
	int max_matches_needed;
	double agreetol;
	double codetol;
	bool quitNow;

	// Must be initialized by caller; will contain outputs:
	xy* cornerpix;
	blocklist* hitlist;

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


typedef struct match_struct {
    qidx quadno;
    sidx iA, iB, iC, iD;
    sidx fA, fB, fC, fD;
    double code_err;
    star *sMin, *sMax;
    double vector[6];
	/*
	  double* transform;
	  double corners[4];
	  double starA[3];
	  double starB[3];
	  double starC[3];
	  double starD[3];
	  double fieldA[2];
	  double fieldB[2];
	  double fieldC[2];
	  double fieldD[2];
	  int abcdorder;
	*/
} MatchObj;
#define MATCH_VECTOR_SIZE 6

#define mk_MatchObj() ((MatchObj *)malloc(sizeof(MatchObj)))
#define free_MatchObj(m) free(m)

int solver_add_hit(blocklist* hitlist, MatchObj* mo, double AgreeTol);

void solve_field(solver_params* params);

#endif
