/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2008 Dustin Lang, Keir Mierle and Sam Roweis.

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
#include <stdlib.h>
#include <assert.h>

#include "starkd.h"
#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "starutil.h"
#include "fitsbin.h"
#include "errors.h"
#include "tic.h"
#include "log.h"
#include "ioutils.h"
#include "fitsioutils.h"

static startree_t* startree_alloc() {
	startree_t* s = calloc(1, sizeof(startree_t));
	if (!s) {
		fprintf(stderr, "Failed to allocate a star kdtree struct.\n");
		return NULL;
	}
	return s;
}

void startree_search_for(const startree_t* s, const double* xyzcenter, double radius2,
						 double** xyzresults, double** radecresults,
						 int** starinds, int* nresults) {
    kdtree_qres_t* res = NULL;
	int opts;
    double* xyz;
    int i, N;

	opts = KD_OPTIONS_SMALL_RADIUS;
	if (xyzresults || radecresults)
		opts |= KD_OPTIONS_RETURN_POINTS;

	res = kdtree_rangesearch_options(s->tree, xyzcenter, radius2, opts);
	
    if (!res || !res->nres) {
        if (xyzresults)
            *xyzresults = NULL;
        if (radecresults)
            *radecresults = NULL;
        if (starinds)
			*starinds = NULL;
        *nresults = 0;
        return;
    }

    xyz = res->results.d;
    N = res->nres;
    *nresults = N;

    if (radecresults) {
        *radecresults = malloc(N * 2 * sizeof(double));
        for (i=0; i<N; i++)
            xyzarr2radecdegarr(xyz + i*3, (*radecresults) + i*2);
    }
    if (xyzresults) {
        // Steal the results array.
        *xyzresults = xyz;
        res->results.d = NULL;
    }
	if (starinds) {
		*starinds = malloc(res->nres * sizeof(int));
		for (i=0; i<N; i++)
			(*starinds)[i] = res->inds[i];
    }
    kdtree_free_query(res);
}


void startree_search(const startree_t* s, const double* xyzcenter, double radius2,
                     double** xyzresults, double** radecresults, int* nresults) {
	startree_search_for(s, xyzcenter, radius2, xyzresults, radecresults, NULL, nresults);
}

int startree_N(const startree_t* s) {
	return s->tree->ndata;
}

int startree_nodes(const startree_t* s) {
	return s->tree->nnodes;
}

int startree_D(const startree_t* s) {
	return s->tree->ndim;
}

qfits_header* startree_header(const startree_t* s) {
	return s->header;
}

