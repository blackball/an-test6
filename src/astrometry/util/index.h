/*
 This file is part of the Astrometry.net suite.
 Copyright 2007-2008 Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef _INDEX_H
#define _INDEX_H

#include "quadfile.h"
#include "starkd.h"
#include "codekd.h"
#include "an-bool.h"

/*
 * These routines handle loading of index files, which can consist of
 * several files (.skdt.fits , .ckdt.fits, .quad.fits), or a
 * single large file (.fits).
 */

#define DEFAULT_INDEX_JITTER 1.0  // arcsec

struct index_meta_s {
	char* indexname;

	// Unique id for this index.
	int indexid;
	int healpix;
    int hpnside;

	// Jitter in the index, in arcseconds.
	double index_jitter;

	// cut-an parameters:
	int cutnside;
	int cutnsweep;
	double cutdedup;
	char* cutband;
	int cutmargin;

	// Does the index have the CIRCLE header - (codes live in the circle, not the box)?
	bool circle;
	// Does the index have the CX <= DX property
	bool cx_less_than_dx;

	// Limits of the size of quads in the index, in arcseconds.
	double index_scale_upper;
	double index_scale_lower;

    int dimquads;
    int nstars;
    int nquads;
};
typedef struct index_meta_s index_meta_t;

// Returns TRUE if the given index contains quads of sizes that overlap the given
// range of quad sizes, [quadlo, quadhi], in arcseconds.
bool index_meta_overlaps_scale_range(index_meta_t* meta, double quadlo, double quadhi);

/**
 Reads index metadata from the given 'filename' into the given 'meta' struct.

 This is done by basically loading the index, grabbing the metadata,
 and closing the index; therefore it checks for structural consistency
 of the index file.  As a side-effect, this means it's slow.
 */
int index_get_meta(const char* filename, index_meta_t* meta);

/**
 Reads index metadata from the given 'filename' into the given 'meta' struct.

 This function just quickly reads the metadata items out of the FITS
 header, so should be faster than index_get_meta().
int index_get_meta_quickly(const char* filename, index_meta_t* meta);
 */


/**
 * A loaded index
 */
struct index_s {
	codetree* codekd;
	quadfile* quads;
	startree_t* starkd;

    index_meta_t meta;
};
typedef struct index_s index_t;

#define INDEX_ONLY_LOAD_METADATA 2
#define INDEX_ONLY_LOAD_SKDT     4

index_t* index_build_from(codetree* codekd, quadfile* quads, startree_t* starkd);

bool index_is_file_index(const char* filename);

char* index_get_quad_filename(const char* indexname);

char* index_get_qidx_filename(const char* indexname);

int index_get_quad_dim(const index_t* index);

int index_nquads(const index_t* index);

int index_nstars(const index_t* index);

/**
 * Load an index from disk
 *
 * Parameters:
 *
 *   indexname - the base name of the index files; for example, if the index is
 *               in files 'myindex.ckdt.fits' and 'myindex.skdt.fits', then
 *               the indexname is just 'myindex'
 *
 *   flags     - If INDEX_ONLY_LOAD_METADATA, then only metadata will be
 *               loaded.
 *
 * Returns:
 *
 *   A pointer to an index_t structure or NULL on error.
 *
 */
index_t* index_load(const char* indexname, int flags);

/**
 * Close an index and free associated data structures
 */
void index_close(index_t* index);


int index_get_missing_cut_params(int indexid, int* hpnside, int* nsweep,
								 double* dedup, int* margin, char** band);

#endif // _INDEX_H
