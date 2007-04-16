/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef TYCHO2_H
#define TYCHO2_H

#include <stdint.h>

#include "starutil.h"

// 206 bytes of data, but each record is supposed to be terminated
// by \r\n, making...
#define TYCHO_RECORD_SIZE_RAW 206

// ... 208 bytes total!
#define TYCHO_RECORD_SIZE 208

#define TYCHO_SUPPLEMENT_RECORD_SIZE_RAW 122
#define TYCHO_SUPPLEMENT_RECORD_SIZE 124

struct tycho2_entry {
	// together these form the star ID.
	int16_t tyc1;  // [1, 9537] in main catalog; [2, 9529] in suppl.
	int16_t tyc2;  // [1, 12121] main; [1,12112] suppl.
	uchar  tyc3;  // [1, 3] main, [1, 4] suppl.

	// flag "P": photo-center of two stars was used for position
	bool photo_center;
	// flag "X": no mean position, no proper motion
	bool no_motion;
	// flag "T"
	bool tycho1_star;
	// flag "D"
	bool double_star;
	// flag "P" ( DP)
	bool photo_center_treatment;
	// flag "H" (in supplements)
	bool hipparcos_star;

	// degrees
	double RA;         // RAdeg
	double DEC;        // DEdeg
	double meanRA;     // mRAdeg
	double meanDEC;    // mDEdeg
	// mas/yr
	float pmRA;        // pmRA*
	float pmDEC;       // pmDE
	// mas
	float sigma_RA;    // e_RA*
	float sigma_DEC;   // e_DE
	float sigma_mRA;   // e_mRA*
	float sigma_mDEC;  // e_mDE
	// mas/yr
	float sigma_pmRA;  // e_pmRA*
	float sigma_pmDEC; // e_pmDE
	// yr
	float epoch_RA;    // epRA
	float epoch_DEC;   // epDE
	float epoch_mRA;   // mepRA
	float epoch_mDEC;  // mepDE
	//
	unsigned char nobs; // Num
	// "goodness"
	float goodness_mRA;  // g_mRA
	float goodness_mDEC; // g_mDEC
	float goodness_pmRA;  // g_pmRA
	float goodness_pmDEC; // g_pmDEC
	// mag (0.0 means unavailable)
	float mag_BT;        // BT
	float mag_VT;        // VT
	float sigma_BT;      // e_BT
	float sigma_VT;      // e_VT

	// supplements only: Hp magnitude
	float mag_HP;        // HP
	float sigma_HP;      // e_HP

	// arcsec
	float prox;

	float correlation;

	int32_t hipparcos_id;    // [1, 120404] (or zero)
	char hip_ccdm[4];     // (up to three chars; null-terminated.)
};
typedef struct tycho2_entry tycho2_entry;

int tycho2_guess_is_supplement(char* line);

int tycho2_parse_entry(char* line, tycho2_entry* entry);

int tycho2_supplement_parse_entry(char* line, tycho2_entry* entry);

#endif

