#include "sip_qfits.h"
#include "an-bool.h"

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

static bool read_polynomial(qfits_header* hdr, char* format,
							int order, double* data, int datastride) {
	int i, j;
	char key[64];
	double nil = -1e300;
	double val;
	for (i=0; i<=order; i++)
		for (j=0; (i+j)<=order; j++)
			if (i || j) {
				sprintf(key, format, i, j);
				val = qfits_header_getdouble(hdr, key, nil);
				if (val == nil) {
					fprintf(stderr, "SIP: key \"%s\" not found.\n", key);
					return FALSE;
				}
				data[i*datastride + j] = val;
			}
	return TRUE;
}

sip_t* sip_read_header(qfits_header* hdr, sip_t* dest) {
	sip_t sip;
	char* str;
	char* key;
	char* expect;

	memset(&sip, 0, sizeof(sip_t));

	key = "CTYPE1";
	expect = "RA---TAN-SIP";
	str = qfits_header_getstr(hdr, key);
	str = qfits_pretty_string(str);
	if (!str || strncmp(str, expect, strlen(expect))) {
		fprintf(stderr, "SIP header: invalid \"%s\": expected \"%s\", got \"%s\".\n", key, expect, str);
		return NULL;
	}

	key = "CTYPE2";
	expect = "DEC--TAN-SIP";
	str = qfits_header_getstr(hdr, key);
	str = qfits_pretty_string(str);
	if (!str || strncmp(str, expect, strlen(expect))) {
		fprintf(stderr, "SIP header: invalid \"%s\": expected \"%s\", got \"%s\".\n", key, expect, str);
		return NULL;
	}

	if (!tan_read_header(hdr, &sip.wcstan)) {
		fprintf(stderr, "SIP: failed to read TAN header.\n");
		return NULL;
	}

	sip.a_order  = qfits_header_getint(hdr, "A_ORDER", -1);
	sip.b_order  = qfits_header_getint(hdr, "B_ORDER", -1);
	sip.ap_order = qfits_header_getint(hdr, "AP_ORDER", -1);
	sip.bp_order = qfits_header_getint(hdr, "BP_ORDER", -1);

	if ((sip.a_order == -1) || 
		(sip.b_order == -1) || 
		(sip.ap_order == -1) || 
		(sip.bp_order == -1)) {
		fprintf(stderr, "SIP: failed to read polynomial orders.\n");
		return NULL;
	}

	if ((sip.a_order > SIP_MAXORDER) || 
		(sip.b_order > SIP_MAXORDER) || 
		(sip.ap_order > SIP_MAXORDER) || 
		(sip.bp_order > SIP_MAXORDER)) {
		fprintf(stderr, "SIP: polynomial orders (A=%i, B=%i, AP=%i, BP=%i) exceeds maximum of %i.\n",
				sip.a_order, sip.b_order, sip.ap_order, sip.bp_order, SIP_MAXORDER);
		return NULL;
	}

	if (!read_polynomial(hdr, "A_%i_%i",  sip.a_order,  (double*)sip.a,  SIP_MAXORDER) ||
		!read_polynomial(hdr, "B_%i_%i",  sip.b_order,  (double*)sip.b,  SIP_MAXORDER) ||
		!read_polynomial(hdr, "AP_%i_%i", sip.ap_order, (double*)sip.ap, SIP_MAXORDER) ||
		!read_polynomial(hdr, "BP_%i_%i", sip.bp_order, (double*)sip.bp, SIP_MAXORDER)) {
		fprintf(stderr, "SIP: failed to read polynomial terms.\n");
		return NULL;
	}

	if (!dest)
		dest = malloc(sizeof(sip_t));

	memcpy(dest, &sip, sizeof(sip_t));
	return dest;
}

tan_t* tan_read_header(qfits_header* hdr, tan_t* dest) {
	char* str;
	char* key;
	char* expect;
	tan_t tan;
	double nil = -1e300;

	memset(&tan, 0, sizeof(tan_t));

	key = "CTYPE1";
	expect = "RA---TAN";
	str = qfits_header_getstr(hdr, key);
	str = qfits_pretty_string(str);
	if (!str || strncmp(str, expect, strlen(expect))) {
		fprintf(stderr, "TAN header: invalid \"%s\": expected \"%s\", got \"%s\".\n", key, expect, str);
		return NULL;
	}

	key = "CTYPE2";
	expect = "DEC--TAN";
	str = qfits_header_getstr(hdr, key);
	str = qfits_pretty_string(str);
	if (!str || strncmp(str, expect, strlen(expect))) {
		fprintf(stderr, "TAN header: invalid \"%s\": expected \"%s\", got \"%s\".\n", key, expect, str);
		return NULL;
	}

	{
		char* keys[] = { "CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2",
						 "CD1_1", "CD1_2", "CD2_1", "CD2_2" };
		double* vals[] = { &(tan.crval[0]), &(tan.crval[1]),
						   &(tan.crpix[0]), &(tan.crpix[1]),
						   &(tan.cd[0][0]), &(tan.cd[0][1]),
						   &(tan.cd[1][0]), &(tan.cd[1][1]) };
		int i;

		for (i=0; i<8; i++) {
			*(vals[i]) = qfits_header_getdouble(hdr, keys[i], nil);
			if (*(vals[i]) == nil) {
				fprintf(stderr, "TAN header: invalid value for \"%s\".\n", keys[i]);
				return NULL;
			}
		}
	}

	if (!dest)
		dest = malloc(sizeof(tan_t));
	memcpy(dest, &tan, sizeof(tan_t));
	return dest;
}
