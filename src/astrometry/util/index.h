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

/**
 Info about an index, including part of the sky it covers, size of
 quads, etc.; plus the code kdtree, quad list, and star kdtree.

 Some of the functions below only read the metadata, leaving the
 "codekd", "quads", and "starkd" fields NULL.
 */
struct index_s {
	// The actual components of an index.
	codetree* codekd;
	quadfile* quads;
	startree_t* starkd;

	// Below here: metadata about the index.
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
	// Does the index have the CX + DX <= 1/2 property
	bool meanx_less_than_half;

	// Limits of the size of quads in the index, in arcseconds.
	double index_scale_upper;
	double index_scale_lower;

    int dimquads;
    int nstars;
    int nquads;
};
typedef struct index_s index_t;

/**
 Returns TRUE if the given index contains quads of sizes that overlap
 the given range of quad sizes, [quadlo, quadhi], in arcseconds.
 */
bool index_overlaps_scale_range(index_t* indx, double quadlo, double quadhi);

/**
 Returns TRUE if the given index covers a part of the sky that is
 within "radius_deg" degrees of the given "ra","dec" position (in
 degrees).
 */
bool index_is_within_range(index_t* indx, double ra, double dec, double radius_deg);

/**
 Reads index metadata from the given 'filename' into the given 'indx' struct.

 This is done by basically loading the index, grabbing the metadata,
 and closing the index; therefore it checks for structural consistency
 of the index file as well as getting the metadata; this also means
 it's slower than it could be...
 */
int index_get_meta(const char* filename, index_t* indx);

bool index_is_file_index(const char* filename);

char* index_get_quad_filename(const char* indexname);

char* index_get_qidx_filename(const char* indexname);

#define INDEX_ONLY_LOAD_METADATA 2
#define INDEX_ONLY_LOAD_SKDT     4

int index_get_quad_dim(const index_t* index);

int index_nquads(const index_t* index);

int index_nstars(const index_t* index);

index_t* index_build_from(codetree* codekd, quadfile* quads, startree_t* starkd);

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
 *   dest      - If NULL, a new index_t will be allocated and returned; otherwise,
 *               the results will be put in this index_t object.
 *
 * Returns:
 *
 *   A pointer to an index_t structure or NULL on error.
 *
 */
index_t* index_load(const char* indexname, int flags, index_t* dest);

/**
 Close an index and free associated data structures, *without freeing
 'index' itself*.
 */
void index_close(index_t* index);

/**
 Closes the given index and calls free(index).
 */
void index_free(index_t* index);

int index_get_missing_cut_params(int indexid, int* hpnside, int* nsweep,
								 double* dedup, int* margin, char** band);

#endif // _INDEX_H
