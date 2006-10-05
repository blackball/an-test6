#include <string.h>
#include <assert.h>
#include "sip.h"
#include "sip_util.h"
#include "libwcs/wcs.h"
#include "libwcs/fitsfile.h"

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

	sip->crval[0] = wcs->crval[0];
	sip->crval[1] = wcs->crval[1];
	sip->crpix[0] = wcs->crpix[0];
	sip->crpix[1] = wcs->crpix[1];

	// wcs->cd is in row major
	sip->cd[0][0] = wcs->cd[0];
	sip->cd[0][1] = wcs->cd[1];
	sip->cd[1][0] = wcs->cd[2];
	sip->cd[1][1] = wcs->cd[3];
}

sip_t* load_sip_from_fitsio(fitsfile* fptr)
{
	wcs_t* wcs = load_wcs_from_fitsio(fptr);

	if (!wcs) 
		return NULL;

	sip_t* sip = malloc(sizeof(sip_t));
	memset(sip, 0, sizeof(sip_t));
	sip->a_order = sip->b_order = 0;
	sip->ap_order = sip->bp_order = 0;
	copy_wcs_into_sip(wcs, sip);
	return sip;
}

void print_sip(sip_t* sip)
{

	printf("SIP Structure:\n");
	printf("crval[0]=%lf\n", sip->crval[0]);
	printf("crval[1]=%lf\n", sip->crval[1]);
	printf("crpix[0]=%lf\n", sip->crpix[0]);
	printf("crpix[1]=%lf\n", sip->crpix[1]);

	printf("cd00=%lf\n", sip->cd[0][0]);
	printf("cd01=%lf\n", sip->cd[0][1]);
	printf("cd10=%lf\n", sip->cd[1][0]);
	printf("cd11=%lf\n", sip->cd[1][1]);
	printf("\n");
}

// Hacky. Pull the complete hdu from the current hdu, then use wcstools to
// figure out the wcs, and return it
wcs_t* load_wcs_from_fitsio(fitsfile* infptr)
{
	int mystatus = 0;
	int* status = &mystatus;
	int nkeys, ii;

	if (ffghsp(infptr, &nkeys, NULL, status) > 0) {
		fprintf(stderr, "nomem\n");
		return NULL;
	}
	fprintf(stderr, "nkeys=%d\n",nkeys);

	// Create a memory buffer to hold the header records
	int tmpbufflen = nkeys*(FLEN_CARD-1)*sizeof(char)+1;
	char* tmpbuff = malloc(tmpbufflen);
	assert(tmpbuff);

	// Read all of the header records in the input HDU
	for (ii = 0; ii < nkeys; ii++) {
		char* thiscard = tmpbuff + (ii * (FLEN_CARD-1));
		if (ffgrec(infptr, ii+1, thiscard, status)) {
			fits_report_error(stderr, *status);
			exit(-1);
		}

		// Stupid hack because ffgrec null terminates
		int n = strlen(thiscard);
		if (n!=80) {
			int jj;
			for(jj=n;jj<80;jj++) 
				thiscard[jj] = ' ';
		}
	}
	return wcsninit(tmpbuff, tmpbufflen);
}

