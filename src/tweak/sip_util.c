/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Keir Mierle.

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

#include <string.h>
#include <assert.h>
#include <math.h>
#include "sip.h"
#include "sip_util.h"

// libwcs:
#include "wcs.h"
#include "fitsfile.h"

wcs_t* get_wcs_from_fits_header(char* buf)
{
	wcs_t* wcs = wcsninit(buf, strlen(buf));
	assert(wcs);
	return wcs;
}

void copy_wcs_into_sip(wcs_t* wcs, sip_t* sip)
{

	// FIXME need to do sanity checks to ensure this is a tan projection
	// or such. Really, this function is eventually going to be VERY
	// large, as it handles more and more actual image headers that we see
	// in the wild.

	sip->wcstan.crval[0] = wcs->crval[0];
	sip->wcstan.crval[1] = wcs->crval[1];
	sip->wcstan.crpix[0] = wcs->crpix[0];
	sip->wcstan.crpix[1] = wcs->crpix[1];

	// wcs->cd is in row major
	sip->wcstan.cd[0][0] = wcs->cd[0];
	sip->wcstan.cd[0][1] = wcs->cd[1];
	sip->wcstan.cd[1][0] = wcs->cd[2];
	sip->wcstan.cd[1][1] = wcs->cd[3];
}

sip_t* load_sip_from_fitsio(fitsfile* fptr)
{
   sip_t* sip;
	wcs_t* wcs = load_wcs_from_fitsio(fptr);

	if (!wcs) 
		return NULL;

	sip = malloc(sizeof(sip_t));
	memset(sip, 0, sizeof(sip_t));
	sip->a_order = sip->b_order = 0;
	sip->ap_order = sip->bp_order = 0;
	copy_wcs_into_sip(wcs, sip);
	return sip;
}

// Hacky. Pull the complete hdu from the current hdu, then use wcstools to
// figure out the wcs, and return it
wcs_t* load_wcs_from_fitsio(fitsfile* infptr)
{
	int mystatus = 0;
	int* status = &mystatus;
	int nkeys, ii,n;
	int tmpbufflen;
	char* tmpbuff;

	if (ffghsp(infptr, &nkeys, NULL, status) > 0) {
		fprintf(stderr, "nomem\n");
		return NULL;
	}
	fprintf(stderr, "nkeys=%d\n",nkeys);

	// Create a memory buffer to hold the header records
	tmpbufflen = nkeys*(FLEN_CARD-1)*sizeof(char)+1;
	tmpbuff = malloc(tmpbufflen);
	assert(tmpbuff);

	// Read all of the header records in the input HDU
	for (ii = 0; ii < nkeys; ii++) {
		char* thiscard = tmpbuff + (ii * (FLEN_CARD-1));
		if (ffgrec(infptr, ii+1, thiscard, status)) {
			fits_report_error(stderr, *status);
			exit(-1);
		}

		// Stupid hack because ffgrec null terminates
		n = strlen(thiscard);
		if (n!=80) {
			int jj;
			for(jj=n;jj<80;jj++) 
				thiscard[jj] = ' ';
		}
	}
	return wcsninit(tmpbuff, tmpbufflen);
}

