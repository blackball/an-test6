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

// libwcs:
//#include "fitsfile.h"
//#include "wcs.h"

// levmar:
#include "lm.h"

#include "starutil.h"
//#include "fitsio.h"
//#include "kdtree.h"
//#include "kdtree_fits_io.h"
//#include "dualtree_rangesearch.h"
//#include "healpix.h"
//#include "ezfits.h"
#include "sip.h"
#include "sip_util.h"
#include "bl.h"
#include "tweak_internal.h"



#define NUM_FAKE_OBJS 100
#define FAKE_IMAGE_EDGESIZE 256
#define FAKE_IMAGE_DEGREES 1.0
#define FAKE_IMAGE_RA 100
#define FAKE_IMAGE_DEC 100


void get_fake_xy_data(tweak_t* t, int numobjs);
sip_t* make_fake_sip(void);


void get_fake_xy_data(tweak_t* t, int numobjs)
{
	// make numobjs random stars and create both xy and xyz data for them
	int jj;
	t->n=numobjs;
	double tmp1,tmp2;
	double *xyz; // this will hold the xyz "reference" coordinates
	double xyzcentre[3]; // this will hold the xyz of the tangent point

	radecdeg2xyzarr(FAKE_IMAGE_RA,FAKE_IMAGE_DEC,xyzcentre);

	t->x = malloc(sizeof(double)*t->n);
	t->y = malloc(sizeof(double)*t->n);
	xyz = malloc(sizeof(double)*3*t->n);

	for (jj=0; jj<t->n; jj++) {
	  tmp1=drand48()-0.5;
	  tmp2=drand48()-0.5;
	  radecdeg2xyzarr(FAKE_IMAGE_RA +FAKE_IMAGE_DEGREES*tmp1, // ra is ra_centre + noise
							FAKE_IMAGE_DEC+FAKE_IMAGE_DEGREES*tmp2, // dec is dec_centre + noise
							xyz+3*jj);
	  star_coords(xyz+3*jj,xyzcentre,t->x+jj,t->y+jj);
	  t->x[jj] *= 180*FAKE_IMAGE_EDGESIZE/(FAKE_IMAGE_DEGREES*PIl);
	  t->y[jj] *= 180*FAKE_IMAGE_EDGESIZE/(FAKE_IMAGE_DEGREES*PIl);
	  t->x[jj] += FAKE_IMAGE_EDGESIZE/2.0;
	  t->y[jj] += FAKE_IMAGE_EDGESIZE/2.0;
	  fprintf(stderr,"image star %04d: %g,%g\n",jj,t->x[jj],t->y[jj]);
	}
	t->state |= TWEAK_HAS_IMAGE_XY;
	tweak_push_ref_xyz(t,xyz,t->n);
	free(xyz);
}


// Our parameter vector is 'p' for LM. The breakdown of parameters into the
// parameter vector goes in the following order:
//
// OPT_CRVAL       CRVAL0
//                 CRVAL1
//               
// OPT_CRPIX       CRPIX0
//                 CRPIX1
//               
// OPT_CD          CD1_1
//                 CD1_2
//                 CD2_1
//                 CD2_2
//                 
// OPT_SIP         A_1_2         Note that A_1_1 is not present; it is 0
//                 A_1_3
//                 A_1_4
//                 A_1_5
//                 A_1_a_order   All A matrix terms listed in row-major order;
//                               same for B matrix. All terms are such that
//                               i+j <= a_order. Below-cross-diagonal terms
//                               are implicitly zero.
//                 B_1_1
//                 B_1_2         ... and so on

void unpack_params(sip_t* sip, double *pp, int opt_flags) 
{
	assert(pp);
	assert(sip);
	if (opt_flags & OPT_CRVAL) {
		sip->wcstan.crval[0] = *pp++;
		sip->wcstan.crval[1] = *pp++;
	}
	if (opt_flags & OPT_CRPIX) {
		sip->wcstan.crpix[0] = *pp++;
		sip->wcstan.crpix[1] = *pp++;
	}
	if (opt_flags & OPT_CD) {
		sip->wcstan.cd[0][0] = *pp++;
		sip->wcstan.cd[0][1] = *pp++;
		sip->wcstan.cd[1][0] = *pp++;
		sip->wcstan.cd[1][1] = *pp++;
	}
	if (opt_flags & OPT_SIP) {
		int p, q;
		for (p=0; p<sip->a_order; p++)
			for (q=0; q<sip->a_order; q++)
				if (p+q <= sip->a_order && !(p==0&&q==0))
					 sip->a[p][q] = *pp++;
		for (p=0; p<sip->b_order; p++)
			for (q=0; q<sip->b_order; q++)
				if (p+q <= sip->b_order && !(p==0&&q==0))
					 sip->b[p][q] = *pp++;
	}
	if (opt_flags & OPT_SIP_INVERSE) {
		int p, q;
		for (p=0; p<sip->ap_order; p++)
			for (q=0; q<sip->ap_order; q++)
				if (p+q <= sip->ap_order && !(p==0&&q==0))
					 sip->ap[p][q] = *pp++;
		for (p=0; p<sip->bp_order; p++)
			for (q=0; q<sip->bp_order; q++)
				if (p+q <= sip->bp_order && !(p==0&&q==0))
					 sip->bp[p][q] = *pp++;
	}
}

