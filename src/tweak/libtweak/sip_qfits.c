#include "sip_qfits.h"

static void wcs_hdr_common(qfits_header* hdr, tan_t* tan) {
	char val[64];

	// This seemed to be required for libwcs...
	/*
	  qfits_header_mod(hdr, "NAXIS", "2", NULL);
	  qfits_header_add(hdr, "NAXIS1", "1", NULL, NULL);
	  qfits_header_add(hdr, "NAXIS2", "1", NULL, NULL);
	*/
	qfits_header_add(hdr, "WCSAXES", "2", NULL, NULL);

	sprintf(val, "%.12g", tan->crval[0]);
	qfits_header_add(hdr, "CRVAL1", val, "RA  of reference point", NULL);
	sprintf(val, "%.12g", tan->crval[1]);
	qfits_header_add(hdr, "CRVAL2", val, "DEC of reference point", NULL);

	sprintf(val, "%.12g", tan->crpix[0]);
	qfits_header_add(hdr, "CRPIX1", val, "X reference pixel", NULL);
	sprintf(val, "%.12g", tan->crpix[1]);
	qfits_header_add(hdr, "CRPIX2", val, "Y reference pixel", NULL);

	qfits_header_add(hdr, "CUNIT1", "deg", "X pixel scale units", NULL);
	qfits_header_add(hdr, "CUNIT2", "deg", "Y pixel scale units", NULL);

	// bizarrely, this only seems to work when I swap the rows of R.
	sprintf(val, "%.12g", tan->cd[0][0]);
	qfits_header_add(hdr, "CD1_1", val, "Transformation matrix", NULL);
	sprintf(val, "%.12g", tan->cd[0][1]);
	qfits_header_add(hdr, "CD1_2", val, " ", NULL);
	sprintf(val, "%.12g", tan->cd[1][0]);
	qfits_header_add(hdr, "CD2_1", val, " ", NULL);
	sprintf(val, "%.12g", tan->cd[1][1]);
	qfits_header_add(hdr, "CD2_2", val, " ", NULL);
}

static void add_polynomial(qfits_header* hdr, char* format,
						   int order, double* data, int datastride) {
	int i, j;
	char key[64];
	char val[64];
	for (i=0; i<=order; i++)
		for (j=0; (i+j)<=order; j++)
			if (i || j) {
				sprintf(key, format, i, j);
				//sprintf(val, "%.12g", data[i][j]);
				sprintf(val, "%.12g", data[i*datastride + j]);
				qfits_header_add(hdr, key, val, NULL, NULL);
			}
}

void sip_add_to_header(qfits_header* hdr, sip_t* sip) {
	char val[64];

	qfits_header_add(hdr, "CTYPE1", "RA---TAN-SIP", "TAN (gnomic) projection + SIP distortions", NULL);
	qfits_header_add(hdr, "CTYPE2", "DEC--TAN-SIP", "TAN (gnomic) projection + SIP distortions", NULL);

	wcs_hdr_common(hdr, &(sip->wcstan));

	sprintf(val, "%i", sip->a_order);
	qfits_header_add(hdr, "A_ORDER", val, "Polynomial order, axis 1", NULL);
	add_polynomial(hdr, "A_%i_%i", sip->a_order, (double*)sip->a, SIP_MAXORDER);

	sprintf(val, "%i", sip->b_order);
	qfits_header_add(hdr, "B_ORDER", val, "Polynomial order, axis 2", NULL);
	add_polynomial(hdr, "B_%i_%i", sip->b_order, (double*)sip->b, SIP_MAXORDER);

	sprintf(val, "%i", sip->ap_order);
	qfits_header_add(hdr, "AP_ORDER", val, "Inv polynomial order, axis 1", NULL);
	add_polynomial(hdr, "AP_%i_%i", sip->ap_order, (double*)sip->ap, SIP_MAXORDER);

	sprintf(val, "%i", sip->bp_order);
	qfits_header_add(hdr, "BP_ORDER", val, "Inv polynomial order, axis 2", NULL);
	add_polynomial(hdr, "BP_%i_%i", sip->bp_order, (double*)sip->bp, SIP_MAXORDER);
}

qfits_header* sip_create_header(sip_t* sip) {
	qfits_header* hdr = qfits_table_prim_header_default();
	sip_add_to_header(hdr, sip);
	return hdr;
}

void tan_add_to_header(qfits_header* hdr, tan_t* tan) {
	qfits_header_add(hdr, "CTYPE1", "RA---TAN", "TAN (gnomic) projection", NULL);
	qfits_header_add(hdr, "CTYPE2", "DEC--TAN", "TAN (gnomic) projection", NULL);
	wcs_hdr_common(hdr, tan);
}

qfits_header* tan_create_header(tan_t* tan) {
	qfits_header* hdr = qfits_table_prim_header_default();
	tan_add_to_header(hdr, tan);
	return hdr;
}

sip_t* sip_read_header(qfits_header* hdr) {
	return NULL;
}

tan_t* tan_read_header(qfits_header* hdr) {
	return NULL;
}
