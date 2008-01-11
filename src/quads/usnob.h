/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef USNOB_H
#define USNOB_H

#include "an-bool.h"
#include <stdint.h>

#include "starutil.h"

#define USNOB_RECORD_SIZE 80

#define USNOB_SURVEY_POSS_I_O 0
#define USNOB_SURVEY_POSS_I_E 1
#define USNOB_SURVEY_POSS_II_J 2
#define USNOB_SURVEY_POSS_II_F 3
#define USNOB_SURVEY_SERC_J 4
#define USNOB_SURVEY_SERC_EJ 4
#define USNOB_SURVEY_ESO_R 5
#define USNOB_SURVEY_SERC_ER 5
#define USNOB_SURVEY_AAO_R 6
#define USNOB_SURVEY_POSS_II_N 7
#define USNOB_SURVEY_SERC_I 8
#define USNOB_SURVEY_SERC_I_OR_POSS_II_N 9

struct observation {
	// 0 to 99.99 (m:4)
	float mag;

	// field number in the original survey. 1-937 (F:3)
	unsigned short field;

	// the original survey. (S:1)
	// (eg USNOB_SURVEY_POSS_I_O)
	unsigned char survey;

	// star/galaxy estimate.  0=galaxy, 11=star. 19=no value computed.
	//     (GG:2)
    // (but note, in fact values 12, 13, 14, 15 and possibly others exist
    //  in the data files as well!)
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
	// M=0 means Tycho-2 star.  In this case, NONE of the other fields
	//                          in the struct can be trusted!  The USNOB
	//                          compilers used a different (and undocumented)
	//                          format to store Tycho-2 stars.
	// M=1 means it's a reject USNOB star.
	// M>=2 means it's a valid USNOB star.
	unsigned char ndetections;

	bool diffraction_spike;
	bool motion_catalog;
	// YS4.0 correlation
	bool ys4;

	// astrometry.net diffraction detection
	bool an_diffraction_spike;

	// this is our identifier; it's not in the USNO-B files.
	// it allows us to point back to the USNO-B source.
	// top byte: [0,180): south-polar distance slice.
	// bottom 24 bits: [0, 12,271,141): index within slice.
	uint usnob_id;

	// the observations for this object.  These are stored in a fixed
	// order (same as the raw USNOB data):
	//   obs[OBS_BLUE1] is the "first-epoch (old) blue" observation,
	//   obs[OBS_RED2]  is the "second-epoch (new) red" observation
	//
	// Note that many objects have fewer than five observations.  To check
	// whether an observation exists, check the "field" value: all valid
	// observations have non-zero values.
	struct observation obs[5];
};
typedef struct usnob_entry usnob_entry;

int usnob_get_slice(usnob_entry* entry);

int usnob_get_index(usnob_entry* entry);

int usnob_parse_entry(unsigned char* line, usnob_entry* usnob);

unsigned char usnob_get_survey_band(int survey);

// returns 1 if the observation is first-epoch
//         2 if the observation is second-epoch
//        -1 on error.
int unsob_get_survey_epoch(int survey, int obsnum);

/*
  Returns TRUE if this entry is a true USNOB star, not a Tycho-2 or reject.
  (This doesn't check diffraction flags, just the "M" / "ndetection" field).
 */
bool usnob_is_usnob_star(usnob_entry* entry);

/*
  Returns TRUE if the given observation contains real data.
  (Note that "usnob_is_usnob_star" must pass for this to be valid)
*/
bool usnob_is_observation_valid(struct observation* obs);

/*
  Returns TRUE if the given bandpass (emulsion) is "blue" (band is 'O' or 'J').
*/
bool usnob_is_band_blue(unsigned char band);

/* Returns TRUE if the given observation comes from a blue emulsion. */
bool usnob_is_observation_blue(struct observation* obs);

/*
  Returns TRUE if the given bandpass (emulsion) is "red" (band is 'E' or 'F').
*/
bool usnob_is_band_red(unsigned char band);

/* Returns TRUE if the given observation comes from a red emulsion. */
bool usnob_is_observation_red(struct observation* obs);

/*
  Returns TRUE if the given bandpass (emulsion) is "infrared" (band is 'N')
*/
bool usnob_is_band_ir(unsigned char band);

/* Returns TRUE if the given observation comes from an infrared emulsion. */
bool usnob_is_observation_ir(struct observation* obs);

#endif
