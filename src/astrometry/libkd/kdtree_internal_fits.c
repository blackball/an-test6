/*
  This file is part of libkd.
  Copyright 2006-2008 Dustin Lang and Keir Mierle.

  libkd is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, version 2.

  libkd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with libkd; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef KDTREE_NO_FITS

#include "kdtree_fits_io.h"
#include "kdtree.h"
#include "fitsioutils.h"
#include "ioutils.h"
#include "errors.h"

#define KDTREE_FITS_VERSION 1

static char* get_table_name(const char* treename, const char* tabname) {
    char* rtn;
    if (!treename) {
        return strdup_safe(tabname);
    }
    asprintf_safe(&rtn, "%s_%s", tabname, treename);
    return rtn;
}

int MANGLE(kdtree_read_fits)(kdtree_fits_t* io, kdtree_t* kd) {
    fitsbin_chunk_t chunk;

    memset(&chunk, 0, sizeof(fitsbin_chunk_t));

	// kd->nodes
    chunk.tablename = get_table_name(kd->name, KD_STR_NODES);
    chunk.itemsize = COMPAT_NODE_SIZE(kd);
    chunk.nrows = kd->nnodes;
    chunk.required = FALSE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
        kd->nodes = chunk.data;
    }
    free(chunk.tablename);

    // kd->lr
    chunk.tablename = get_table_name(kd->name, KD_STR_LR);
    chunk.itemsize = sizeof(u32);
    chunk.nrows = kd->nbottom;
    chunk.required = FALSE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
        kd->lr = chunk.data;
    }
    free(chunk.tablename);

	// kd->perm
    chunk.tablename = get_table_name(kd->name, KD_STR_PERM);
    chunk.itemsize = sizeof(u32);
    chunk.nrows = kd->ndata;
    chunk.required = FALSE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
        kd->perm = chunk.data;
    }
    free(chunk.tablename);

	// kd->bb
    chunk.tablename = get_table_name(kd->name, KD_STR_BB);
    chunk.itemsize = sizeof(ttype) * kd->ndim * 2;
    chunk.nrows = 0;
    chunk.required = FALSE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
        int nbb_old = (kd->nnodes + 1) / 2 - 1;
        int nbb_new = kd->nnodes;

        // accept (but warn about) old-school buggy BB extension.
        if (chunk.nrows == nbb_new) {
        } else if (chunk.nrows == nbb_old) {
            ERROR("Warning: this file contains an old, buggy, %s "
                  "extension; it has %i rather than %i items.  Proceeding "
                  "anyway, but this is probably going to cause problems!",
                  chunk.tablename, nbb_old, nbb_new);
        } else {
            // error?
            ERROR("Bounding-box table %s should contain either %i (new) or "
                  "%i (old) bounding-boxes, but it has %i.",
                  chunk.tablename, nbb_new, nbb_old, chunk.nrows);
            free(chunk.tablename);
            return -1;
        }
        kd->bb.any = chunk.data;
        kd->n_bb = chunk.nrows;
    }
    free(chunk.tablename);

	// kd->split
    chunk.tablename = get_table_name(kd->name, KD_STR_SPLIT);
    chunk.itemsize = sizeof(ttype);
    chunk.nrows = kd->ninterior;
    chunk.required = FALSE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
		kd->split.any = chunk.data;
    }
    free(chunk.tablename);

	// kd->splitdim
    chunk.tablename = get_table_name(kd->name, KD_STR_SPLITDIM);
    chunk.itemsize = sizeof(u8);
    chunk.nrows = kd->ninterior;
    chunk.required = FALSE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
		kd->splitdim = chunk.data;
    }
    free(chunk.tablename);

	// kd->data
    chunk.tablename = get_table_name(kd->name, KD_STR_DATA);
    chunk.itemsize = sizeof(dtype) * kd->ndim;
    chunk.nrows = kd->ndata;
    chunk.required = TRUE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
		kd->data.any = chunk.data;
    }
    free(chunk.tablename);

	// kd->minval/kd->maxval/kd->scale
    chunk.tablename = get_table_name(kd->name, KD_STR_RANGE);
    chunk.itemsize = sizeof(double);
    chunk.nrows = (kd->ndim * 2 + 1);
    chunk.required = FALSE;
    if (kdtree_fits_read_chunk(io, &chunk) == 0) {
        double* r;
		r = chunk.data;
		kd->minval = r;
		kd->maxval = r + kd->ndim;
		kd->scale  = r[kd->ndim * 2];
		kd->invscale = 1.0 / kd->scale;
    }
    free(chunk.tablename);

    if (!(kd->bb.any || kd->nodes ||
          (kd->split.any && (TTYPE_INTEGER || kd->splitdim)))) {
		ERROR("kdtree contains neither traditional nodes, bounding boxes nor split+dim data");
        return -1;
    }

	if ((TTYPE_INTEGER && !ETYPE_INTEGER) &&
		!(kd->minval && kd->maxval)) {
		ERROR("treee does not contain required range information");
        return -1;
	}

	if (kd->split.any) {
        if (kd->splitdim)
            kd->splitmask = UINT32_MAX;
        else
            compute_splitbits(kd);
	}

    return 0;
}

#define WRITE_CHUNK() \
do { \
  if (fitsbin_write_chunk(fb, &chunk)) { \
    ERROR("Failed to write kdtree chunk"); \
    fitsbin_chunk_clean(&chunk); \
    return -1; \
  } \
} while (0)

int MANGLE(kdtree_write_fits)(kdtree_fits_t* io, const kdtree_t* kd, 
                              const qfits_header* inhdr) {
    fitsbin_chunk_t chunk;
    fitsbin_t* fb = kdtree_fits_get_fitsbin(io);
    qfits_header* hdr;

    fitsbin_chunk_init(&chunk);

    // kdtree header is an empty fitsbin_chunk.
    chunk.tablename = "kdtree_header";
    hdr = fitsbin_get_chunk_header(fb, &chunk);
    if (inhdr)
        fits_copy_all_headers(inhdr, hdr, NULL);
    fits_add_endian(hdr);
    fits_header_addf   (hdr, "KDT_NAME", "kdtree: name of this tree", "'%s'", kd->name ? kd->name : "");
    fits_header_add_int(hdr, "KDT_NDAT", kd->ndata,  "kdtree: number of data points");
    fits_header_add_int(hdr, "KDT_NDIM", kd->ndim,   "kdtree: number of dimensions");
    fits_header_add_int(hdr, "KDT_NNOD", kd->nnodes, "kdtree: number of nodes");
    fits_header_add_int(hdr, "KDT_VER",  KDTREE_FITS_VERSION, "kdtree: version number");
    qfits_header_add(hdr, "KDT_EXT",  (char*)kdtree_kdtype_to_string(kdtree_exttype(kd)),  "kdtree: external type", NULL);
    qfits_header_add(hdr, "KDT_INT",  (char*)kdtree_kdtype_to_string(kdtree_treetype(kd)), "kdtree: type of the tree's structures", NULL);
    qfits_header_add(hdr, "KDT_DATA", (char*)kdtree_kdtype_to_string(kdtree_datatype(kd)), "kdtree: type of the data", NULL);
    qfits_header_add(hdr, "KDT_LINL", (kd->has_linear_lr ? "T" : "F"), "kdtree: has_linear_lr", NULL);
    WRITE_CHUNK();
    fitsbin_chunk_reset(&chunk);

	if (kd->nodes) {
		chunk.tablename = get_table_name(kd->name, KD_STR_NODES);
		chunk.itemsize = COMPAT_NODE_SIZE(kd);
		chunk.nrows = kd->nnodes;
		chunk.data = kd->nodes;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		fits_add_long_comment
			(hdr, "The table containing column \"%s\" contains \"legacy\" "
			 "kdtree nodes (kdtree_node_t structs).  These nodes contain two "
			 "%u-byte, native-endian unsigned ints, followed by a bounding-box, "
			 "which is two points in NDIM (=%u) dimensional space, stored as "
			 "native-endian doubles (%u bytes each).  The whole struct has size %u.",
			 chunk.tablename, (unsigned int)sizeof(unsigned int), kd->ndim,
             (unsigned int)sizeof(double), chunk.itemsize);
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}
	if (kd->lr) {
        chunk.tablename = get_table_name(kd->name, KD_STR_LR);
        chunk.itemsize = sizeof(u32);
        chunk.nrows = kd->nbottom;
		chunk.data = kd->lr;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		fits_add_long_comment
			(hdr, "The \"%s\" table contains the kdtree \"LR\" array. "
			 "This array has one %u-byte, native-endian unsigned int for each "
			 "leaf node in the tree. For each node, it gives the index of the "
			 "rightmost data point owned by the node.",
			 chunk.tablename, chunk.itemsize);
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}
	if (kd->perm) {
        chunk.tablename = get_table_name(kd->name, KD_STR_PERM);
        chunk.itemsize = sizeof(u32);
        chunk.nrows = kd->ndata;
        chunk.data = kd->perm;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		fits_add_long_comment
			(hdr, "The \"%s\" table contains the kdtree permutation array. "
			 "This array contains one %u-byte, native-endian unsigned int for "
			 "each data point in the tree. For each data point, it gives the "
			 "index that the data point had in the original array on which the "
			 "kdtree was built.", chunk.tablename, chunk.itemsize);
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}
	if (kd->bb.any) {
        chunk.tablename = get_table_name(kd->name, KD_STR_BB);
        chunk.itemsize = sizeof(ttype) * kd->ndim * 2;
        chunk.nrows = kd->nnodes;
        chunk.data = kd->bb.any;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		fits_add_long_comment
			(hdr, "The \"%s\" table contains the kdtree bounding-box array. "
			 "This array contains two %u-dimensional points, stored as %u-byte, "
			 "native-endian %ss, for each node in the tree. Each data "
			 "point owned by a node is contained within its bounding box.",
			 chunk.tablename, (unsigned int)kd->ndim,
             (unsigned int)sizeof(ttype),
			 kdtree_kdtype_to_string(kdtree_treetype(kd)));
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}
	if (kd->split.any) {
        chunk.tablename = get_table_name(kd->name, KD_STR_SPLIT);
        chunk.itemsize = sizeof(ttype);
        chunk.nrows = kd->ninterior;
        chunk.data = kd->split.any;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		if (!kd->splitdim) {
			fits_add_long_comment
				(hdr, "The \"%s\" table contains the kdtree splitting-plane "
				 "boundaries, and also the splitting dimension, packed into "
				 "a %u-byte, native-endian %s, for each interior node in the tree. "
				 "The splitting dimension is packed into the low %u bit%s, and the "
				 "splitting location uses the remaining bits. "
				 "The left child of a node contains data points that lie on the "
				 "low side of the splitting plane, and the right child contains "
				 "data points on the high side of the plane.",
				 chunk.tablename, chunk.itemsize,
				 kdtree_kdtype_to_string(kdtree_treetype(kd)),
				 kd->dimbits, (kd->dimbits > 1 ? "s" : ""));
		} else {
			fits_add_long_comment
				(hdr, "The \"%s\" table contains the kdtree splitting-plane "
				 "boundaries as %u-byte, native-endian %s, for each interior node in the tree. "
				 "The dimension along which the splitting-plane splits is stored in "
				 "a separate array. "
				 "The left child of a node contains data points that lie on the "
				 "low side of the splitting plane, and the right child contains "
				 "data points on the high side of the plane.",
				 chunk.tablename, chunk.itemsize,
				 kdtree_kdtype_to_string(kdtree_treetype(kd)));
		}
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}
	if (kd->splitdim) {
        chunk.tablename = get_table_name(kd->name, KD_STR_SPLITDIM);
        chunk.itemsize = sizeof(u8);
        chunk.nrows = kd->ninterior;
        chunk.data = kd->splitdim;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		fits_add_long_comment
			(hdr, "The \"%s\" table contains the kdtree splitting-plane "
			 "dimensions as %u-byte unsigned ints, for each interior node in the tree. "
			 "The location of the splitting-plane along that dimension is stored "
			 "in a separate array. "
			 "The left child of a node contains data points that lie on the "
			 "low side of the splitting plane, and the right child contains "
			 "data points on the high side of the plane.",
			 chunk.tablename, chunk.itemsize);
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}
	if (kd->minval && kd->maxval) {
        double tempranges[kd->ndim * 2 + 1];
        int d;
		memcpy(tempranges, kd->minval, kd->ndim * sizeof(double));
		memcpy(tempranges + kd->ndim, kd->maxval, kd->ndim * sizeof(double));
		tempranges[kd->ndim*2] = kd->scale;

        chunk.tablename = get_table_name(kd->name, KD_STR_RANGE);
        chunk.itemsize = sizeof(double);
        chunk.nrows = (kd->ndim * 2 + 1);
        chunk.data = tempranges;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		fits_add_long_comment
			(hdr, "The \"%s\" table contains the scaling parameters of the "
			 "kdtree.  This tells how to convert from the format of the data "
			 "to the internal format of the tree (and vice versa). "
			 "It is stored as an array "
			 "of %u-byte, native-endian doubles.  The first %u elements are "
			 "the lower bound of the data, the next %u elements are the upper "
			 "bound, and the final element is the scale, which says how many "
			 "tree units there are per data unit.",
			 chunk.tablename, chunk.itemsize, (unsigned int)kd->ndim, (unsigned int)kd->ndim);
		fits_add_long_comment
			(hdr, "For reference, here are the ranges of the data.  Note that "
			 "this is not used by the libkd software, it's just for human readers.");
		for (d=0; d<kd->ndim; d++)
			fits_add_long_comment
				(hdr, "  dim %i: [%g, %g]", d, kd->minval[d], kd->maxval[d]);
		fits_add_long_comment(hdr, "scale: %g", kd->scale);
		fits_add_long_comment(hdr, "1/scale: %g", kd->invscale);
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}
	if (kd->data.any) {
        chunk.tablename = get_table_name(kd->name, KD_STR_DATA);
        chunk.itemsize = sizeof(dtype) * kd->ndim;
        chunk.nrows = kd->ndata;
        chunk.data = kd->data.any;
        hdr = fitsbin_get_chunk_header(fb, &chunk);
		fits_add_long_comment
			(hdr, "The \"%s\" table contains the kdtree data. "
			 "It is stored as %u-dimensional, %u-byte native-endian %ss.",
			 chunk.tablename, (unsigned int)kd->ndim, (unsigned int)sizeof(dtype),
			 kdtree_kdtype_to_string(kdtree_datatype(kd)));
        WRITE_CHUNK();
        fitsbin_chunk_reset(&chunk);
	}

    return 0;
}
#undef WRITE_CHUNK


#endif
