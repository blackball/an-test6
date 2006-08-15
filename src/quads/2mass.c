#define _GNU_SOURCE
#include <math.h>  // to get NAN

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "2mass.h"

// parses a value that may be null "\N".
// expects the value to be followed by "|".
static int parse_null(char** pcursor, float* dest) {
	int nchars;
	if (!strncmp(*pcursor, "\\N|", 3)) {
		*dest = TWOMASS_NULL;
		*pcursor += 3;
		return 0;
	}
	if (sscanf(*pcursor, "%f|%n", dest, &nchars) != 1) {
		return -1;
	}
	*pcursor += nchars;
	return 0;
}

int twomass_parse_entry(struct twomass_entry* e, char* line) {
	char* cursor;
	int nchars;
	int i;
	double vals1[5];
	float val2;

	cursor = line;
	for (i=0; i<5; i++) {
		char* names[] = { "ra", "dec", "err_maj", "err_min", "err_ang" };
		if (sscanf(cursor, "%lf|%n", vals1+i, &nchars) != 1) {
			fprintf(stderr, "Failed to parse \"%s\" entry in 2MASS line.\n", names[i]);
			return -1;
		}
		cursor += nchars;
	}
	e->ra = vals1[0];
	e->dec = vals1[1];
	e->err_major = vals1[2];
	e->err_minor = vals1[3];
	e->err_angle = vals1[4];

	strncpy(e->designation, cursor, 17);
	e->designation[17] = '\0';
	if (strlen(e->designation) != 17) {
		fprintf(stderr, "Failed to parse \"designation\" entry in 2MASS line.\n");
		return -1;
	}
	cursor += 18;

	for (i=0; i<12; i++) {
		char* names[] = {
			"j_m", "j_cmsig", "j_msigcom", "j_snr",
			"h_m", "h_cmsig", "h_msigcom", "h_snr",
			"k_m", "k_cmsig", "k_msigcom", "k_snr" };
		float* dests[] = {
			&e->j_m, &e->j_cmsig, &e->j_msigcom, &e->j_snr,
			&e->h_m, &e->h_cmsig, &e->h_msigcom, &e->h_snr,
			&e->k_m, &e->k_cmsig, &e->k_msigcom, &e->k_snr };
		if (parse_null(&cursor, dests[i])) {
			fprintf(stderr, "Failed to parse \"%s\" entry in 2MASS line.\n", names[i]);
			return -1;
		}
	}

	e->j_no_brightness = e->j_upper_limit_mag = e->j_no_sigma = e->j_bad_fit =
		e->j_quality_A = e->j_quality_B = e->j_quality_C = e->j_quality_D = 0;
	switch (*cursor) {
	case 'X':
		e->j_no_brightness = 1;
		break;
	case 'U':
		e->j_upper_limit_mag = 1;
		break;
	case 'F':
		e->j_no_sigma = 1;
		break;
	case 'E':
		e->j_bad_fit = 1;
		break;
	case 'A':
		e->j_quality_A = 1;
		break;
	case 'B':
		e->j_quality_B = 1;
		break;
	case 'C':
		e->j_quality_C = 1;
		break;
	case 'D':
		e->j_quality_D = 1;
		break;
	}
	cursor++;

	// ugh, the disadvantage of bitfields...

	e->h_no_brightness = e->h_upper_limit_mag = e->h_no_sigma = e->h_bad_fit =
		e->h_quality_A = e->h_quality_B = e->h_quality_C = e->h_quality_D = 0;
	switch (*cursor) {
	case 'X':
		e->h_no_brightness = 1;
		break;
	case 'U':
		e->h_upper_limit_mag = 1;
		break;
	case 'F':
		e->h_no_sigma = 1;
		break;
	case 'E':
		e->h_bad_fit = 1;
		break;
	case 'A':
		e->h_quality_A = 1;
		break;
	case 'B':
		e->h_quality_B = 1;
		break;
	case 'C':
		e->h_quality_C = 1;
		break;
	case 'D':
		e->h_quality_D = 1;
		break;
	}
	cursor++;

	e->k_no_brightness = e->k_upper_limit_mag = e->k_no_sigma = e->k_bad_fit =
		e->k_quality_A = e->k_quality_B = e->k_quality_C = e->k_quality_D = 0;
	switch (*cursor) {
	case 'X':
		e->k_no_brightness = 1;
		break;
	case 'U':
		e->k_upper_limit_mag = 1;
		break;
	case 'F':
		e->k_no_sigma = 1;
		break;
	case 'E':
		e->k_bad_fit = 1;
		break;
	case 'A':
		e->k_quality_A = 1;
		break;
	case 'B':
		e->k_quality_B = 1;
		break;
	case 'C':
		e->k_quality_C = 1;
		break;
	case 'D':
		e->k_quality_D = 1;
		break;
	default:
		assert(0);
	}
	cursor++;

	assert(*cursor == '|');
	cursor++;

	if (cursor[0] < '0' || cursor[0] > '9') {
		fprintf(stderr, "Error parsing read_flag from 2MASS entry.\n");
		return -1;
	}
	e->j_read_flag = '0' + cursor[0];
	cursor++;

	if (cursor[0] < '0' || cursor[0] > '9') {
		fprintf(stderr, "Error parsing read_flag from 2MASS entry.\n");
		return -1;
	}
	e->h_read_flag = '0' + cursor[0];
	cursor++;

	if (cursor[0] < '0' || cursor[0] > '9') {
		fprintf(stderr, "Error parsing read_flag from 2MASS entry.\n");
		return -1;
	}
	e->k_read_flag = '0' + cursor[0];
	cursor++;

	assert(*cursor == '|');
	cursor++;

	if (!strncmp(cursor, ">1", 2)) {
		cursor += 2;
		e->j_blend_flag = 2;
	} else if (cursor[0] == '0') {
		cursor++;
		e->j_blend_flag = 0;
	} else if (cursor[0] == '1') {
		cursor++;
		e->j_blend_flag = 1;
	} else
		assert(0);

	if (!strncmp(cursor, ">1", 2)) {
		cursor += 2;
		e->h_blend_flag = 2;
	} else if (cursor[0] == '0') {
		cursor++;
		e->h_blend_flag = 0;
	} else if (cursor[0] == '1') {
		cursor++;
		e->h_blend_flag = 1;
	} else
		assert(0);

	if (!strncmp(cursor, ">1", 2)) {
		cursor += 2;
		e->k_blend_flag = 2;
	} else if (cursor[0] == '0') {
		cursor++;
		e->k_blend_flag = 0;
	} else if (cursor[0] == '1') {
		cursor++;
		e->k_blend_flag = 1;
	} else
		assert(0);

	assert(*cursor == '|');
	cursor++;

	e->j_cc_persistence = e->j_cc_confusion = e->j_cc_diffraction =
		e->j_cc_stripe = e->j_cc_bandmerge = 0;
	switch (*cursor) {
	case 'p':
		e->j_cc_persistence = 1;
		break;
	case 'c':
		e->j_cc_confusion = 1;
		break;
	case 'd':
		e->j_cc_diffraction = 1;
		break;
	case 's':
		e->j_cc_stripe = 1;
		break;
	case 'b':
		e->j_cc_bandmerge = 1;
		break;
	case '0':
		break;
	default:
		assert(0);
	}
	cursor++;

	e->h_cc_persistence = e->h_cc_confusion = e->h_cc_diffraction =
		e->h_cc_stripe = e->h_cc_bandmerge = 0;
	switch (*cursor) {
	case 'p':
		e->h_cc_persistence = 1;
		break;
	case 'c':
		e->h_cc_confusion = 1;
		break;
	case 'd':
		e->h_cc_diffraction = 1;
		break;
	case 's':
		e->h_cc_stripe = 1;
		break;
	case 'b':
		e->h_cc_bandmerge = 1;
		break;
	case '0':
		break;
	default:
		assert(0);
	}
	cursor++;

	e->k_cc_persistence = e->k_cc_confusion = e->k_cc_diffraction =
		e->k_cc_stripe = e->k_cc_bandmerge = 0;
	switch (*cursor) {
	case 'p':
		e->k_cc_persistence = 1;
		break;
	case 'c':
		e->k_cc_confusion = 1;
		break;
	case 'd':
		e->k_cc_diffraction = 1;
		break;
	case 's':
		e->k_cc_stripe = 1;
		break;
	case 'b':
		e->k_cc_bandmerge = 1;
		break;
	case '0':
		break;
	default:
		assert(0);
	}
	cursor++;

	assert(*cursor == '|');
	cursor++;

	assert(*cursor >= '0' && *cursor <= '9');
	e->j_ndet_M = *cursor - '0';
	cursor++;
	assert(*cursor >= '0' && *cursor <= '9');
	e->j_ndet_N = *cursor - '0';
	cursor++;
	assert(*cursor >= '0' && *cursor <= '9');
	e->h_ndet_M = *cursor - '0';
	cursor++;
	assert(*cursor >= '0' && *cursor <= '9');
	e->h_ndet_N = *cursor - '0';
	cursor++;
	assert(*cursor >= '0' && *cursor <= '9');
	e->k_ndet_M = *cursor - '0';
	cursor++;
	assert(*cursor >= '0' && *cursor <= '9');
	e->k_ndet_N = *cursor - '0';
	cursor++;

	assert(*cursor == '|');
	cursor++;

	if (sscanf(cursor, "%f|%n", &e->proximity, &nchars) != 1) {
		fprintf(stderr, "Failed to parse 'proximity' entry in 2MASS line.\n");
		return -1;
	}
	cursor += nchars;

	if (parse_null(&cursor, &val2)) {
		fprintf(stderr, "Failed to parse 'proximity' entry in 2MASS line.\n");
		return -1;
	}
	e->prox_angle = val2;

	if (sscanf(cursor, "%u|%n", &e->prox_key, &nchars) != 1) {
		fprintf(stderr, "Failed to parse 'prox_key' entry in 2MASS line.\n");
		return -1;
	}
	cursor += nchars;

	if (*cursor < '0' || *cursor > '2') {
		fprintf(stderr, "Failed to parse 'galaxy_contam' entry in 2MASS line.\n");
		return -1;
	}
	e->galaxy_contam = *cursor - '0';
	cursor ++;

	assert(*cursor == '|');
	cursor++;

	if (*cursor < '0' || *cursor > '1') {
		fprintf(stderr, "Failed to parse 'minor_planet' entry in 2MASS line.\n");
		return -1;
	}
	e->minor_planet = *cursor - '0';
	cursor ++;

	assert(*cursor == '|');
	cursor++;

	if (sscanf(cursor, "%u|%n", &e->key, &nchars) != 1) {
		fprintf(stderr, "Failed to parse 'key' entry in 2MASS line.\n");
		return -1;
	}
	cursor += nchars;

	switch (*cursor) {
	case 'n':
		e->northern_hemisphere = 1;
		break;
	case 's':
		e->northern_hemisphere = 0;
		break;
	default:
		fprintf(stderr, "Failed to parse 'northern_hemisphere' entry in 2MASS line.\n");
		return -1;
	}
	cursor ++;

	assert(*cursor == '|');
	cursor++;

	{
		unsigned int yr, mon, day, scan;
		if (sscanf(cursor, "%u-%u-%u|%n", &yr, &mon, &day, &nchars) != 3) {
			fprintf(stderr, "Failed to parse 'date' entry in 2MASS line.\n");
			return -1;
		}
		cursor += nchars;
		e->date_year = yr;
		e->date_month = mon;
		e->date_day = day;

		if (sscanf(cursor, "%u|%n", &scan, &nchars) != 1) {
			fprintf(stderr, "Failed to parse 'scan' entry in 2MASS line.\n");
			return -1;
		}
		cursor += nchars;
		e->scan = scan;

	}

	if (sscanf(cursor, "%f|%f|%n", &e->glon, &e->glat, &nchars) != 2) {
		fprintf(stderr, "Failed to parse 'glon/glat' entry in 2MASS line.\n");
		return -1;
	}
	cursor += nchars;

	if (sscanf(cursor, "%f|%lf|%n", &e->x_scan, &e->jdate, &nchars) != 2) {
		fprintf(stderr, "Failed to parse 'x_scan/jdate' entry in 2MASS line.\n");
		return -1;
	}
	cursor += nchars;

	for (i=0; i<9; i++) {
		char* names[] = {
			"j_psfchi", "h_psfchi", "k_psfchi",
			"j_m_stdap", "j_msig_stdap",
			"h_m_stdap", "h_msig_stdap",
			"k_m_stdap", "k_msig_stdap" };
		float* dests[] = {
			&e->j_psfchi, &e->h_psfchi, &e->k_psfchi,
			&e->j_m_stdap, &e->j_msig_stdap,
			&e->h_m_stdap, &e->h_msig_stdap,
			&e->k_m_stdap, &e->k_msig_stdap, };
		if (parse_null(&cursor, dests[i])) {
			fprintf(stderr, "Failed to parse \"%s\" entry in 2MASS line.\n", names[i]);
			return -1;
		}
	}

	assert(*cursor == '|');
	cursor++;

	{
		int dist_ns;
		int dist_ew;
		char ns;
		char ew;
		if (sscanf(cursor, "%i|%i|%c%c|%n", &dist_ns, &dist_ew, &ns, &ew, &nchars) != 4) {
			fprintf(stderr, "Failed to parse 'dist_edge_ns/dest_edge_ew/dist_edge_flag' entries in 2MASS line.\n");
			return -1;
		}
		e->dist_edge_ns = dist_ns;
		e->dist_edge_ew = dist_ew;
		switch (ns) {
		case 'n':
			e->dist_flag_ns = 1;
			break;
		case 's':
			e->dist_flag_ns = 0;
			break;
		default:
			fprintf(stderr, "Failed to parse 'dist_edge_flag' entry in 2MASS line.\n");
			return -1;
		}
		switch (ew) {
		case 'e':
			e->dist_flag_ew = 1;
			break;
		case 'w':
			e->dist_flag_ew = 0;
			break;
		default:
			fprintf(stderr, "Failed to parse 'dist_edge_flag' entry in 2MASS line.\n");
			return -1;
		}
	}

	if (!strncmp(cursor, ">1", 2)) {
		e->dup_src = 2;
		cursor += 2;
	} else if ((*cursor == '1') || (*cursor == '0')) {
		e->dup_src = *cursor - '0';
		cursor++;
	} else {
		fprintf(stderr, "Failed to parse 'dup_src' entry in 2MASS line.\n");
		return -1;
	}

	assert(*cursor == '|');
	cursor++;

	if ((*cursor == '1') || (*cursor == '0')) {
		e->use_src = *cursor - '0';
		cursor++;
	} else {
		fprintf(stderr, "Failed to parse 'use_src' entry in 2MASS line.\n");
		return -1;
	}

	assert(*cursor == '|');
	cursor++;

	switch (*cursor) {
	case '0':
		e->association = TWOMASS_ASSOCIATION_NONE;
		break;
	case 'T':
		e->association = TWOMASS_ASSOCIATION_TYCHO;
		break;
	case 'U':
		e->association = TWOMASS_ASSOCIATION_USNOA2;
		break;

	}
	cursor++;

	assert(*cursor == '|');
	cursor++;

	if (parse_null(&cursor, &e->dist_opt) ||
		parse_null(&cursor, &val2) ||
		parse_null(&cursor, &e->b_m_opt) ||
		parse_null(&cursor, &e->vr_m_opt)) {
		fprintf(stderr, "Failed to parse 'dist_opt/phi_opt/b_m_opt/vr_m_opt' entries in 2MASS line.\n");
		return -1;
	}
	e->phi_opt = val2;

	if ((*cursor >= '0') && (*cursor <= '9')) {
		e->nopt_mchs = *cursor - '0';
		cursor++;
	} else {
		fprintf(stderr, "Failed to parse 'nopt_mchs' entry in 2MASS line.\n");
		return -1;
	}
	cursor++;

	if (!strncmp(cursor, "\\N|", 3)) {
		e->xsc_key = TWOMASS_XSC_KEY_NULL;
		cursor += 3;
	} else {
		int ival;
		if (sscanf(cursor, "%i|%n", &ival, &nchars) != 1) {
			fprintf(stderr, "Failed to parse 'xsc_key' entry in 2MASS line.\n");
			return -1;
		}
		cursor += nchars;
	}

	{
		int scan, coadd_key, coadd;
		if (sscanf(cursor, "%i|%i|%i%n", &scan, &coadd_key, &coadd, &nchars) != 3) {
			fprintf(stderr, "Failed to parse 'scan/coadd_key/coadd' entries in 2MASS line.\n");
			return -1;
		}
		e->scan_key = scan;
		e->coadd_key = coadd_key;
		e->coadd = coadd;
		cursor += nchars;
	}

	return 0;
}

