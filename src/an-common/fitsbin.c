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

void fitsbin_set_filename(fitsbin_t* fb, const char* fn) {
    free(fb->filename);
    fb->filename = strdup(fn);
}

fitsbin_t* fitsbin_new(int nchunks) {
	fitsbin_t* fb;
	fb = calloc(1, sizeof(fitsbin_t));
	if (!fb)
		return NULL;
    fb->chunks = calloc(nchunks, sizeof(fitsbin_chunk_t));
    fb->nchunks = nchunks;
	return fb;
}

static void free_chunk(fitsbin_chunk_t* chunk) {
	free(chunk->tablename);
    fitsfile_extension_close(&chunk->ext);
    /*
     if (chunk->ext.header)
     qfits_header_destroy(chunk->ext.header);
     */
	if (chunk->map) {
		if (munmap(chunk->map, chunk->mapsize)) {
			fprintf(stderr, "Failed to munmap fitsbin: %s\n", strerror(errno));
		}
	}
}

int fitsbin_close(fitsbin_t* fb) {
    int i;
    int rtn = 0;
	if (!fb) return rtn;
    if (fitsfile_close(fb->fitsfile)) {
        fprintf(stderr, "Error closing fitsbin: %s\n", strerror(errno));
        rtn = -1;
    }
    for (i=0; i<fb->nchunks; i++)
        free_chunk(fb->chunks + i);
    free(fb->chunks);
	free(fb);
    return rtn;
}

int fitsbin_write_primary_header(fitsbin_t* fb) {
    return fitsfile_write_primary_header(fb->fitsfile);
}

qfits_header* fitsbin_get_primary_header(fitsbin_t* fb) {
    return fitsfile_get_primary_header(fb->fitsfile);
}

int fitsbin_fix_primary_header(fitsbin_t* fb) {
    return fitsfile_fix_primary_header(fb->fitsfile);
}

int fitsbin_write_header(fitsbin_t* fb) {
    return fitsbin_write_chunk_header(fb, 0);
}

qfits_header* fitsbin_get_chunk_header(fitsbin_t* fb, int chunknum) {
    fitsbin_chunk_t* chunk;
    qfits_table* table;
    int tablesize;
    qfits_header* hdr;
    int ncols = 1;

    chunk = fb->chunks + chunknum;
	// the table header
	tablesize = chunk->itemsize * chunk->nrows * ncols;
	table = qfits_table_new(fb->filename, QFITS_BINTABLE, tablesize, ncols, chunk->nrows);
	assert(table);
    qfits_col_fill(table->col, chunk->itemsize, 0, 1, TFITS_BIN_TYPE_A,
				   chunk->tablename, "", "", "", 0, 0, 0, 0, 0);
    hdr = qfits_table_ext_header_default(table);
    qfits_table_close(table);
    fitsfile_extension_set_header(&(chunk->ext), hdr);
    return hdr;
}

int fitsbin_write_chunk_header(fitsbin_t* fb, int chunknum) {
    qfits_header* hdr;
    fitsbin_chunk_t* chunk;

    assert(chunknum < fb->nchunks);
    assert(chunknum >= 0);
    chunk = fb->chunks + chunknum;
    hdr = fitsbin_get_chunk_header(fb, chunknum);
    if (fitsfile_extension_write_header(&(chunk->ext), fb->fitsfile->fid)) {
        fprintf(stderr, "Failed to write fitsbin extension header %i in file %s.\n", chunknum, fitsfile_filename(fb->fitsfile));
        return -1;
    }

    /*
     // Copy headers from "chunk->header" to "hdr".
     // Skip first and last ("SIMPLE" and "END") headers.
     for (i=1; i<(chunk->ext.header->n - 1); i++) {
     char key[FITS_LINESZ+1];
     char val[FITS_LINESZ+1];
     char com[FITS_LINESZ+1];
     char lin[FITS_LINESZ+1];
     qfits_header_getitem(chunk->ext.header, i, key, val, com, lin);
     qfits_header_add(hdr, key, val, com, lin);
     }
     */
	//qfits_header_destroy(hdr);
	return 0;
}

int fitsbin_fix_header(fitsbin_t* fb) {
    return fitsbin_fix_chunk_header(fb, 0);
}

int fitsbin_fix_chunk_header(fitsbin_t* fb, int chunknum) {
    fitsbin_chunk_t* chunk;

    assert(chunknum < fb->nchunks);
    assert(chunknum >= 0);

    chunk = fb->chunks + chunknum;

    if (fitsfile_extension_fix_header(&(chunk->ext), fb->fitsfile->fid)) {
        fprintf(stderr, "Failed to fix extension %i of fitsbin file %s\n",
                chunknum, fb->fitsfile->filename);
        return -1;
    }
	return 0;
}

