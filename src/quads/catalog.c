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

#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "catalog.h"
#include "fitsioutils.h"
#include "ioutils.h"
#include "mathutil.h"

int catalog_write_to_file(catalog* cat, char* fn)
{
	qfits_table* table;
	qfits_header* tablehdr;
	qfits_header* hdr;
	uint datasize;
	uint ncols, nrows, tablesize;
	FILE *catfid = NULL;
	catfid = fopen(fn, "wb");
	if (!catfid) {
		fflush(stdout);
		fprintf(stderr, "Couldn't open catalog output file %s: %s\n",
		        fn, strerror(errno));
		return -1;
	}

	// the header
	hdr = qfits_table_prim_header_default();
	fits_add_endian(hdr);
	fits_add_double_size(hdr);
	fits_header_add_int(hdr, "NSTARS", cat->numstars, "Number of stars used.");
	qfits_header_add(hdr, "AN_FILE", AN_FILETYPE_CATALOG, "This file has a list of object positions.", NULL);
	fits_header_add_int(hdr, "HEALPIX", cat->healpix, "The healpix covered by this catalog.");
	qfits_header_dump(hdr, catfid);
	qfits_header_destroy(hdr);

	// first table: the star locations.
	datasize = DIM_STARS * sizeof(double);
	ncols = 1;
	// may be dummy
	nrows = cat->numstars;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "xyz", "", "", "", 0, 0, 0, 0, 0);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, catfid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);

	if (fwrite(cat->stars, sizeof(double), cat->numstars*DIM_STARS, catfid) !=
	        cat->numstars * DIM_STARS) {
		fflush(stdout);
		fprintf(stderr, "Failed to write catalog data to file %s: %s.\n",
		        fn, strerror(errno));
		fclose(catfid);
		return -1;
	}

	fits_pad_file(catfid);

	if (fclose(catfid)) {
		fflush(stdout);
		fprintf(stderr, "Couldn't close catalog file %s: %s\n",
		        fn, strerror(errno));
		return -1;
	}
	return 0;
}

qfits_header* catalog_get_header(catalog* cat) {
    return cat->fb->primheader;
}

int catalog_write_header(catalog* cat) {
	fitsbin_t* fb = cat->fb;
	fb->chunks[0].nrows = cat->numstars;

	if (fitsbin_write_primary_header(fb) ||
		fitsbin_write_header(fb)) {
		fprintf(stderr, "Failed to write catalog header.\n");
		return -1;
	}
	return 0;
}

int catalog_fix_header(catalog* cat) {
	qfits_header* hdr;
	fitsbin_t* fb = cat->fb;
	fb->chunks[0].nrows = cat->numstars;

	hdr = fb->primheader;

	// fill in the real values...
     fits_header_mod_int(hdr, "NSTARS", cat->numstars, "Number of stars.");
     fits_header_mod_int(hdr, "HEALPIX", cat->healpix, "Healpix covered by this catalog.");

	if (fitsbin_fix_primary_header(fb) ||
		fitsbin_fix_header(fb)) {
        fprintf(stderr, "Failed to fix catalog header.\n");
		return -1;
	}
	return 0;
}

void catalog_compute_radecminmax(catalog* cat) {
	double ramin, ramax, decmin, decmax;
	int i;
	ramin = HUGE_VAL;
	ramax = -HUGE_VAL;
	decmin = HUGE_VAL;
	decmax = -HUGE_VAL;
	for (i = 0; i < cat->numstars; i++) {
		double* xyz;
		double ra, dec;
		xyz = catalog_get_star(cat, i);
		ra = xy2ra(xyz[0], xyz[1]);
		dec = z2dec(xyz[2]);
		if (ra > ramax)
			ramax = ra;
		if (ra < ramin)
			ramin = ra;
		if (dec > decmax)
			decmax = dec;
		if (dec < decmin)
			decmin = dec;
	}
	cat->ramin = ramin;
	cat->ramax = ramax;
	cat->decmin = decmin;
	cat->decmax = decmax;
}

double* catalog_get_base(catalog* cat) {
	return cat->stars;
}

static int callback_read_header(qfits_header* primheader, qfits_header* header,
								size_t* expected, char** errstr, void* userdata) {
	catalog* cat = userdata;

	cat->numstars = qfits_header_getint(primheader, "NSTARS", -1);
	cat->healpix = qfits_header_getint(primheader, "HEALPIX", -1);
	if (fits_check_endian(primheader)) {
		if (errstr) *errstr = "Catalog file was written with wrong endianness.\n";
        return -1;
	}
	if (cat->numstars == -1) {
		if (errstr) *errstr = "Couldn't find NSTARS header in catalog file.";
        return -1;
	}

    *expected = cat->numstars * DIM_STARS * sizeof(double);
	return 0;
}

static int callback_read_mags(qfits_header* primheader, qfits_header* header,
                              size_t* expected, char** errstr, void* userdata) {
	catalog* cat = userdata;
    *expected = cat->numstars * sizeof(float);
	return 0;
}

