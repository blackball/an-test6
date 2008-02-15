/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include "2mass-fits.h"
#include "fitsioutils.h"
#include "starutil.h"

static int refill_buffer(void* userdata, void* buffer, uint offset, uint n) {
	twomass_catalog* cat = userdata;
	twomass_entry* en = buffer;
	return twomass_catalog_read_entries(cat, offset, n, en);
}

static twomass_catalog* cat_new() {
	twomass_catalog* cat = calloc(1, sizeof(twomass_catalog));
	if (!cat) {
		fprintf(stderr, "Couldn't allocate memory for a twomass_catalog structure.\n");
		exit(-1);
	}
	cat->br.blocksize = 1000;
	cat->br.elementsize = sizeof(twomass_entry);
	cat->br.refill_buffer = refill_buffer;
	cat->br.userdata = cat;
	return cat;
}

// This is a naughty preprocessor function because it uses variables
// declared in the scope from which it is called.
#define ADDARR(ctype, ftype, col, units, member, arraysize)             \
    if (write) {                                                        \
        fitstable_add_column_struct                                     \
            (tab, ctype, 1, offsetof(twomass_entry, member),            \
             ftype, col, units, TRUE);                                  \
    } else {                                                            \
        fitstable_add_column_struct                                     \
            (tab, ctype, 1, offsetof(twomass_entry, member),            \
             any, col, units, TRUE);                                    \
    }

#define ADDCOL(ctype, ftype, col, units, member) \
ADDARR(ctype, ftype, col, units, member, 1)