bl* get_chunks(startree_t* s, il* wordsizes) {
    bl* chunks = bl_new(4, sizeof(fitsbin_chunk_t));
    fitsbin_chunk_t chunk;
    kdtree_t* kd = s->tree;

    fitsbin_chunk_init(&chunk);
    chunk.tablename = "sweep";
    chunk.itemsize = sizeof(uint8_t);
    chunk.nrows = kd->ndata;
    chunk.data = s->sweep;
    chunk.userdata = &(s->sweep);
    chunk.required = FALSE;
    bl_append(chunks, &chunk);
    if (wordsizes)
        il_append(wordsizes, sizeof(uint8_t));

    fitsbin_chunk_reset(&chunk);
    chunk.tablename = "sigma_radec";
    chunk.itemsize = 2 * sizeof(float);
    chunk.nrows = kd->ndata;
    chunk.data = s->sigma_radec;
    chunk.userdata = &(s->sigma_radec);
    chunk.required = FALSE;
    bl_append(chunks, &chunk);
    if (wordsizes)
        il_append(wordsizes, sizeof(float));

    fitsbin_chunk_reset(&chunk);
    chunk.tablename = "proper_motion";
    chunk.itemsize = 2 * sizeof(float);
    chunk.nrows = kd->ndata;
    chunk.data = s->proper_motion;
    chunk.userdata = &(s->proper_motion);
    chunk.required = FALSE;
    bl_append(chunks, &chunk);
    if (wordsizes)
        il_append(wordsizes, sizeof(float));

    fitsbin_chunk_reset(&chunk);
    chunk.tablename = "sigma_pm";
    chunk.itemsize = 2 * sizeof(float);
    chunk.nrows = kd->ndata;
    chunk.data = s->sigma_pm;
    chunk.userdata = &(s->sigma_pm);
    chunk.required = FALSE;
    bl_append(chunks, &chunk);
    if (wordsizes)
        il_append(wordsizes, sizeof(float));

    fitsbin_chunk_reset(&chunk);
    chunk.tablename = "mag";
    chunk.itemsize = sizeof(float);
    chunk.nrows = kd->ndata;
    chunk.data = s->mag;
    chunk.userdata = &(s->mag);
    chunk.required = FALSE;
    bl_append(chunks, &chunk);
    if (wordsizes)
        il_append(wordsizes, sizeof(float));

    fitsbin_chunk_reset(&chunk);
    chunk.tablename = "mag_err";
    chunk.itemsize = sizeof(float);
    chunk.nrows = kd->ndata;
    chunk.data = s->mag_err;
    chunk.userdata = &(s->mag_err);
    chunk.required = FALSE;
    bl_append(chunks, &chunk);
    if (wordsizes)
        il_append(wordsizes, sizeof(float));

    fitsbin_chunk_reset(&chunk);
    chunk.tablename = "starid";
    chunk.itemsize = sizeof(uint64_t);
    chunk.nrows = kd->ndata;
    chunk.data = s->starids;
    chunk.userdata = &(s->starids);
    chunk.required = FALSE;
    bl_append(chunks, &chunk);
    if (wordsizes)
        il_append(wordsizes, sizeof(uint64_t));

    fitsbin_chunk_clean(&chunk);
    return chunks;
}

startree_t* startree_open(const char* fn) {
    struct timeval tv1, tv2;
	startree_t* s;
    bl* chunks;
    int i;
    kdtree_fits_t* io;
    char* treename = STARTREE_NAME;

	s = startree_alloc();
	if (!s)
		return s;

    gettimeofday(&tv1, NULL);
    io = kdtree_fits_open(fn);
    gettimeofday(&tv2, NULL);
    debug("kdtree_fits_open() took %g ms\n", millis_between(&tv1, &tv2));
	if (!io) {
        ERROR("Failed to open FITS file \"%s\"", fn);
        goto bailout;
    }

    gettimeofday(&tv1, NULL);
    if (!kdtree_fits_contains_tree(io, treename))
        treename = NULL;
    gettimeofday(&tv2, NULL);
    debug("kdtree_fits_contains_tree() took %g ms\n", millis_between(&tv1, &tv2));

    gettimeofday(&tv1, NULL);
    s->tree = kdtree_fits_read_tree(io, treename, &s->header);
    gettimeofday(&tv2, NULL);
    debug("kdtree_fits_read_tree() took %g ms\n", millis_between(&tv1, &tv2));
    if (!s->tree) {
        ERROR("Failed to read kdtree from file \"%s\"", fn);
        goto bailout;
    }

    gettimeofday(&tv1, NULL);
    chunks = get_chunks(s, NULL);
    for (i=0; i<bl_size(chunks); i++) {
        fitsbin_chunk_t* chunk = bl_access(chunks, i);
        void** dest = chunk->userdata;
        kdtree_fits_read_chunk(io, chunk);
        *dest = chunk->data;
    }
    bl_free(chunks);
    gettimeofday(&tv2, NULL);
    debug("reading chunks took %g ms\n", millis_between(&tv1, &tv2));

	return s;

 bailout:
    kdtree_fits_io_close(io);
    startree_close(s);
	return NULL;
}

uint64_t startree_get_starid(const startree_t* s, int ind) {
    if (!s->starids)
        return 0;
    return s->starids[ind];
}

