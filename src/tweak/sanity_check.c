/*
  NAME:
    sanity_check
  PURPOSE:
    do an internal self-consistency check on tweak/tweak_internal
  INPUTS:
    Run sanity_check -h to see help
  OPTIONAL INPUTS:
    None yet
  KEYWORD:
  BUGS:

  REVISION HISTORY: created by sam monday april9, 2007
  WHO TO SHOOT IF IT BREAKS: sam
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
#include "sip.h"
#include "sip_util.h"
#include "bl.h"
#include "tweak_internal.h"

#define NUM_FAKE_OBJS 100
#define FAKE_IMAGE_EDGESIZE 256
#define NOISE 0.5


void get_fake_xy_data(tweak_t* t, int numobjs);
sip_t* make_fake_sip(void);

void get_fake_xy_data(tweak_t* t, int numobjs)
{
	// make numobjs random stars and create both xy and xyz data for them
	int jj;
	double *xyz; // this will hold the xyz "reference" coordinates
	double xyzcentre[3]; // this will hold the xyz of the tangent point
	t->n=numobjs;

	// WTF? why won't the next line compile?
//	tan_pixelxy2xyzarr(&(t->sip.wcstan), 100,100,xyzcentre);
//
	tan_pixelxy2xyzarr((tan_t*)t->sip, 100,100,xyzcentre);

	t->x = malloc(sizeof(double)*t->n);
	t->y = malloc(sizeof(double)*t->n);
	xyz = malloc(sizeof(double)*3*t->n);

	for (jj=0; jj<t->n; jj++) {
	  double px = FAKE_IMAGE_EDGESIZE * (drand48()-0.5);
	  double py = FAKE_IMAGE_EDGESIZE * (drand48()-0.5);
	  tan_pixelxy2xyzarr((tan_t*)t->sip, px,py, xyz+3*jj);

	  // add noise in the image; later we can perturb the reference too
	  double nx = NOISE * (drand48()-0.5);
	  double ny = NOISE * (drand48()-0.5);
	  t->x[jj] = px + nx;
	  t->y[jj] = py + ny;
	  fprintf(stderr,"image star %04d: %g,%g\n",jj,t->x[jj],t->y[jj]);
	}
	t->state |= TWEAK_HAS_IMAGE_XY;
	tweak_push_ref_xyz(t,xyz,t->n);
	free(xyz);
}


void printHelp(char* progname)
{
	fprintf(stderr, "%s usage:\n"
	        "   -h print this help\n"
	        , progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
	char argchar;
	int k;

	// the tweak struct
	tweak_t tweak;
	// the fitting order
	int order = 4;

	tweak_init(&tweak);

	tweak.Nside = 0;

	// set jitter: 6 arcsec.
	tweak.jitter = 6.0;

	while ((argchar = getopt(argc, argv, "h")) != -1)
	  switch (argchar) {
	  case 'h':
		 printHelp(argv[0]);
		 exit(0);
	  }

   fprintf(stderr,"getting fake WCS\n");


	// pretend to get the WCS data
	tweak.sip = make_fake_sip(); //was load_sip_from_fitsio
	sip_print(tweak.sip);
	tweak.state = TWEAK_HAS_SIP;

   fprintf(stderr,"getting fake XY list\n");

	// pretend to get image XY data
	get_fake_xy_data(&tweak,NUM_FAKE_OBJS); //was get_xy_data
	//tweak_push_hppath(&tweak, hppat);

	tweak_go_to(&tweak, TWEAK_HAS_LINEAR_CD);
	tweak_go_to(&tweak, TWEAK_HAS_REF_XY);
	tweak_go_to(&tweak, TWEAK_HAS_IMAGE_AD);
	tweak_go_to(&tweak, TWEAK_HAS_CORRESPONDENCES);
	//tweak_dump_ascii(&tweak);

	tweak.sip->a_order = order;
	tweak.sip->b_order = order;
	tweak.sip->ap_order = order;
	tweak.sip->bp_order = order;
	
	for (k=0; k<6; k++) {
	  fprintf(stderr,"\n");
	  fprintf(stderr,"--------------------------------\n");
	  fprintf(stderr,"Iterating linear tweak number %d\n", k);
	  tweak.state &= ~TWEAK_HAS_LINEAR_CD;
	  tweak_go_to(&tweak, TWEAK_HAS_LINEAR_CD);
	}
	tweak_go_to(&tweak, TWEAK_HAS_REF_XY);
	tweak_go_to(&tweak, TWEAK_HAS_IMAGE_AD);
	tweak_go_to(&tweak, TWEAK_HAS_CORRESPONDENCES);
	fprintf(stderr,"final state: ");
	tweak_print_state(&tweak);
	//tweak_dump_ascii(&tweak);
	sip_print(tweak.sip);
	printf("\n");
	
	exit(1);
}




sip_t* make_fake_sip(void)
{
   sip_t* sip;
	char fakewcs[] ="NAXIS = 2                                                                       "
	                "NAXIS1 = 256                                                                    "
	                "NAXIS2 = 256                                                                    "
	                "CTYPE1 = 'RA---TAN'                                                             "
	                "CTYPE2 = 'DEC--TAN'                                                             "
	                "CRPIX1 = 128                                                                    "
	                "CRPIX2 = 128                                                                    "
	                "CD1_1  = .004                                                                   "
	                "CD1_2  = 0                                                                      "
	                "CD2_1  = 0                                                                      "
	                "CD2_2  = .004                                                                   "
	                "CRVAL1 = 100                                                                    "
	                "CRVAL2 = 100                                                                    ";

	wcs_t* wcs = wcsinit(fakewcs);

	if (!wcs) 
		return NULL;

	sip = malloc(sizeof(sip_t));
	memset(sip, 0, sizeof(sip_t));
	sip->a_order = sip->b_order = 0;
	sip->ap_order = sip->bp_order = 0;
	copy_wcs_into_sip(wcs, sip);
	return sip;
}