static void add_columns(fitstable_t* tab, bool write) {
    tfits_type any = fitscolumn_any_type();
    tfits_type d = fitscolumn_double_type();
    tfits_type f = fitscolumn_float_type();
    tfits_type u8 = fitscolumn_u8_type();
    tfits_type b = fitscolumn_bool_type();
    tfits_type i16 = fitscolumn_i16_type();
    tfits_type j = TFITS_BIN_TYPE_J;
    tfits_type I = TFITS_BIN_TYPE_I;
    tfits_type i = fitscolumn_int_type();
    tfits_type c = fitscolumn_char_type();
    tfits_type logical = fitscolumn_boolean_type();
    char* nil = " ";

    // gawk '{printf("\t%-10s %-8s %-20s %-7s %s\n", $1, $2, $3, $4, $5);}'

	ADDCOL(d,  d,       "RA",                "deg",  ra);
	ADDCOL(d,  d,       "DEC",               "deg",  dec);
	ADDCOL(i,  j,       "KEY",               nil,    key);
	ADDCOL(f,  f,       "ERR_MAJOR",         "deg",  err_major);
	ADDCOL(f,  f,       "ERR_MINOR",         "deg",  err_minor);
	ADDCOL(f,  u8,      "ERR_ANGLE",         "deg",  err_angle);

    // FIXME - be sure to NULL-terminate this.
	ADDARR(c,  c,       "DESIGNATION",       nil,    designation, 17);

	ADDCOL(b,  logical, "NORTHERN_HEMI",     nil,    northern_hemisphere);
	ADDCOL(u8, u8,      "GALAXY_CONTAM",     nil,    galaxy_contam);
	ADDCOL(f,  f,       "PROX",              "deg",  proximity);
	ADDCOL(f,  u8,      "PROX_ANGLE",        "deg",  prox_angle);
	ADDCOL(i,  j,       "PROX_KEY",          nil,    prox_key);
	ADDCOL(i16, I,       "DATE_YEAR",         "yr",   date_year);
	ADDCOL(u8, u8,      "DATE_MONTH",        "month", date_month);
	ADDCOL(u8, u8,      "DATE_DAY",          "day",  date_day);
	ADDCOL(d,  d,       "JDATE",             "day",  date_day);
	ADDCOL(i16, i,       "SCAN",              nil,    scan);
	ADDCOL(b,  logical, "MINOR_PLANET",      nil,    minor_planet);
	ADDCOL(f,  u8,      "PHI_OPT",           "deg",  phi_opt);
	ADDCOL(f,  f,       "GLON",              "deg",  glon);
	ADDCOL(f,  f,       "GLAT",              "deg",  glat);
	ADDCOL(f,  f,       "X_SCAN",            "deg",  x_scan);
	ADDCOL(u8, u8,      "N_OPT_MATCHES",     nil,    nopt_mchs);
	ADDCOL(f,  f,       "DIST_OPT",          "deg",  dist_opt);
	ADDCOL(f,  f,       "B_M_OPT",           "mag",  b_m_opt);
	ADDCOL(f,  f,       "VR_M_OPT",          "mag",  vr_m_opt);
	ADDCOL(f,  f,       "DIST_EDGE_NS",      "deg",  dist_edge_ns);
	ADDCOL(f,  f,       "DIST_EDGE_EW",      "deg",  dist_edge_ew);
	ADDCOL(b,  logical, "DIST_FLAG_NS",      nil,    dist_flag_ns);
	ADDCOL(b,  logical, "DIST_FLAG_EW",      nil,    dist_flag_ew);
	ADDCOL(u8, u8,      "DUP_SRC",           nil,    dup_src);
	ADDCOL(b,  logical, "USE_SRC",           nil,    use_src);
	ADDCOL(c,  c,       "ASSOCATION",        nil,    association);
	ADDCOL(i,  j,       "COADD_KEY",         nil,    coadd_key);
	ADDCOL(i16, I,       "COADD",             nil,    coadd);
	ADDCOL(i,  j,       "SCAN_KEY",          nil,    scan_key);
	ADDCOL(i,  j,       "XSC_KEY",           nil,    xsc_key);
	                                                 
	ADDCOL(f,  f,       "J_MAG",             "mag",  j_m);
	ADDCOL(f,  f,       "J_CMSIG",           "mag",  j_cmsig);
	ADDCOL(f,  f,       "J_MSIGCOM",         "mag",  j_msigcom);
	ADDCOL(f,  f,       "J_M_STDAP",         "mag",  j_m_stdap);
	ADDCOL(f,  f,       "J_MSIG_STDAP",      "mag",  j_msig_stdap);
	ADDCOL(f,  f,       "J_SNR",             nil,    j_snr);
	ADDCOL(c,  c,       "J_QUALITY",         nil,    j_quality);
	ADDCOL(u8, u8,      "J_READ",            nil,    j_read_flag);
	ADDCOL(u8, u8,      "J_BLEND",           nil,    j_blend_flag);
	ADDCOL(c,  c,       "J_CC",              nil,    j_cc);
	ADDCOL(u8, u8,      "J_NDET_M",          nil,    j_ndet_M);
	ADDCOL(u8, u8,      "J_NDET_N",          nil,    j_ndet_N);
	ADDCOL(f,  f,       "J_PSFCHI",          nil,    j_psfchi);

	ADDCOL(f,  f,       "H_MAG",             "mag",  h_m);
	ADDCOL(f,  f,       "H_CMSIG",           "mag",  h_cmsig);
	ADDCOL(f,  f,       "H_MSIGCOM",         "mag",  h_msigcom);
	ADDCOL(f,  f,       "H_M_STDAP",         "mag",  h_m_stdap);
	ADDCOL(f,  f,       "H_MSIG_STDAP",      "mag",  h_msig_stdap);
	ADDCOL(f,  f,       "H_SNR",             nil,    h_snr);
	ADDCOL(c,  c,       "H_QUALITY",         nil,    h_quality);
	ADDCOL(u8, u8,      "H_READ",            nil,    h_read_flag);
	ADDCOL(u8, u8,      "H_BLEND",           nil,    h_blend_flag);
	ADDCOL(c,  c,       "H_CC",              nil,    h_cc);
	ADDCOL(u8, u8,      "H_NDET_M",          nil,    h_ndet_M);
	ADDCOL(u8, u8,      "H_NDET_N",          nil,    h_ndet_N);
	ADDCOL(f,  f,       "H_PSFCHI",          nil,    h_psfchi);

	ADDCOL(f,  f,       "K_MAG",             "mag",  k_m);
	ADDCOL(f,  f,       "K_CMSIG",           "mag",  k_cmsig);
	ADDCOL(f,  f,       "K_MSIGCOM",         "mag",  k_msigcom);
	ADDCOL(f,  f,       "K_M_STDAP",         "mag",  k_m_stdap);
	ADDCOL(f,  f,       "K_MSIG_STDAP",      "mag",  k_msig_stdap);
	ADDCOL(f,  f,       "K_SNR",             nil,    k_snr);
	ADDCOL(c,  c,       "K_QUALITY",         nil,    k_quality);
	ADDCOL(u8, u8,      "K_READ",            nil,    k_read_flag);
	ADDCOL(u8, u8,      "K_BLEND",           nil,    k_blend_flag);
	ADDCOL(c,  c,       "K_CC",              nil,    k_cc);
	ADDCOL(u8, u8,      "K_NDET_M",          nil,    k_ndet_M);
	ADDCOL(u8, u8,      "K_NDET_N",          nil,    k_ndet_N);
	ADDCOL(f,  f,       "K_PSFCHI",          nil,    k_psfchi);
}
#undef ADDCOL
#undef ADDARR

