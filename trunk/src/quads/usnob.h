#include <stdint.h>

typedef unsigned char bool;
typedef unsigned int uint;

#define USNOB_RECORD_SIZE 80

struct observation {
	// 0 to 99.99
	double mag;
	// field number in the original survey. 1-937
	unsigned short field;
	// the original survey.
	unsigned char survey;
	// star/galaxy estimate.  0=galaxy, 11=star. 19=no value computed.
	unsigned char star_galaxy;
	// in arcsec
	double xi_resid;
	// in arcsec
	double eta_resid;
	// source of photometric calibration:
	//  0=bright photometric standard on this plate
	//  1=faint pm standard on this plate
	//  2=faint " " one plate away
	//  etc
	unsigned char calibration;
	// back-pointer to PMM file.
	uint pmmscan;
};

#define OBS_BLUE1 0
#define OBS_RED1  1
#define OBS_BLUE2 2
#define OBS_RED2  3
#define OBS_N     4

struct usnob_entry {
	// in degrees
	double ra;
	// in degrees
	double dec;
	// in arcsec / yr.
	double mra;
	// in arcsec / yr.
	double mdec;
	// probability.
	double mprob;
	bool motion_catalog;
	// in arcsec / yr.
	double sig_mra;
	// in arcsec / yr.
	double sig_mdec;
	// in arcsec.
	double sig_mra_fit;
	// in arcsec.
	double sig_mdec_fit;
	// number of detections;
	// M=0 means Tycho-2 star.
	// M=1 means it's a reject USNOB star.
	int ndetections;
	bool diffraction_spike;
	// in arcsec.
	double sig_ra;
	// in arcsec.
	double sig_dec;
	// in years; 1950-2050.
	double epoch;
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