int pack_params(sip_t* sip, double *parameters, int opt_flags) 
{
	double* pp = parameters;
	if (opt_flags & OPT_CRVAL) {
		*pp++ = sip->wcstan.crval[0];
		*pp++ = sip->wcstan.crval[1];
	}
	if (opt_flags & OPT_CRPIX) {
		*pp++ = sip->wcstan.crpix[0];
		*pp++ = sip->wcstan.crpix[1];
	}
	if (opt_flags & OPT_CD) {
		*pp++ = sip->wcstan.cd[0][0];
		*pp++ = sip->wcstan.cd[0][1];
		*pp++ = sip->wcstan.cd[1][0];
		*pp++ = sip->wcstan.cd[1][1];
	}
	if (opt_flags & OPT_SIP) {
//		printf("Packing SIP||||||||\n");
		int p, q;
		for (p=0; p<sip->a_order; p++)
			for (q=0; q<sip->a_order; q++)
				if (p+q <= sip->a_order && !(p==0&&q==0)) {
//					if (sip->a[p][q] != 0.00) 
//						printf("Found nonzero=%le\n", sip->a[p][q]);
					  *pp++ = sip->a[p][q];
				}
		for (p=0; p<sip->b_order; p++)
			for (q=0; q<sip->b_order; q++)
				if (p+q <= sip->b_order && !(p==0&&q==0))
					  *pp++ = sip->b[p][q];
	}

	if (opt_flags & OPT_SIP_INVERSE) {
		int p, q;
		for (p=0; p<sip->ap_order; p++)
			for (q=0; q<sip->ap_order; q++)
				if (p+q <= sip->ap_order && !(p==0&&q==0))
					  *pp++ = sip->ap[p][q];
		for (p=0; p<sip->bp_order; p++)
			for (q=0; q<sip->bp_order; q++)
				if (p+q <= sip->bp_order && !(p==0&&q==0))
					  *pp++ = sip->bp[p][q];
	}
	return pp - parameters;
}

// RIFE with optimization opportunities
void cost(double *p, double *hx, int m, int n, void *adata)
{
	tweak_t* t = adata;
	double err = 0; // calculate our own sum-squared error
	int i;

	sip_t sip;
	memcpy(&sip, t->sip, sizeof(sip_t));
	unpack_params(&sip, p, t->opt_flags);

	// Run the gauntlet.
	for (i=0; i<il_size(t->image); i++) {
		double a, d;
		double image_xyz[3];
		double ref_xyz[3];
		int image_ind,ref_ind;
		double dx,dy,dz;

		if (!il_get(t->included, i)) 
			continue;
		image_ind = il_get(t->image, i);
		sip_pixelxy2radec(&sip, t->x[image_ind], t->y[image_ind], &a, &d);
		radecdeg2xyzarr(a, d, image_xyz);
		*hx++ = image_xyz[0];
		*hx++ = image_xyz[1];
		*hx++ = image_xyz[2];

		ref_ind = il_get(t->ref, i);
		radecdeg2xyzarr(t->a_ref[ref_ind], t->d_ref[ref_ind], ref_xyz);
		dx = ref_xyz[0] - image_xyz[0];
		dy = ref_xyz[1] - image_xyz[1];
		dz = ref_xyz[2] - image_xyz[2];
		err += dx*dx+dy*dy+dz*dz;
//		printf("dx=%le, dy=%le, dz=%le\n",dx,dy,dz);

	}
//	printf("sqd error=%le\n", err);
}

// Do a fit.
// Requires: correspondences     t->image, t->ref,
//           image sources       t->x,t->y,
//           reference sources   t->a_ref,t->d_ref
//           optimization flags  t->opt_flags
//           initial SIP guess   t->sip
void lm_fit(tweak_t* t)
{
   int m,n,i;
	double params[410];
	double *desired;
	double *hx;
	double info[LM_INFO_SZ];
	int max_iterations = 200;
	double opts[] = { 0.001*LM_INIT_MU,
	                  0.001*LM_STOP_THRESH,
	                  0.001*LM_STOP_THRESH,
		          0.001*LM_STOP_THRESH,
	                  0.001*-LM_DIFF_DELTA};

	t->opt_flags = OPT_CRVAL | OPT_CRPIX | OPT_CD | OPT_SIP;
	t->sip->a_order = 7;
	t->sip->b_order = 7;

//	print_sip(t->sip);

	m =  pack_params(t->sip, params, t->opt_flags);


	// Pack target values
	desired = malloc(sizeof(double)*il_size(t->image)*3);
	hx = desired;
	for (i=0; i<il_size(t->image); i++) {
	   int ref_ind;
		double ref_xyz[3];
		if (!il_get(t->included, i)) 
			continue;
		ref_ind = il_get(t->ref, i);
		radecdeg2xyzarr(t->a_ref[ref_ind], t->d_ref[ref_ind], ref_xyz);
		*hx++ = ref_xyz[0];
		*hx++ = ref_xyz[1];
		*hx++ = ref_xyz[2];
	}

	n = hx - desired;
//	assert(hx-desired == 3*il_size(t->image));

	printf("Starting optimization m=%d n=%d!!!!!!!!!!!\n",m,n);
	dlevmar_dif(cost, params, desired, m, n, max_iterations,
	            opts, info, NULL, NULL, t);

	printf("initial error^2 = %le\n", info[0]);
	printf("final   error^2 = %le\n", info[1]);
	printf("nr iterations   = %lf\n", info[5]);
	printf("term reason     = %lf\n", info[6]);
	printf("function evals  = %lf\n", info[7]);

	unpack_params(t->sip, params, t->opt_flags);
	t->err = info[1];
//	memcpy(t->parameters, params, sizeof(double)*t->n);
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