int startree_close(startree_t* s) {
	if (!s) return 0;
	if (s->inverse_perm)
		free(s->inverse_perm);
 	if (s->header)
		qfits_header_destroy(s->header);
    if (s->tree) {
        if (s->writing)
            kdtree_free(s->tree);
        else
            kdtree_fits_close(s->tree);
    }
	if (s->tagalong)
		fitstable_close(s->tagalong);
	free(s);
	return 0;
}

fitstable_t* startree_get_tagalong(startree_t* s) {
	char* fn;
	int next;
	int i;
	int ext = -1;
	if (s->tagalong)
		return s->tagalong;
	if (!s->tree->io)
		return NULL;
	fn = fitsbin_get_filename(s->tree->io);
	if (!fn) {
		ERROR("No filename");
		return NULL;
	}
	s->tagalong = fitstable_open(fn);
	if (!s->tagalong) {
		ERROR("Failed to open FITS table from %s", fn);
		return NULL;
	}
	next = qfits_query_n_ext(fn);
	for (i=1; i<=next; i++) {
		char* type;
		bool eq;
		qfits_header* hdr;
		hdr = qfits_header_readext(fn, i);
		if (!hdr) {
			ERROR("Failed to read FITS header for ext %i in %s", i, fn);
			return NULL;
		}
		type = fits_get_dupstring(hdr, "AN_FILE");
		eq = streq(type, AN_FILETYPE_TAGALONG);
		free(type);
		qfits_header_destroy(hdr);
		if (!eq)
			continue;
		ext = i;
		break;
	}
	if (ext == -1) {
		ERROR("Failed to find a FITS header with the card AN_FILE = TAGALONG");
		return NULL;
	}
	fitstable_open_extension(s->tagalong, ext);
	return s->tagalong;
}

static int Ndata(const startree_t* s) {
	return s->tree->ndata;
}

int startree_check_inverse_perm(startree_t* s) {
	// ensure that each value appears exactly once.
	int i, N;
	uint8_t* counts;
	N = Ndata(s);
	counts = calloc(Ndata(s), sizeof(uint8_t));
	for (i=0; i<Ndata(s); i++) {
		assert(s->inverse_perm[i] >= 0);
		assert(s->inverse_perm[i] < N);
		counts[s->inverse_perm[i]]++;
	}
	for (i=0; i<Ndata(s); i++) {
		assert(counts[i] == 1);
	}
	return 0;
}

void startree_compute_inverse_perm(startree_t* s) {
	// compute inverse permutation vector.
	s->inverse_perm = malloc(Ndata(s) * sizeof(int));
	if (!s->inverse_perm) {
		fprintf(stderr, "Failed to allocate star kdtree inverse permutation vector.\n");
		return;
	}
#ifndef NDEBUG
	{
		int i;
		for (i=0; i<Ndata(s); i++)
			s->inverse_perm[i] = -1;
	}
#endif
	kdtree_inverse_permutation(s->tree, s->inverse_perm);
#ifndef NDEBUG
	{
		int i;
		for (i=0; i<Ndata(s); i++)
			assert(s->inverse_perm[i] != -1);
	}
#endif
}

int startree_get_cut_nside(const startree_t* s) {
	return qfits_header_getint(s->header, "CUTNSIDE", -1);
}

int startree_get_cut_nsweeps(const startree_t* s) {
	return qfits_header_getint(s->header, "CUTNSWEP", -1);
}

double startree_get_cut_dedup(const startree_t* s) {
	return qfits_header_getdouble(s->header, "CUTDEDUP", 0.0);
}

char* startree_get_cut_band(const startree_t* s) {
	static char* bands[] = { "R", "B", "J" };
	int i;
	char* str = fits_get_dupstring(s->header, "CUTBAND");
	char* rtn = NULL;
	if (!str)
		return NULL;
	for (i=0; i<sizeof(bands) / sizeof(char*); i++) {
		if (streq(str, bands[i])) {
			rtn = bands[i];
			break;
		}
	}
	free(str);
	return rtn;
}

int startree_get_cut_margin(const startree_t* s) {
	return qfits_header_getint(s->header, "CUTMARG", -1);
}

