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
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>

#include "qfits.h"
#include "fitsioutils.h"
#include "quadfile.h"
#include "starutil.h"
#include "ioutils.h"

static quadfile* new_quadfile() {
	quadfile* qf = calloc(1, sizeof(quadfile));
	if (!qf) {
		fprintf(stderr, "Couldn't malloc a quadfile struct: %s\n", strerror(errno));
		return NULL;
	}
	qf->healpix = -1;
	return qf;
}

int quadfile_dimquads(const quadfile* qf) {
    return qf->dimquads;
}

qfits_header* quadfile_get_header(const quadfile* qf) {
	return qf->fb->primheader;
}

static int callback_read_header(qfits_header* primheader, qfits_header* header,
								size_t* expected, char** errstr,
								void* userdata) {
	quadfile* qf = userdata;

    qf->dimquads = qfits_header_getint(primheader, "DIMQUADS", 4);
    qf->numquads = qfits_header_getint(primheader, "NQUADS", -1);
    qf->numstars = qfits_header_getint(primheader, "NSTARS", -1);
    qf->index_scale_upper = qfits_header_getdouble(primheader, "SCALE_U", -1.0);
    qf->index_scale_lower = qfits_header_getdouble(primheader, "SCALE_L", -1.0);
	qf->indexid = qfits_header_getint(primheader, "INDEXID", 0);
	qf->healpix = qfits_header_getint(primheader, "HEALPIX", -1);
	//qf->header = header;

	if ((qf->numquads == -1) || (qf->numstars == -1) ||
		(qf->index_scale_upper == -1.0) || (qf->index_scale_lower == -1.0)) {
		if (errstr) *errstr = "Couldn't find NQUADS or NSTARS or SCALE_U or SCALE_L entries in FITS header.";
		return -1;
	}

    if (fits_check_endian(primheader)) {
		if (errstr) *errstr = "File was written with the wrong endianness.";
		return -1;
    }

    *expected = qf->numquads * qf->dimquads * sizeof(uint32_t);
	return 0;
}

quadfile* quadfile_open(const char* fn) {
    quadfile* qf = NULL;
	fitsbin_t* fb = NULL;
	char* errstr = NULL;

    qf = new_quadfile();
    if (!qf)
        goto bailout;

	fb = fitsbin_open(fn, "quads", &errstr, callback_read_header, qf);
	if (!fb) {
		fprintf(stderr, "%s\n", errstr);
		goto bailout;
	}

	qf->fb = fb;
	qf->quadarray = (uint32_t*)(fb->data);

    return qf;

 bailout:
    if (qf)
        free(qf);
    return NULL;
}

int quadfile_close(quadfile* qf) {
    int rtn = 0;

	if (!qf) return 0;
	fitsbin_close(qf->fb);
	free(qf);
    return rtn;
}

quadfile* quadfile_open_for_writing(const char* fn) {
	quadfile* qf;
	fitsbin_t* fb = NULL;
	char* errstr = NULL;

	qf = new_quadfile();
	if (!qf)
		goto bailout;

	fb = fitsbin_open_for_writing(fn, "quads", &errstr);
	if (!fb) {
		fprintf(stderr, "%s\n", errstr);
		goto bailout;
	}
	qf->fb = fb;

    // default
    qf->dimquads = 4;

	// add default values to header
    fits_add_endian(fb->primheader);
	qfits_header_add(fb->primheader, "AN_FILE", "QUAD", "This file lists, for each quad, its stars.", NULL);
	qfits_header_add(fb->primheader, "DIMQUADS", "0", "Number of stars in a quad.", NULL);
	qfits_header_add(fb->primheader, "NQUADS", "0", "Number of quads.", NULL);
	qfits_header_add(fb->primheader, "NSTARS", "0", "Number of stars used (or zero).", NULL);
	qfits_header_add(fb->primheader, "SCALE_U", "0.0", "Upper-bound index scale.", NULL);
	qfits_header_add(fb->primheader, "SCALE_L", "0.0", "Lower-bound index scale.", NULL);
	qfits_header_add(fb->primheader, "INDEXID", "0", "Index unique ID.", NULL);
	qfits_header_add(fb->primheader, "HEALPIX", "-1", "Healpix of this index.", NULL);
    fits_add_long_comment(fb->primheader, "The first extension contains the quads "
                          "stored as %i 32-bit native-endian unsigned ints.", qf->dimquads);
	return qf;

 bailout:
	if (qf)
		free(qf);
	return NULL;
}

int quadfile_write_header(quadfile* qf) {
	fitsbin_t* fb = qf->fb;
	fb->itemsize = qf->dimquads * sizeof(uint32_t);
	fb->nrows = qf->numquads;

	if (fitsbin_write_primary_header(fb) ||
		fitsbin_write_header(fb)) {
		fprintf(stderr, "Failed to write quadfile header.\n");
		return -1;
	}
	return 0;
}

int quadfile_write_quad(quadfile* qf, uint* stars) {
    int i;
    for (i=0; i<qf->dimquads; i++) {
        uint32_t star = stars[i];
        if (fwrite(&star, sizeof(uint32_t), 1, qf->fb->fid) != 1) {
            fprintf(stderr, "quadfile_fits_write_quad: failed to write: %s\n", strerror(errno));
            return -1;
        }
    }
	qf->numquads++;
	return 0;
}

int quadfile_fix_header(quadfile* qf) {
	fitsbin_t* fb = qf->fb;
	fb->itemsize = qf->dimquads * sizeof(uint32_t);
	fb->nrows = qf->numquads;

	// fill in the real values...
	fits_header_mod_int(fb->primheader, "DIMQUADS", qf->dimquads, "Number of stars in a quad.");
	fits_header_mod_int(fb->primheader, "NQUADS", qf->numquads, "Number of quads.");
	fits_header_mod_int(fb->primheader, "NSTARS", qf->numstars, "Number of stars.");
	fits_header_mod_double(fb->primheader, "SCALE_U", qf->index_scale_upper, "Upper-bound index scale (radians).");
	fits_header_mod_double(fb->primheader, "SCALE_L", qf->index_scale_lower, "Lower-bound index scale (radians).");
	fits_header_mod_int(fb->primheader, "INDEXID", qf->indexid, "Index unique ID.");
	fits_header_mod_int(fb->primheader, "HEALPIX", qf->healpix, "Healpix of this index.");

	if (fitsbin_fix_primary_header(fb) ||
		fitsbin_fix_header(fb)) {
        fprintf(stderr, "Failed to fix quad header.\n");
		return -1;
	}
	return 0;
}

double quadfile_get_index_scale_upper_arcsec(const quadfile* qf) {
    return rad2arcsec(qf->index_scale_upper);
}

double quadfile_get_index_scale_lower_arcsec(const quadfile* qf) {
	return rad2arcsec(qf->index_scale_lower);
}

int quadfile_get_stars(const quadfile* qf, uint quadid, uint* stars) {
    int i;
	if (quadid >= qf->numquads) {
		fprintf(stderr, "Requested quadid %i, but number of quads is %i.\n",
				quadid, qf->numquads);
        assert(0);
		return -1;
	}

    for (i=0; i<qf->dimquads; i++) {
        stars[i] = qf->quadarray[quadid * qf->dimquads + i];
    }
    return 0;
}
