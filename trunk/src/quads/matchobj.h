#ifndef MATCHOBJ_H_
#define MATCHOBJ_H_

#include "starutil.h"

struct match_struct {
    qidx quadno;
    sidx iA, iB, iC, iD;
    sidx fA, fB, fC, fD;
    double code_err;
    //star *sMin, *sMax;

	double sMin[3];
	double sMax[3];

    double vector[6];

	double* transform;
	/*
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

	void* extra;
};
typedef struct match_struct MatchObj;

#define MATCH_VECTOR_SIZE 6

#define mk_MatchObj() ((MatchObj *)malloc(sizeof(MatchObj)))
#define free_MatchObj(m) free(m)

#endif
