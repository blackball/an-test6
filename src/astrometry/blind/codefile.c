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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "codefile.h"
#include "starutil.h"
#include "ioutils.h"
#include "fitsioutils.h"
#include "errors.h"

#define CHUNK_CODES 0

static fitsbin_chunk_t* codes_chunk(codefile* cf) {
    return fitsbin_get_chunk(cf->fb, CHUNK_CODES);
}

static int callback_read_header(fitsbin_t* fb, fitsbin_chunk_t* chunk) {
    qfits_header* primheader = fitsbin_get_primary_header(fb);
	codefile* cf = chunk->userdata;

    cf->dimcodes = qfits_header_getint(primheader, "DIMCODES", 4);
    cf->numcodes = qfits_header_getint(primheader, "NCODES", -1);
    cf->numstars = qfits_header_getint(primheader, "NSTARS", -1);
    cf->index_scale_upper = qfits_header_getdouble(primheader, "SCALE_U", -1.0);
    cf->index_scale_lower = qfits_header_getdouble(primheader, "SCALE_L", -1.0);
	cf->indexid = qfits_header_getint(primheader, "INDEXID", 0);
	cf->healpix = qfits_header_getint(primheader, "HEALPIX", -1);
	cf->hpnside = qfits_header_getint(primheader, "HPNSIDE", 1);

	if ((cf->numcodes == -1) || (cf->numstars == -1) ||
		(cf->index_scale_upper == -1.0) || (cf->index_scale_lower == -1.0)) {
        ERROR("Couldn't find NCODES or NSTARS or SCALE_U or SCALE_L entries in FITS header");
		return -1;
	}
    if (fits_check_endian(primheader)) {
		ERROR("File was written with the wrong endianness");
		return -1;
    }
    chunk->itemsize = cf->dimcodes * sizeof(double);
    chunk->nrows = cf->numcodes;
	return 0;
}

static codefile* new_codefile(const char* fn, bool writing) {
	fitsbin_chunk_t chunk;
	codefile* cf = calloc(1, sizeof(codefile));
	if (!cf) {
		SYSERROR("Couldn't calloc a codefile struct");
		return NULL;
	}
	cf->healpix = -1;
    cf->hpnside = 1;

    if (writing)
        cf->fb = fitsbin_open_for_writing(fn);
    else
        cf->fb = fitsbin_open(fn);
    if (!cf->fb) {
        ERROR("Failed to create fitsbin");
        return NULL;
    }

    fitsbin_chunk_init(&chunk);
    chunk.tablename = "codes";
    chunk.required = 1;
    chunk.callback_read_header = callback_read_header;
    chunk.userdata = cf;
    fitsbin_add_chunk(cf->fb, &chunk);
    fitsbin_chunk_clean(&chunk);

	return cf;
}

void codefile_get_code(const codefile* cf, int codeid, double* code) {
	int i;
	if (codeid >= cf->numcodes) {
		ERROR("Requested codeid %i, but number of codes is %i", codeid, cf->numcodes);
        assert(codeid < cf->numcodes);
        // just carry on - we'll probably segfault.
	}
	for (i=0; i<cf->dimcodes; i++)
		code[i] = cf->codearray[codeid * cf->dimcodes + i];
}

int codefile_close(codefile* cf) {
    int rtn;
	if (!cf) return 0;
	rtn = fitsbin_close(cf->fb);
	free(cf);
    return rtn;
}

codefile* codefile_open(const char* fn) {
    codefile* cf = NULL;

    cf = new_codefile(fn, FALSE);
    if (!cf)
        goto bailout;
    if (fitsbin_read(cf->fb)) {
        ERROR("Failed to open codes file");
        goto bailout;
    }
	cf->codearray = codes_chunk(cf)->data;
    return cf;

 bailout:
    if (cf)
        codefile_close(cf);
    return NULL;
}

codefile* codefile_open_for_writing(const char* fn) {
	codefile* cf;
	qfits_header* hdr;

	cf = new_codefile(fn, TRUE);
	if (!cf)
		goto bailout;
    // default
    cf->dimcodes = 4;

	// add default values to header
	hdr = codefile_get_header(cf);
    fits_add_endian(hdr);
	qfits_header_add(hdr, "AN_FILE", "CODE", "This file lists the code for each quad.", NULL);
	qfits_header_add(hdr, "NCODES", "0", "", NULL);
	qfits_header_add(hdr, "NSTARS", "0", "", NULL);
	fits_header_add_int(hdr, "DIMCODES", cf->dimcodes, "");
	qfits_header_add(hdr, "SCALE_U", "0.0", "", NULL);
	qfits_header_add(hdr, "SCALE_L", "0.0", "", NULL);
	qfits_header_add(hdr, "INDEXID", "0", "", NULL);
	qfits_header_add(hdr, "HEALPIX", "-1", "", NULL);
	qfits_header_add(hdr, "HPNSIDE", "1", "", NULL);
	fits_add_long_comment(hdr, "The first extension contains the codes "
						  "stored as %i native-endian doubles.  "
						  "(the quad location in %i-D code space)", cf->dimcodes, cf->dimcodes);
	return cf;

 bailout:
	if (cf)
		codefile_close(cf);
	return NULL;
}

int codefile_write_header(codefile* cf) {
	fitsbin_t* fb = cf->fb;
    fitsbin_chunk_t* chunk = codes_chunk(cf);
	chunk->itemsize = cf->dimcodes * sizeof(double);
	chunk->nrows = cf->numcodes;

	if (fitsbin_write_primary_header(fb) ||
		fitsbin_write_chunk_header(fb, chunk)) {
		ERROR("Failed to write codefile header");
		return -1;
	}
	return 0;
}

int codefile_fix_header(codefile* cf) {
	qfits_header* hdr;
	fitsbin_t* fb = cf->fb;
    fitsbin_chunk_t* chunk = codes_chunk(cf);
	chunk->itemsize = cf->dimcodes * sizeof(double);
	chunk->nrows = cf->numcodes;

	hdr = codefile_get_header(cf);

	// fill in the real values...
	fits_header_mod_int(hdr, "DIMCODES", cf->dimcodes, "Number of values in a code.");
	fits_header_mod_int(hdr, "NCODES", cf->numcodes, "Number of codes.");
	fits_header_mod_int(hdr, "NSTARS", cf->numstars, "Number of stars.");
	fits_header_mod_double(hdr, "SCALE_U", cf->index_scale_upper, "Upper-bound index scale (radians).");
	fits_header_mod_double(hdr, "SCALE_L", cf->index_scale_lower, "Lower-bound index scale (radians).");
	fits_header_mod_int(hdr, "INDEXID", cf->indexid, "Index unique ID.");
	fits_header_mod_int(hdr, "HEALPIX", cf->healpix, "Healpix of this index.");
	fits_header_mod_int(hdr, "HPNSIDE", cf->hpnside, "Nside of the healpixelization");

	if (fitsbin_fix_primary_header(fb) ||
		fitsbin_fix_chunk_header(fb, chunk)) {
        ERROR("Failed to fix code header");
		return -1;
	}
	return 0;
}

int codefile_write_code(codefile* cf, double* code) {
    fitsbin_chunk_t* chunk = codes_chunk(cf);
    if (fitsbin_write_item(cf->fb, chunk, code)) {
		ERROR("Failed to write code");
		return -1;
	}
	cf->numcodes++;
	return 0;
}

qfits_header* codefile_get_header(const codefile* cf) {
	return fitsbin_get_primary_header(cf->fb);
}

int codefile_dimcodes(const codefile* cf) {
	return cf->dimcodes;
}