int fitsbin_write_items(fitsbin_t* fb, int chunk, void* data, int N) {
    if (fwrite(data, fb->chunks[chunk].itemsize, N, fb->fitsfile->fid) != N) {
        fprintf(stderr, "Failed to write %i items: %s\n", N, strerror(errno));
        return -1;
    }
    return 0;
}

int fitsbin_write_item(fitsbin_t* fb, int chunk, void* data) {
    return fitsbin_write_items(fb, chunk, data, 1);
}

fitsbin_t* fitsbin_open_for_writing(const char* fn, const char* tablename,
									char** errstr) {
	fitsbin_t* fb;
    fitsbin_chunk_t* chunk;

	fb = fitsbin_new(1);
	if (!fb)
        return NULL;

    /*
     if (fitsbin_start_write(fb)) {
     goto bailout;
     }
     */
    fb->fitsfile = fitsfile_open_for_writing(fn);
    if (!fb->fitsfile) {
        fprintf(stderr, "Failed to open fitsbin file %s for writing.\n", fn);
        free(fb);
        return NULL;
    }
	//chunk->ext.header = qfits_header_default();

    chunk = fb->chunks;
	chunk->tablename = strdup(tablename);

	return fb;
}

fitsbin_t* fitsbin_open(const char* fn, const char* tablename,
						char** errstr, 
						int (*callback_read_header)(qfits_header* primheader, qfits_header* header, size_t* expected, char** errstr, void* userdata),
						void* userdata) {
    fitsbin_t* fb;
    fitsbin_chunk_t* chunk;
    int rtn;

    fb = fitsbin_new(1);
    if (!fb)
        return fb;

    fb->fitsfile = fitsfile_open(fn);
    if (!fb->fitsfile) {
        free(fb);
        return NULL;
    }
    //fb->filename = strdup(fn);
    fb->errstr = errstr;

    chunk = fb->chunks;
    chunk->tablename = strdup(tablename);
    chunk->callback_read_header = callback_read_header;
    chunk->userdata = userdata;

    rtn = fitsbin_read(fb);
    if (rtn) {
        fitsbin_close(fb);
        return NULL;
    }
    if (errstr)
        *errstr = *(fb->errstr);

    return fb;
}

int fitsbin_read(fitsbin_t* fb) {
	FILE* fid = NULL;
    int tabstart, tabsize, ext;
    size_t expected = 0;
	int mode, flags;
	off_t mapstart;
	int mapoffset;
    char* fn;
    int i;
    char** errstr;

    fn = fb->fitsfile->filename;
    errstr = fb->errstr;

	if (!qfits_is_fits(fn)) {
        seterr(errstr, "File \"%s\" is not FITS format.", fn);
        goto bailout;
	}

	fid = fopen(fn, "rb");
	if (!fid) {
		seterr(errstr, "Failed to open file \"%s\": %s.", fn, strerror(errno));
        goto bailout;
	}

    /*
     fb->primheader = qfits_header_read(fn);
     if (!fb->primheader) {
     seterr(errstr, "Couldn't read FITS header from file \"%s\".", fn);
     goto bailout;
     }
     */

    for (i=0; i<fb->nchunks; i++) {
        fitsbin_chunk_t* chunk = fb->chunks + i;

        if (fits_find_table_column(fn, chunk->tablename, &tabstart, &tabsize, &ext)) {
            if (chunk->required) {
                seterr(errstr, "Couldn't find table \"%s\" in file \"%s\".", chunk->tablename, fn);
                goto bailout;
            } else {
                continue;
            }
        }

        chunk->ext.header = qfits_header_readext(fn, ext);
        if (!chunk->ext.header) {
            seterr(errstr, "Couldn't read FITS header from file \"%s\" extension %i.", fn, ext);
            goto bailout;
        }

        if (chunk->callback_read_header &&
            chunk->callback_read_header(fb->fitsfile->primary.header, chunk->ext.header, &expected, fb->errstr, chunk->userdata)) {
            goto bailout;
        }

        if (expected && (fits_bytes_needed(expected) != tabsize)) {
            seterr(errstr, "Expected table size (%i => %i FITS blocks) is not equal to size of table \"%s\" (%i FITS blocks).",
                   (int)expected, fits_blocks_needed(expected), chunk->tablename, tabsize / FITS_BLOCK_SIZE);
            goto bailout;
        }

        mode = PROT_READ;
        flags = MAP_SHARED;

        get_mmap_size(tabstart, tabsize, &mapstart, &(chunk->mapsize), &mapoffset);

        chunk->map = mmap(0, chunk->mapsize, mode, flags, fileno(fid), mapstart);
        if (chunk->map == MAP_FAILED) {
            seterr(errstr, "Couldn't mmap file \"%s\": %s", fn, strerror(errno));
            chunk->map = NULL;
            goto bailout;
        }
        chunk->data = chunk->map + mapoffset;
    }
    fclose(fid);
    fid = NULL;

    return 0;

 bailout:
    return -1;
}
