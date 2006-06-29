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
	// this isn't stored, it's computed from noverlap and ninfield.
	float overlap;

	// formerly matchfile_entry:
	int fieldnum;
	int fieldfile;
	int16_t indexid;
	int16_t healpix;

	bool parity;

	// how many field quads did we try before finding this one?
	int quads_tried;
	// how many matching quads from the index did we find before this one?
	int quads_matched;
	// how many matching quads had the right scale?
	int quads_scaleok;
	// how many field objects did we have to look at?
	//  (this isn't stored in the matchfile, it's max(field))
	int objs_tried;
	// how many matches have we run verification on?
	int nverified;
	// how many seconds of CPU time have we spent on this field?
	float timeused;

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

// compute all derived fields.
void matchobj_compute_derived(MatchObj* mo);

#define MATCH_VECTOR_SIZE 6

#define mk_MatchObj() ((MatchObj *)calloc(1, sizeof(MatchObj)))
#define free_MatchObj(m) free(m)

#endif
