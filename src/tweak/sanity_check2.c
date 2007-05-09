/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Sam Roweis.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <assert.h>

#include "starutil.h"
#include "mathutil.h"
#include "sip.h"
#include "sip_util.h"
#include "bl.h"
#include "tweak_internal.h"

#define NUM_FAKE_OBJS 200
#define FAKE_IMAGE_EDGESIZE 250
#define FAKE_IMAGE_DEGREES 1.0
#define FAKE_IMAGE_RA 100
#define FAKE_IMAGE_DEC 80
#define NOISE_PIXELS 2 

// pixel error
double pixjitter = 1.0;

// pixel scale, arcsec/pixel
double pixscale = 0.4;

double ra_center = 100.0;
double dec_center = 75.0;

double x_center = 1024;
double y_center = 750;

double W = 2048;
double H = 1489;

double rot = 30;
//double rot = 0;

int Nstars = 20;

static char* OPTIONS = "ht:";

static void printHelp(char* progname)
{
	fprintf(stderr, "%s usage:\n"
	        "   -h print this help\n"
			"   [-t <test-number>]: run a particular test.\n"
	        , progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
	char argchar;
	//int k;

	// the tweak struct
	tweak_t tweak;

	sip_t* true_sip;
	double true_x[Nstars], true_y[Nstars];
	double true_xyz[3 * Nstars];

	double x[Nstars], y[Nstars];
	double xyz[3 * Nstars];
	double ra[Nstars], dec[Nstars];
	sip_t* sip;

	tan_t* wcs;
	int i;

	int test = 0;

	while ((argchar = getopt(argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			printHelp(argv[0]);
			exit(0);
		case 't':
			test = atoi(optarg);
			break;
		}

	printf("Running test %i\n", test);

	true_sip = sip_create();
	wcs = &(true_sip->wcstan);

	// Set up true WCS.
	wcs->crval[0] = ra_center;
	wcs->crval[1] = dec_center;
	wcs->crpix[0] = x_center;
	wcs->crpix[1] = y_center;
	{
		double rotrad = deg2rad(rot);
		double psdeg  = arcsec2deg(pixscale);
		wcs->cd[0][0] = psdeg *  cos(rotrad);
		wcs->cd[0][1] = psdeg *  sin(rotrad);
		wcs->cd[1][0] = psdeg * -sin(rotrad);
		wcs->cd[1][1] = psdeg *  cos(rotrad);
	}

	// Sample true stars.
	for (i=0; i<Nstars; i++) {
		true_x[i] = uniform_sample(0.0, W);
		true_y[i] = uniform_sample(0.0, H);
		sip_pixelxy2xyzarr(true_sip, true_x[i], true_y[i], true_xyz + 3*i);
	}

	printf("True SIP:\n");
	sip_print_to(true_sip, stdout);

	sip = sip_create();

	// Set observed values = True values.
	memcpy(sip, true_sip, sizeof(sip_t));
	memcpy(x,   true_x,   Nstars * sizeof(double));
	memcpy(y,   true_y,   Nstars * sizeof(double));
	memcpy(xyz, true_xyz, 3 * Nstars * sizeof(double));

	for (i=0; i<Nstars; i++)
		xyzarr2radecdeg(xyz + 3*i, ra + i, dec + i);

	if (test == 1) {
		// Offset the pixel values by (-20, 100).
		for (i=0; i<Nstars; i++) {
			x[i] += -20;
			y[i] += 100;
		}
	} else if (test == 2) {
		// Offset the pixel values by (-1, 2);
		printf("Adding (-1, 2) pixels to field objects.\n");
		for (i=0; i<Nstars; i++) {
			x[i] += -1;
			y[i] +=  2;
		}
	}

	// Stuff the observed values into tweak.
	tweak_init(&tweak);
	tweak.Nside = 0;
	tweak.jitter = 6.0 * pixscale * pixjitter;

	tweak_push_wcs_tan(&tweak, &(sip->wcstan));
	tweak_push_ref_xyz(&tweak, xyz, Nstars);
	tweak_push_ref_ad(&tweak, ra, dec, Nstars);
	tweak_push_image_xy(&tweak, x, y, Nstars);

	tweak.sip->a_order = tweak.sip->b_order = 1;
	tweak.sip->ap_order = tweak.sip->bp_order = 3;

	/*
	  tweak.sip->wcstan.crval[0]-=.5;
	  tweak.sip->wcstan.crval[1]-=.5;
	*/

	// Avoid shift.
	tweak.state |= TWEAK_HAS_REALLY_FINELY_SHIFTED;

	tweak_go_to(&tweak, TWEAK_HAS_LINEAR_CD);
	tweak_go_to(&tweak, TWEAK_HAS_REF_XY);
	tweak_go_to(&tweak, TWEAK_HAS_IMAGE_AD);
	tweak_go_to(&tweak, TWEAK_HAS_CORRESPONDENCES);
	//tweak_dump_ascii(&tweak);

	/*
	//for (k=0; k<6; k++) {
	printf("\n");
	printf("--------------------------------\n");
	printf("Iterating linear tweak number %d\n", k);
	tweak.state &= ~TWEAK_HAS_LINEAR_CD;
	tweak_go_to(&tweak, TWEAK_HAS_LINEAR_CD);
	//}
	*/
	tweak_go_to(&tweak, TWEAK_HAS_REF_XY);
	tweak_go_to(&tweak, TWEAK_HAS_IMAGE_AD);
	tweak_go_to(&tweak, TWEAK_HAS_CORRESPONDENCES);

	printf("final state: ");
	//tweak_print_state(&tweak);
	//tweak_dump_ascii(&tweak);
	sip_print_to(tweak.sip, stdout);

	printf("\n");

	exit(1);
}

