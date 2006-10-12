#ifndef TWOMASS_H
#define TWOMASS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>  // to get NAN

/**

   See:
   .    ftp://ftp.ipac.caltech.edu/pub/2mass/allsky/format_psc.html

*/
struct twomass_entry {
	// degrees, J2000 ICRS
	double ra;
	// degrees, J2000 ICRS
	double dec;

	// unique id of this object.
	unsigned int key;

	// arcsec, one-sigma positional error ellipse: major axis
	float err_major;
	// arcsec, one-sigma positional error ellipse: minor axis
	float err_minor;

	// degrees east of north of the major axis of the error ellipse
	unsigned char err_angle;

	// hhmmssss[+-]ddmmsss[ABC...]
	char designation[18];

	// is this in the northern hemisphere?
	// 1=north, 0=south.
	unsigned char northern_hemisphere;

	// mag (null)
	float j_m;
	// mag (null): corrected photometric uncertainty
	float j_cmsig;
	// mag (null): total photometric uncertainty
	float j_msigcom;
	// (null):
	float j_snr;

	// mag (null)
	float h_m;
	// mag (null): corrected photometric uncertainty
	float h_cmsig;
	// mag (null): total photometric uncertainty
	float h_msigcom;
	// (null):
	float h_snr;

	// mag (null)
	float k_m;
	// mag (null): corrected photometric uncertainty
	float k_cmsig;
	// mag (null): total photometric uncertainty
	float k_msigcom;
	// (null):
	float k_snr;

	// photometric quality flags
	unsigned char j_quality;
	unsigned char h_quality;
	unsigned char k_quality;

	// read flags: where did the data come from?
	unsigned char j_read_flag;
	unsigned char h_read_flag;
	unsigned char k_read_flag;

	// blend flags: how many source measurements were blended?
	unsigned char h_blend_flag;
	unsigned char j_blend_flag;
	unsigned char k_blend_flag;

	// contamination and confusion flags: object detection.
	unsigned char j_cc;
	unsigned char h_cc;
	unsigned char k_cc;

	// number of detections (seen, possible)
	unsigned char j_ndet_M;
	unsigned char j_ndet_N;
	unsigned char h_ndet_M;
	unsigned char h_ndet_N;
	unsigned char k_ndet_M;
	unsigned char k_ndet_N;

	// may be a foreground star superimposed on a galaxy.
	unsigned char galaxy_contam;

	// (null) angle (degrees east of north) to the nearest object
	unsigned char prox_angle;

	// arcsec: proximity to the nearest other source in the catalog.
	float proximity;

	// : key of the nearest neighbour.
	unsigned int prox_key;

	// day the observation run was started
	unsigned short date_year;
	unsigned char date_month;
	unsigned char date_day;

	// in days: Julian date (+- 30 seconds) of the measurement.
	double jdate;

	// nightly scan number
	unsigned short scan;
	// may be a minor planet, comet, asteroid, etc.
	unsigned char minor_planet;

	// degrees east of north (null)
	unsigned char phi_opt;



	// degrees: galactic longitude
	float glon;
	// degrees: galactic latitude
	float glat;

	// arcsec
	float x_scan;

	// (null)
	float j_psfchi;
	// mag (null)
	float j_m_stdap;
	// mag (null)
	float j_msig_stdap;

	// (null)
	float h_psfchi;
	// mag (null)
	float h_m_stdap;
	// mag (null)
	float h_msig_stdap;

	// (null)
	float k_psfchi;
	// mag (null)
	float k_m_stdap;
	// mag (null)
	float k_msig_stdap;

	// arcsec (null):
	float dist_opt;
	// mag (null)
	float b_m_opt;
	// mag (null)
	float vr_m_opt;



	  // arcsec:
	unsigned int dist_edge_ns;
	unsigned int dist_edge_ew;
	// 1=north
	unsigned char dist_flag_ns;
	// 1=east
	unsigned char dist_flag_ew;

	unsigned char dup_src;
	unsigned char use_src;


	unsigned char association;

	unsigned char nopt_mchs;

	unsigned short coadd;

	unsigned int scan_key;

	// (null)
	unsigned int xsc_key;

	unsigned int coadd_key;
};
typedef struct twomass_entry twomass_entry;

#define TWOMASS_NULL NAN

#define TWOMASS_KEY_NULL 0xffffff

#define TWOMASS_ANGLE_NULL 0xff

enum twomass_association_val {
	TWOMASS_ASSOCIATION_NONE,
	TWOMASS_ASSOCIATION_TYCHO,
	TWOMASS_ASSOCIATION_USNOA2
};

enum twomass_quality_val {
	TWOMASS_QUALITY_NO_BRIGHTNESS,    // X flag
	TWOMASS_QUALITY_UPPER_LIMIT_MAG,  // U
	TWOMASS_QUALITY_NO_SIGMA,         // F
	TWOMASS_QUALITY_BAD_FIT,          // E
	TWOMASS_QUALITY_A,
	TWOMASS_QUALITY_B,
	TWOMASS_QUALITY_C,
	TWOMASS_QUALITY_D
};

enum twomass_cc_val {
	TWOMASS_CC_NONE,
	TWOMASS_CC_PERSISTENCE,     // p flag
	TWOMASS_CC_CONFUSION,       // c
	TWOMASS_CC_DIFFRACTION,     // d
	TWOMASS_CC_STRIPE,          // s
	TWOMASS_CC_BANDMERGE        // b
};

int twomass_parse_entry(struct twomass_entry* entry, char* line);

int twomass_cc_flag(unsigned char val, unsigned char flag);

int twomass_quality_flag(unsigned char val, unsigned char flag);

int twomass_is_null_float(float f);

#endif
