typedef unsigned char bool;

struct usnob_entry {
	double ra;
	double dec;
	double mra;
	double mdec;
	double mprob;
	double sig_mra_fit;
	double sig_mdec_fit;
	bool motion_catalog;
	int ndetections;
	bool diffraction_spike;
	double sig_ra;
	double sig_dec;
	double epoch;
	bool ys4;

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
};
typedef struct usnob_entry usnob_entry;