twomass_catalog* twomass_catalog_open(char* fn) {
	twomass_catalog* cat = NULL;
	cat = cat_new();
    if (!cat)
        return NULL;
    cat->ft = fitstable_open(fn);
    if (!cat->ft) {
        fprintf(stderr, "2mass-catalog: failed to open table.\n");
        twomass_catalog_close(cat);
        return NULL;
    }
    add_columns(cat->ft, FALSE);
    if (fitstable_read_extension(cat->ft, 1)) {
        fprintf(stderr, "2mass-catalog: table in extension 1 didn't contain the required columns.\n");
        twomass_catalog_close(cat);
        return NULL;
    }
	cat->br.ntotal = fitstable_nrows(cat->ft);
	return cat;
}

twomass_catalog* twomass_catalog_open_for_writing(char* fn) {
	twomass_catalog* cat;
    qfits_header* hdr;
	cat = cat_new();
    if (!cat)
        return NULL;
    cat->ft = fitstable_open_for_writing(fn);
    if (!cat->ft) {
        fprintf(stderr, "2mass-catalog: failed to open table.\n");
        twomass_catalog_close(cat);
        return NULL;
    }
    hdr = fitstable_get_primary_header(cat->ft);
	//qfits_header_add(hdr, "2MASS", "T", "This is a 2-MASS catalog.", NULL);
    //qfits_header_add(hdr, "AN_FILE", AN_FILETYPE_2MASS, "Astrometry.net file type", NULL);
    return cat;
}

int twomass_catalog_write_headers(twomass_catalog* cat) {
    if (fitstable_write_primary_header(cat->ft))
        return -1;
    return fitstable_write_header(cat->ft);
}

int twomass_catalog_fix_headers(twomass_catalog* cat) {
    if (fitstable_fix_primary_header(cat->ft))
        return -1;
    return fitstable_fix_header(cat->ft);
}

int twomass_catalog_read_entries(twomass_catalog* cat, uint offset,
								 uint count, twomass_entry* entries) {
    return fitstable_read_structs(cat->ft, entries, sizeof(twomass_entry),
                                  offset, count);
}

twomass_entry* twomass_catalog_read_entry(twomass_catalog* cat) {
	twomass_entry* e = buffered_read(&cat->br);
	if (!e)
		fprintf(stderr, "Failed to read an 2MASS catalog entry.\n");
	return e;
}

int twomass_catalog_count_entries(twomass_catalog* cat) {
	return fitstable_nrows(cat->ft);
}

int twomass_catalog_close(twomass_catalog* cat) {
    if (fitstable_close(cat->ft)) {
        fprintf(stderr, "Error closing AN catalog file: %s\n", strerror(errno));
        return -1;
    }
	buffered_read_free(&cat->br);
	free(cat);
	return 0;
}

qfits_header* twomass_catalog_get_primary_header(const twomass_catalog* cat) {
    return fitstable_get_primary_header(cat->ft);
}

int twomass_catalog_write_entry(twomass_catalog* cat, twomass_entry* entry) {
    return fitstable_write_struct(cat->ft, entry);
}
