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

#ifndef NOMAD_H
#define NOMAD_H

#include <stdint.h>

#include "starutil.h"

#define NOMAD_RECORD_SIZE 88

struct nomad_entry {
	// degrees
	double ra;
	double dec;

	// arcsec
	float sigma_racosdec;
	float sigma_dec;

	// arcsec/yr
	float mu_racosdec;
	float mu_dec;

	// arcsec/yr
	float sigma_mu_racosdec;
	float sigma_mu_dec;

	// yr
	float epoch_ra;
	float epoch_dec;

	// mag
	// (30.000 in any magnitude field indicates "no data".)
	// (30.001 = "no data" but ...)
	float mag_B;
	float mag_V;
	float mag_R;
	float mag_J;
	float mag_H;
	float mag_K;

	uint usnob_id;
	uint twomass_id;
	uint yb6_id;
	uint ucac2_id;
	uint tycho2_id;

	// all these take values from the "nomad_src" enum.
	unsigned char astrometry_src;
	unsigned char blue_src;
	unsigned char visual_src;
	unsigned char red_src;

	bool usnob_fail;       // UBBIT   "Fails Blaise's test for USNO-B1.0 star"
	bool twomass_fail;     // TMBIT   "Fails Roc's test for clean 2MASS star"
	bool tycho_astrometry; // TYBIT   "Astrometry comes from Tycho2"
	bool alt_radec;        // XRBIT   "Alt correlations for same (RA,Dec)"
	bool alt_2mass;        // ITMBIT  "Alt correlations for same 2MASS ID"
	bool alt_ucac;         // IUCBIT  "Alt correlations for same UCAC-2 ID"
	bool alt_tycho;        // ITYBIT  "Alt correlations for same Tycho2 ID"
	bool blue_o;           // OMAGBIT "Blue magnitude from O (not J) plate"
	bool red_e;            // EMAGBIT "Red magnitude from E (not F) plate"
	bool twomass_only;     // TMONLY  "Object found only in 2MASS cat"
	bool hipp_astrometry;  // HIPAST  "Ast from Hipparcos (not Tycho2) cat"
	bool diffraction;      // SPIKE   "USNO-B1.0 diffraction spike bit set"
	bool confusion;        // TYCONF  "Tycho2 confusion flag"
	bool bright_confusion; // BSCONF  "Bright star has nearby faint source"
	bool bright_artifact;  // BSART   "Faint source is bright star artifact"
	bool standard;         // USEME   "Recommended astrometric standard"
	bool external;         // EXCAT   "External, non-astrometric object"

	// sequence number assigned by us (it's not in the original catalogue),
	// composed of the 1/10 degree DEC zone (top 11 bits) and the sequence
	// number within the zone (bottom 21 bits).
	uint nomad_id;
};
typedef struct nomad_entry nomad_entry;

enum nomad_src {
	NOMAD_SRC_NONE = 0,
	NOMAD_SRC_USNOB,
	NOMAD_SRC_2MASS,
	NOMAD_SRC_YB6,
	NOMAD_SRC_UCAC2,
	NOMAD_SRC_TYCHO2,
	NOMAD_SRC_HIPPARCOS
};

int nomad_parse_entry(struct nomad_entry* entry, void* encoded);

#endif

