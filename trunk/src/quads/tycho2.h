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
	//uint tyc1;
	//uint tyc2;
	//uint tyc3;
	ushort tyc1;
	ushort tyc2;
	uchar  tyc3;

	// flag "P"
	bool photo_center;
	// flag "X"
	bool no_motion;
	// flag "T"
	bool tycho1_star;
	// flag "D"
	bool double_star;
	// flag "P" ( DP)
	bool photo_center_2;
	// flag "H" (in supplements)
	bool hipparcos_star;

	// degrees
	double RA;         // RAdeg
	double DEC;        // DEdeg
	double meanRA;     // mRAdeg
	double meanDEC;    // mDEdeg
	// mas/yr
	float pmRA;        // pmRA
	float pmDEC;       // pmDE
	// mas
	float sigma_RA;    // e_RA
	float sigma_DEC;   // e_DE
	float sigma_mRA;   // e_mRA
	float sigma_mDEC;  // e_mDE
	// mas/yr
	float sigma_pmRA;  // e_pmRA
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
	// mag
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

	uint hipparcos_id;
	char hip_ccdm[4];
};
typedef struct tycho2_entry tycho2_entry;

int tycho2_guess_is_supplement(char* line);

int tycho2_parse_entry(char* line, tycho2_entry* entry);

int tycho2_supplement_parse_entry(char* line, tycho2_entry* entry);

#endif

