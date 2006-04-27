#include <stdint.h>

#include "starutil.h"

/*
  typedef unsigned char bool;
  typedef unsigned int uint;
*/

#define USNOB_RECORD_SIZE 80

struct observation {
	// 0 to 99.99 (m:4)
	float mag;
	//double mag;

	// field number in the original survey. 1-937 (F:3)
	unsigned short field;

	// the original survey. (S:1)
	unsigned char survey;

	// star/galaxy estimate.  0=galaxy, 11=star. 19=no value computed.
	//     (GG:2)
	unsigned char star_galaxy;

	// in arcsec (R:4)
	float xi_resid;
	//double xi_resid;

	// in arcsec (r:4)
	float eta_resid;
	//double eta_resid;

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
	//double mra;

	// in arcsec / yr. (S:3)
	float mu_dec;
	//double mdec;

	// motion probability. (P:1)
	float mu_prob;
	//double mprob;

	// in arcsec / yr. (x:3)
	float sigma_mu_ra;
	//double sig_mra;

	// in arcsec / yr. (y:3)
	float sigma_mu_dec;
	//double sig_mdec;

	// in arcsec. (Q:1)
	float sigma_ra_fit;
	//double sig_mra_fit;

	// in arcsec. (R:1)
	float sigma_dec_fit;
	//double sig_mdec_fit;

	// in arcsec. (u:3)
	float sigma_ra;
	//double sig_ra;

	// in arcsec. (v:3)
	float sigma_dec;
	//double sig_dec;

	// in years; 1950-2050. (e:3)
	float epoch;
	//double epoch;

	// number of detections; (M:1)
	// M=0 means Tycho-2 star.
	// M=1 means it's a reject USNOB star.
	unsigned char ndetections;
	//int ndetections;

	bool diffraction_spike;
	bool motion_catalog;
	// YS4.0 correlation
	bool ys4;

	struct observation obs[5];

	/*
	  double blue1_mag;
	  int blue1_field;
	  int blue1_survey;
	  int blue1_star;
	  int blue1_pmmscan;
	  double red1_mag;
	  int red1_field;
	  int red1_survey;
	  int red1_star;
	  int red1_pmmscan;
	  double blue2_mag;
	  int blue2_field;
	  int blue2_survey;
	  int blue2_star;
	  int blue2_pmmscan;
	  double red2_mag;
	  int red2_field;
	  int red2_survey;
	  int red2_star;
	  int red2_pmmscan;
	  double N_mag;
	  int N_field;
	  int N_survey;
	  int N_star;
	  int N_pmmscan;
	*/
};
typedef struct usnob_entry usnob_entry;

int usnob_parse_entry(unsigned char* line, usnob_entry* usnob);
