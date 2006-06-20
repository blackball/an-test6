#ifndef MATCHOBJ_H
#define MATCHOBJ_H

#include <stdint.h>
#include "starutil.h"

struct match_struct {
    uint quadno;
	uint star[4];
	uint field[4];
    float code_err;
	double sMin[3];
	double sMax[3];
	double sMinMax[3];
	double sMaxMin[3];
    double vector[6];
	bool transform_valid;
	double transform[9];
	int16_t noverlap;
	int16_t ninfield;
	float overlap;

	// formerly matchfile_entry:
	int fieldnum;
	bool parity;
	int fieldfile;
	int16_t indexid;
	int16_t healpix;

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

void matchobj_compute_overlap(MatchObj* mo);

#define MATCH_VECTOR_SIZE 6

#define mk_MatchObj() ((MatchObj *)calloc(1, sizeof(MatchObj)))
#define free_MatchObj(m) free(m)

#endif