double startree_get_jitter(const startree_t* s) {
	return qfits_header_getdouble(s->header, "JITTER", 0.0);
}


/*
 int startree_get_sweep(const startree_t* s, int ind) {
 if (ind < 0 || ind >= Ndata(s) || !s->sweep)
 return -1;
 return s->sweep[ind];
 }
 */

int startree_get(startree_t* s, int starid, double* posn) {
	if (s->tree->perm && !s->inverse_perm) {
		startree_compute_inverse_perm(s);
		if (!s->inverse_perm)
			return -1;
	}
	if (starid >= Ndata(s)) {
		fprintf(stderr, "Invalid star ID: %u >= %u.\n", starid, Ndata(s));
                assert(0);
		return -1;
	}
	if (s->inverse_perm) {
		kdtree_copy_data_double(s->tree, s->inverse_perm[starid], 1, posn);
	} else {
		kdtree_copy_data_double(s->tree, starid, 1, posn);
	}
	return 0;
}

int startree_get_radec(startree_t* s, int starid, double* ra, double* dec) {
	double xyz[3];
	int rtn;
	rtn = startree_get(s, starid, xyz);
	if (rtn)
		return rtn;
	xyzarr2radecdeg(xyz, ra, dec);
	return rtn;
}

startree_t* startree_new() {
	startree_t* s = startree_alloc();
	s->header = qfits_header_default();
	if (!s->header) {
		fprintf(stderr, "Failed to create a qfits header for star kdtree.\n");
		free(s);
		return NULL;
	}
	qfits_header_add(s->header, "AN_FILE", AN_FILETYPE_STARTREE, "This file is a star kdtree.", NULL);
    s->writing = TRUE;
	return s;
}

static int write_to_file(startree_t* s, const char* fn, bool flipped,
						 FILE* fid) {
    bl* chunks;
    il* wordsizes = NULL;
    int i;
    kdtree_fits_t* io = NULL;

	// just haven't bothered...
	assert(!(flipped && fid));

	if (fn) {
		io = kdtree_fits_open_for_writing(fn);
		if (!io) {
			ERROR("Failed to open file \"%s\" for writing kdtree", fn);
			return -1;
		}
	}
    if (flipped) {
        if (kdtree_fits_write_tree_flipped(io, s->tree, s->header)) {
            ERROR("Failed to write (flipped) kdtree to file \"%s\"", fn);
            return -1;
        }
    } else {
		if (fid) {
			if (kdtree_fits_append_tree_to(s->tree, s->header, fid)) {
				ERROR("Failed to write star kdtree");
				return -1;
			}
		} else {
			if (kdtree_fits_write_tree(io, s->tree, s->header)) {
				ERROR("Failed to write kdtree to file \"%s\"", fn);
				return -1;
			}
		}
    }

    if (flipped)
        wordsizes = il_new(4);

    chunks = get_chunks(s, wordsizes);
    for (i=0; i<bl_size(chunks); i++) {
        fitsbin_chunk_t* chunk = bl_access(chunks, i);
        if (!chunk->data)
            continue;
        if (flipped)
            kdtree_fits_write_chunk_flipped(io, chunk, il_get(wordsizes, i));
        else {
			if (fid) {
				kdtree_fits_write_chunk_to(chunk, fid);
			} else {
				kdtree_fits_write_chunk(io, chunk);
			}
		}
		fitsbin_chunk_clean(chunk);
	}
	bl_free(chunks);

    if (flipped)
        il_free(wordsizes);
    
	if (io)
		kdtree_fits_io_close(io);
    return 0;
}

int startree_write_to_file(startree_t* s, const char* fn) {
    return write_to_file(s, fn, FALSE, NULL);
}

int startree_write_to_file_flipped(startree_t* s, const char* fn) {
    return write_to_file(s, fn, TRUE, NULL);
}

int startree_append_to(startree_t* s, FILE* fid) {
	return write_to_file(s, NULL, FALSE, fid);
}

