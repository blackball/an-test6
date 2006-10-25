
#ifndef _TWEAK_INTERNAL_H
#define _TWEAK_INTERNAL_H

#include "kdtreeh."
#include "bl.h"

enum opt_flags {
	OPT_CRVAL         = 1,
	OPT_CRPIX         = 2,
	OPT_CD            = 4,
	OPT_SIP           = 8,
	OPT_SIP_INVERSE   = 16,
	OPT_SHIFT         = 32
};

// These flags represent the work already done on a tweak problem
enum tweak_flags {
	TWEAK_HAS_SIP              = 1,
	TWEAK_HAS_IMAGE_XY         = 2,
	TWEAK_HAS_IMAGE_XYZ        = 4,
	TWEAK_HAS_IMAGE_AD         = 8,
	TWEAK_HAS_REF_XY           = 16, 
	TWEAK_HAS_REF_XYZ          = 32, 
	TWEAK_HAS_REF_AD           = 64, 
	TWEAK_HAS_AD_BAR_AND_R     = 128,
	TWEAK_HAS_CORRESPONDENCES  = 256,
	TWEAK_HAS_RUN_OPT          = 512,
	TWEAK_HAS_RUN_RANSAC_OPT   = 1024,
	TWEAK_HAS_COARSLY_SHIFTED  = 2048,
	TWEAK_HAS_FINELY_SHIFTED   = 4096,
};
// FIXME add a method to print out the state in a readable way

// Really what we want is some sort of fancy dependency system... oh well
typedef struct tweak_s {
	sip_t* sip;
	unsigned int state; // bitfield of tweak_flags

	// For sources in the image
	int n;
	double *a;
	double *d;
	double *x;
	double *y;
	double *xyz;

	// Center of field estimate
	double a_bar;  // degrees
	double d_bar;  // degrees
	double radius; // radians (genius!)

	// Cached values of sources in the catalog
	int n_ref;
	double *x_ref;
	double *y_ref;
	double *a_ref;
	double *d_ref;
	double *xyz_ref;

	// Correspondences
	il* image;
	il* ref;
	dl* dist2;

	// Correspondence subsets for RANSAC
	il* maybeinliers;
	il* bestinliers;
	il* included;

	int opt_flags;
//	int n;
//	int m;
//	double parameters[500];
	double err;

	// Size of Hough space for shift
	double mindx, mindy, maxdx, maxdy;

	// Trees used for finding correspondences
	kdtree_t* kd_image;
	kdtree_t* kd_ref;
} tweak_t;

tweak_t* tweak_new();
void tweak_push_ref_xyz(tweak_t* t, double* xyz, int n);
int tweak_advance(tweak_t* t);

#endif
