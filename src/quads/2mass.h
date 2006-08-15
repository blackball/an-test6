#ifndef TWOMASS_H
#define TWOMASS_H

#define TWOMASS_NULL_MAG NAN

/**
   See:
   .    ftp://ftp.ipac.caltech.edu/pub/2mass/allsky/format_psc.html
*/
struct twomass_entry {
	// degrees, J2000 ICRS
	double ra;
	double dec;
	// arcsec, one-sigma positional error ellipse
	float err_major;
	float err_minor;
	// degrees east of north of the major axis of the error ellipse
	//float err_angle;
	unsigned char err_angle;

	// hhmmssss[+-]ddmmsss[ABC...]
	char designation[18];

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

	// quality bits:
	unsigned j_no_brightness   :1; // flag X
	unsigned j_upper_limit_mag :1; // flag U
	unsigned j_no_sigma        :1; // flag F
	unsigned j_bad_fit         :1; // flag E
	unsigned j_quality_A       :1; // flag A
	unsigned j_quality_B       :1; // flag B
	unsigned j_quality_C       :1; // flag C
	unsigned j_quality_D       :1; // flag D

	unsigned h_no_brightness   :1; // flag X
	unsigned h_upper_limit_mag :1; // flag U
	unsigned h_no_sigma        :1; // flag F
	unsigned h_bad_fit         :1; // flag E
	unsigned h_quality_A       :1; // flag A
	unsigned h_quality_B       :1; // flag B
	unsigned h_quality_C       :1; // flag C
	unsigned h_quality_D       :1; // flag D

	unsigned k_no_brightness   :1; // flag X
	unsigned k_upper_limit_mag :1; // flag U
	unsigned k_no_sigma        :1; // flag F
	unsigned k_bad_fit         :1; // flag E
	unsigned k_quality_A       :1; // flag A
	unsigned k_quality_B       :1; // flag B
	unsigned k_quality_C       :1; // flag C
	unsigned k_quality_D       :1; // flag D

	unsigned j_read_flag       :4;
	unsigned h_read_flag       :4;
	unsigned k_read_flag       :4;

	unsigned j_blend_flag      :2;
	unsigned h_blend_flag      :2;
	unsigned k_blend_flag      :2;

	unsigned j_cc_persistence  :1; // flag p
	unsigned j_cc_confusion    :1; // flag c
	unsigned j_cc_diffraction  :1; // flag d
	unsigned j_cc_stripe       :1; // flag s
	unsigned j_cc_bandmerge    :1; // flag b

	unsigned h_cc_persistence  :1; // flag p
	unsigned h_cc_confusion    :1; // flag c
	unsigned h_cc_diffraction  :1; // flag d
	unsigned h_cc_stripe       :1; // flag s
	unsigned h_cc_bandmerge    :1; // flag b

	unsigned k_cc_persistence  :1; // flag p
	unsigned k_cc_confusion    :1; // flag c
	unsigned k_cc_diffraction  :1; // flag d
	unsigned k_cc_stripe       :1; // flag s
	unsigned k_cc_bandmerge    :1; // flag b

	unsigned j_ndet_M          :3; // number of detections
	unsigned j_ndet_N          :3; // potential number of detections

	unsigned h_ndet_M          :3; // number of detections
	unsigned h_ndet_N          :3; // potential number of detections

	unsigned k_ndet_M          :3; // number of detections
	unsigned k_ndet_N          :3; // potential number of detections

	// may be a foreground star superimposed on a galaxy.
	unsigned galaxy_contam     :2;

	// may be a minor planet, comet, asteroid, etc.
	unsigned minor_planet      :1;

	unsigned northern_hemisphere  :1;

	unsigned date_year            :12;
	unsigned date_month           :4;
	unsigned date_day             :5;

	unsigned scan                 :10;

	// arcsec: proximity to the nearest other source in the catalog.
	float proximity;
	// : key of the nearest neighbour.
	unsigned int prox_key;

	// unique id of this object.
	unsigned int key;

	// degrees: galactic longitude
	float glon;
	// degrees: galactic latitude
	float glat;

	// arcsec
	float x_scan;

	// in days: Julian date (+- 30 seconds) of the measurement.
	double jdate;

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

	// arcsec:
	unsigned dist_edge_ns     :17;
	// arcsec:
	unsigned dist_edge_ew     :10;
	// 1=north
	unsigned dist_flag_ns     :1;
	// 1=east
	unsigned dist_flag_ew     :1;

	unsigned dup_src          :2;
	unsigned use_src          :1;

	unsigned association      :2;

	unsigned nopt_mchs    :4;

	// (null)
	unsigned xsc_key      :24;
	unsigned scan_key     :17;
	unsigned coadd_key    :24;
	unsigned coadd        :10;

	// degrees east of north (null)
	unsigned char phi_opt;
	// arcsec (null):
	float dist_opt;
	// mag (null)
	float b_m_opt;
	// mag (null)
	float vr_m_opt;
};
typedef struct twomass_entry twomass_entry;


#endif
