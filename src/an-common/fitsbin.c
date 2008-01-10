/*
  This file is part of the Astrometry.net suite.
  Copyright 2007-2008 Dustin Lang.

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

#include <stdarg.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "keywords.h"
#include "fitsbin.h"
#include "fitsioutils.h"
#include "ioutils.h"

static void
ATTRIB_FORMAT(printf,2,3)
seterr(char** errstr, const char* format, ...) {
	va_list va;
	if (!errstr) return;
	va_start(va, format);
	vasprintf(errstr, format, va);
	va_end(va);
}

int fitsbin_close(fitsbin_t* fb) {
    int rtn = 0;
	if (!fb) return rtn;
	if (fb->map) {
		if (munmap(fb->map, fb->mapsize)) {
			fprintf(stderr, "Failed to munmap fitsbin: %s\n", strerror(errno));
            rtn = -1;
		}
	}
	if (fb->fid) {
		fits_pad_file(fb->fid);
		if (fclose(fb->fid)) {
			fprintf(stderr, "Error closing fitsbin: %s\n", strerror(errno));
            rtn = -1;
		}
	}
	free(fb->filename);
	free(fb->tablename);
	qfits_header_destroy(fb->primheader);
	qfits_header_destroy(fb->header);
	free(fb);
    return rtn;
}

static fitsbin_t* new_fitsbin() {
	fitsbin_t* fb;
	fb = calloc(1, sizeof(fitsbin_t));
	if (!fb)
		return NULL;
	return fb;
}

int fitsbin_write_primary_header(fitsbin_t* fb) {
	if (qfits_header_dump(fb->primheader, fb->fid))
		return -1;
	fb->primheader_end = ftello(fb->fid);
	return 0;
}

int fitsbin_write_header(fitsbin_t* fb) {
    int ncols = 1;
    int tablesize;
    qfits_table* table;
    qfits_header* hdr;
	int i;

	fb->header_start = ftello(fb->fid);

	// the table header
	tablesize = fb->itemsize * fb->nrows * ncols;
	table = qfits_table_new(fb->filename, QFITS_BINTABLE, tablesize, ncols, fb->nrows);
	assert(table);
    qfits_col_fill(table->col, fb->itemsize, 0, 1, TFITS_BIN_TYPE_A,
				   fb->tablename, "", "", "", 0, 0, 0, 0, 0);
    hdr = qfits_table_ext_header_default(table);
    qfits_table_close(table);

	// Copy headers from "fb->header" to "hdr".
	// Skip first and last ("SIMPLE" and "END") headers.
	for (i=1; i<(fb->header->n - 1); i++) {
		char key[FITS_LINESZ+1];
		char val[FITS_LINESZ+1];
		char com[FITS_LINESZ+1];
		char lin[FITS_LINESZ+1];
		qfits_header_getitem(fb->header, i, key, val, com, lin);
		qfits_header_add(hdr, key, val, com, lin);
	}

    if (qfits_header_dump(hdr, fb->fid))
		return -1;
	fb->header_end = ftello(fb->fid);
	qfits_header_destroy(hdr);
	return 0;
}

int fitsbin_fix_header(fitsbin_t* fb) {
	off_t offset;
	off_t new_header_end;
	off_t old_header_end;

	offset = ftello(fb->fid);
	fseeko(fb->fid, fb->header_start, SEEK_SET);
    old_header_end = fb->header_end;

    if (fitsbin_write_header(fb)) {
        return -1;
    }
	new_header_end = fb->header_end;

	if (new_header_end != old_header_end) {
		fprintf(stderr, "Error: header used to end at %lu, "
				"now it ends at %lu.  Data loss is likely!\n",
                (unsigned long)old_header_end, (unsigned long)new_header_end);
		return -1;
	}

	fseek(fb->fid, offset, SEEK_SET);
	return 0;
}

int fitsbin_fix_primary_header(fitsbin_t* fb) {
	off_t offset;
	off_t new_header_end;
	off_t old_header_end;

	offset = ftello(fb->fid);
	fseeko(fb->fid, 0, SEEK_SET);
    old_header_end = fb->primheader_end;

    if (fitsbin_write_primary_header(fb))
        return -1;
	new_header_end = fb->primheader_end;

	if (new_header_end != old_header_end) {
		fprintf(stderr, "Error: header used to end at %lu, "
				"now it ends at %lu.  Data loss is likely!\n",
                (unsigned long)old_header_end, (unsigned long)new_header_end);
		return -1;
	}
	fseek(fb->fid, offset, SEEK_SET);
	return 0;
}

fitsbin_t* fitsbin_open_for_writing(const char* fn, const char* tablename,
									char** errstr) {
	fitsbin_t* fb;

	fb = new_fitsbin();
	if (!fb)
		goto bailout;
	fb->fid = fopen(fn, "wb");
	if (!fb->fid) {
		fprintf(stderr, "Couldn't open file \"%s\" for output: %s\n", fn, strerror(errno));
		goto bailout;
	}

	fb->filename = strdup(fn);
	fb->tablename = strdup(tablename);

    // the primary header
    fb->primheader = qfits_table_prim_header_default();
	fb->header = qfits_header_default();
	return fb;

 bailout:
	if (fb) {
		if (fb->fid)
			fclose(fb->fid);
		free(fb);
	}
	return NULL;
}

fitsbin_t* fitsbin_open(const char* fn, const char* tablename,
						char** errstr, 
						int (*callback_read_header)(qfits_header* primheader, qfits_header* header, size_t* expected, char** errstr, void* userdata),
						void* userdata) {
	FILE* fid = NULL;
	qfits_header* primheader = NULL;
	qfits_header* header = NULL;
    fitsbin_t* fb = NULL;
    int tabstart, tabsize, ext;
    size_t expected = 0;
	int mode, flags;
	off_t mapstart;
	size_t mapsize;
	int mapoffset;
	char* map;

	if (!qfits_is_fits(fn)) {
		seterr(errstr, "File \"%s\" is not FITS format.", fn);
        goto bailout;
	}

	fid = fopen(fn, "rb");
	if (!fid) {
		seterr(errstr, "Failed to open file \"%s\": %s.", fn, strerror(errno));
        goto bailout;
	}

	primheader = qfits_header_read(fn);
	if (!primheader) {
		seterr(errstr, "Couldn't read FITS header from file \"%s\".", fn);
		goto bailout;
	}

    if (fits_find_table_column(fn, tablename, &tabstart, &tabsize, &ext)) {
        seterr(errstr, "Couldn't find table \"%s\" in file \"%s\".", tablename, fn);
        goto bailout;
    }

	header = qfits_header_readext(fn, ext);
	if (!header) {
		seterr(errstr, "Couldn't read FITS header from file \"%s\" extension %i.", fn, ext);
		goto bailout;
	}

	if (callback_read_header &&
		callback_read_header(primheader, header, &expected, errstr, userdata)) {
		goto bailout;
	}

	if (expected && (fits_bytes_needed(expected) != tabsize)) {
		seterr(errstr, "Expected table size (%i => %i FITS blocks) is not equal to size of table \"%s\" (%i FITS blocks).",
			   (int)expected, fits_blocks_needed(expected), tablename, tabsize / FITS_BLOCK_SIZE);
        goto bailout;
    }

	mode = PROT_READ;
	flags = MAP_SHARED;

	get_mmap_size(tabstart, tabsize, &mapstart, &mapsize, &mapoffset);

    fb = new_fitsbin();
    if (!fb) {
		seterr(errstr, "Failed to allocate a \"fitsbin\" struct.");
        goto bailout;
	}

	map = mmap(0, mapsize, mode, flags, fileno(fid), mapstart);
	if (map == MAP_FAILED) {
		seterr(errstr, "Couldn't mmap file \"%s\": %s", fn, strerror(errno));
        goto bailout;
	}
	fclose(fid);
    fid = NULL;

	fb->filename = strdup(fn);
	fb->tablename = strdup(tablename);
	fb->primheader = primheader;
	fb->header = header;
	fb->map = map;
	fb->mapsize = mapsize;
	fb->data = map + mapoffset;

    return fb;

 bailout:
    if (fb)
        free(fb);
	if (header)
		qfits_header_destroy(header);
	if (primheader)
		qfits_header_destroy(primheader);
	if (fid)
        fclose(fid);
    return NULL;
}
