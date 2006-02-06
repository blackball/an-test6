#ifndef SOLVER2_H
#define SOLVER2_H

#define NO_KD_INCLUDES 1
#include "starutil.h"

#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"

#include "blocklist.h"

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


/**
   You have to define these two functions!
*/
extern void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD);
extern void getstarcoords(star *sA, star *sB, star *sC, star *sD,
						  sidx iA, sidx iB, sidx iC, sidx iD);


int solver_add_hit(blocklist* hitlist, MatchObj* mo, double AgreeTol);

void solve_field(xy *thisfield,
				 int startfieldobj, int endfieldobj,
				 int maxtries, int max_matches_needed,
				 kdtree_t *codekd, double codetol, blocklist* hitlist,
				 bool* pQuitNow, double AgreeTol,
				 int* pnumtries, int* pnummatches, int* pmostAgree,
				 xy* cornerpix, int* pobjsused);


#endif