static catalog* new_catalog() {
	catalog* cat;
	fitsbin_chunk_t* chunk;

    cat = calloc(1, sizeof(catalog));
	if (!cat) {
		fprintf(stderr, "catalog_open: malloc failed.\n");
	}
    cat->fb = fitsbin_new(2);

    // Star positions
    chunk = cat->fb->chunks + 0;
    chunk->tablename = strdup("xyz");
    chunk->required = 1;
    chunk->callback_read_header = callback_read_header;
    chunk->userdata = cat;
	chunk->itemsize = DIM_STARS * sizeof(double);

    // Star magnitudes
    chunk = cat->fb->chunks + 1;
    chunk->tablename = strdup("mags");
    chunk->required = 0;
    chunk->callback_read_header = callback_read_mags;
    chunk->userdata = cat;
    chunk->itemsize = sizeof(float);

    return cat;
}

catalog* catalog_open(char* catfn) {
    catalog* cat = NULL;
	fitsbin_t* fb = NULL;
	char** errstr;

    cat = new_catalog();
    if (!cat)
        goto bailout;

    fb->filename = strdup(catfn);
    fb->errstr = errstr;

    if (fitsbin_read(fb)) {
        if (*fb->errstr)
            fprintf(stderr, "%s\n", *fb->errstr);
        else
            fprintf(stderr, "fitsbin_read() failed.\n");
		goto bailout;
	}
	cat->fb = fb;
	cat->stars = fb->chunks[0].data;
    cat->mags  = fb->chunks[1].data;
    return cat;

 bailout:
    if (cat) {
        if (cat->fb)
            fitsbin_close(fb);
        free(cat);
    }
    return NULL;
}

catalog* catalog_open_for_writing(char* fn)  {
	catalog* cat;
	qfits_header* hdr;

	cat = new_catalog();
	if (!cat)
		goto bailout;

    if (fitsbin_start_write(cat->fb))
		//fprintf(stderr, "%s\n", errstr);
        goto bailout;

	// add default values to header
	hdr = cat->fb->primheader;
	qfits_header_add(hdr, "AN_FILE", "OBJS", "This file has a list of object positions.", NULL);
    fits_add_endian(hdr);
	fits_add_double_size(hdr);
	qfits_header_add(hdr, "NSTARS", "0", "Number of stars in this file.", NULL);
	qfits_header_add(hdr, "HEALPIX", "-1", "Healpix covered by this catalog.", NULL);
	qfits_header_add(hdr, "COMMENT", "This is a flat array of XYZ for each catalog star.", NULL, NULL);
	qfits_header_add(hdr, "COMMENT", "  (ie, star position on the unit sphere)", NULL, NULL);
	qfits_header_add(hdr, "COMMENT", "  (stored as three native-{endian,size} doubles)", NULL, NULL);
	return cat;

 bailout:
	if (cat)
		free(cat);
	return NULL;
}

double* catalog_get_star(catalog* cat, uint sid) {
	if (sid >= cat->numstars) {
		fflush(stdout);
		fprintf(stderr, "catalog: asked for star %u, but catalog size is only %u.\n",
		        sid, cat->numstars);
		return NULL;
	}
	return cat->stars + sid * 3;
}

int catalog_write_star(catalog* cat, double* star) {
	if (!cat->fb->fid) {
		fflush(stdout);
		fprintf(stderr, "Couldn't write a star: file ID null.\n");
		assert(0);
		return -1;
	}

	if (fwrite(star, sizeof(double), DIM_STARS, cat->fb->fid) != DIM_STARS) {
		fflush(stdout);
		fprintf(stderr, "Failed to write catalog data. No, I don't know what file it is you insensitive clod!: %s\n",
		        strerror(errno));
		return -1;
	}
	cat->numstars++;
	return 0;
}

int catalog_write_mags(catalog* cat) {
	fitsbin_chunk_t* chunk;

	if (fits_pad_file(cat->fb->fid)) {
		fflush(stdout);
		fprintf(stderr, "Failed to pad catalog FITS file.\n");
		return -1;
	}

    chunk = cat->fb->chunks + 1;
    chunk->nrows = cat->numstars;

    if (fitsbin_write_chunk_header(cat->fb, 1)) {
        fprintf(stderr, "Failed to write magnitudes header.\n");
        return -1;
    }

	if (fwrite(cat->mags, sizeof(float), cat->numstars, cat->fb->fid) !=
		cat->numstars) {
		fflush(stdout);
		fprintf(stderr, "Failed to write catalog magnitudes: %s.\n",
		        strerror(errno));
		return -1;
	}

	if (fits_pad_file(cat->fb->fid)) {
		fflush(stdout);
		fprintf(stderr, "Failed to pad catalog FITS file.\n");
		return -1;
	}
	return 0;
}

int catalog_close(catalog* cat) {
    int rtn;
	if (!cat) return 0;
	rtn = fitsbin_close(cat->fb);
	free(cat);
    return rtn;
}

