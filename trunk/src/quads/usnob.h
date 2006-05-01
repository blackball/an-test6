#ifndef USNOB_H
#define USNOB_H

#include <stdint.h>

#include "starutil.h"

#define USNOB_RECORD_SIZE 80

struct observation {
	// 0 to 99.99 (m:4)
	float mag;

	// field number in the original survey. 1-937 (F:3)
	unsigned short field;

	// the original survey. (S:1)
	unsigned char survey;

	// star/galaxy estimate.  0=galaxy, 11=star. 19=no value computed.
	//     (GG:2)
	unsigned char star_galaxy;

	// in arcsec (R:4)
	float xi_resid;

	// in arcsec (r:4)
	float eta_resid;

	// source of photometric calibration: (C:1)
	//  0=bright photometric standard on this plate
	//  1=faint pm standard on this plate
	//  2=faint " " one plate away
	//  etc
	unsigned char calibration;

	// back-pointer to PMM file. (i:7)
	uint pmmscan;
};

#define OBS_BLUE1 0
#define OBS_RED1  1
#define OBS_BLUE2 2
#define OBS_RED2  3
#define OBS_N     4

struct usnob_entry {
	// (numbers in brackets are number of digits used in USNOB format)
	// in degrees (a:9)
	double ra;

	// in degrees (s:8)
	double dec;

	// in arcsec / yr. (A:3)
	float mu_ra;

	// in arcsec / yr. (S:3)
	float mu_dec;

	// motion probability. (P:1)
	float mu_prob;

	// in arcsec / yr. (x:3)
	float sigma_mu_ra;

	// in arcsec / yr. (y:3)
	float sigma_mu_dec;

	// in arcsec. (Q:1)
	float sigma_ra_fit;

	// in arcsec. (R:1)
	float sigma_dec_fit;

	// in arcsec. (u:3)
	float sigma_ra;

	// in arcsec. (v:3)
	float sigma_dec;

	// in years; 1950-2050. (e:3)
	float epoch;

	// number of detections; (M:1)
	// M=0 means Tycho-2 star.
	// M=1 means it's a reject USNOB star.
	unsigned char ndetections;

	bool diffraction_spike;
	bool motion_catalog;
	// YS4.0 correlation
	bool ys4;

	// this is our identifier; it's not in the USNO-B files.
	// it allows us to point back to the USNO-B source.
	// top byte: [0,180): south-polar distance slice.
	// bottom 24 bits: [0, 12,271,141): index within slice.
	uint usnob_id;

	struct observation obs[5];
};
typedef struct usnob_entry usnob_entry;

int usnob_get_slice(usnob_entry* entry);

int usnob_get_index(usnob_entry* entry);

int usnob_parse_entry(unsigned char* line, usnob_entry* usnob);

#endif
