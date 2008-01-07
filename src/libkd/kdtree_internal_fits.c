/*
  This file is part of libkd.
  Copyright 2006, 2007 Dustin Lang and Keir Mierle.

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
#include "fitsioutils.h"
#include "kdtree.h"

static void tablesize_kd(kdtree_t* kd, extra_table* ext) {
	if (!strcmp(ext->name, KD_STR_NODES)) {
		ext->datasize = COMPAT_NODE_SIZE(kd);
		ext->nitems = kd->nnodes;
	} else if (!strcmp(ext->name, KD_STR_LR)) {
		ext->nitems = kd->nbottom;
	} else if (!strcmp(ext->name, KD_STR_PERM)) {
		ext->nitems = kd->ndata;
    } else if (!strcmp(ext->name, KD_STR_BB)) {
        ext->datasize = sizeof(ttype) * kd->ndim * 2;
        // ext->nitems = kd->nnodes;
	} else if (!strcmp(ext->name, KD_STR_SPLIT)) {
		ext->nitems = kd->ninterior;
	} else if (!strcmp(ext->name, KD_STR_SPLITDIM)) {
		ext->nitems = kd->ninterior;
	} else if (!strcmp(ext->name, KD_STR_DATA)) {
		ext->datasize = sizeof(dtype) * kd->ndim;
		ext->nitems = kd->ndata;
	} else if (!strcmp(ext->name, KD_STR_RANGE)) {
		ext->nitems = (kd->ndim * 2 + 1);
	} else {
		fprintf(stderr, "tablesize_kd called with ext->name %s\n", ext->name);
	}
}

kdtree_t* MANGLE(kdtree_read_fits)(char* fn, qfits_header** p_hdr, unsigned int treetype, extra_table* userextras, int nuserextras) {
	extra_table* ext;
	int Ne;
	double* tempranges;
	kdtree_t* kdt;

	int inodes;
	int ilr;
	int iperm;
	int ibb;
	int isplit;
	int isplitdim;
	int idata;
	int irange;

	bool bbtree;

	extra_table extras[10 + nuserextras];

	memset(extras, 0, sizeof(extras));

	ext = extras;

	// kd->nodes
	ext->name = KD_STR_NODES;
	ext->compute_tablesize = tablesize_kd;
	inodes = ext - extras;
	ext++;

	// kd->lr
	ext->name = KD_STR_LR;
	ext->compute_tablesize = tablesize_kd;
	ext->datasize = sizeof(u32);
	ilr = ext - extras;
	ext++;

	// kd->perm
	ext->name = KD_STR_PERM;
	ext->compute_tablesize = tablesize_kd;
	ext->datasize = sizeof(u32);
	iperm = ext - extras;
	ext++;

	// kd->bb
    ext->name = KD_STR_BB;
    //ext->compute_tablesize = tablesize_kd;
    ext->compute_tablesize = NULL;
    ibb = ext - extras;
    ext++;

	// kd->split
	ext->name = KD_STR_SPLIT;
	ext->datasize = sizeof(ttype);
	ext->compute_tablesize = tablesize_kd;
	isplit = ext - extras;
	ext++;

	// kd->splitdim
	ext->name = KD_STR_SPLITDIM;
	ext->datasize = sizeof(u8);
	ext->compute_tablesize = tablesize_kd;
	isplitdim = ext - extras;
	ext++;

	// kd->data
	ext->name = KD_STR_DATA;
	ext->compute_tablesize = tablesize_kd;
	ext->required = TRUE;
	idata = ext - extras;
	ext++;

	// kd->minval/kd->maxval/kd->scale
	ext->name = KD_STR_RANGE;
	ext->datasize = sizeof(double);
	ext->compute_tablesize = tablesize_kd;
	irange = ext - extras;
	ext++;

	// user tables are processed after internal ones...
	if (nuserextras) {
		// copy in...
		memcpy(ext, userextras, nuserextras * sizeof(extra_table));
		ext += nuserextras;
	}

	Ne = ext - extras;
	kdt = kdtree_fits_common_read(fn, p_hdr, treetype, extras, Ne);
	if (!kdt) {
		fprintf(stderr, "Failed to read kdtree from file %s.\n", fn);
		return NULL;
	}

    // accept (but warn about) old-school buggy BB extension.
    if (extras[ibb].found) {
        int nitems_old = (kdt->nnodes + 1) / 2 - 1;
        int nitems_new = kdt->nnodes;
        if (!((extras[ibb].nitems == nitems_old) ||
              (extras[ibb].nitems == nitems_new))) {
            fprintf(stderr, "The %s table should contain either %i (new) or "
                    "%i (old buggy) bounding-boxes, but it has %i.  Proceeding "
                    "as though this table extension doesn't exist.\n",
                    KD_STR_BB, nitems_new, nitems_old, extras[ibb].nitems);
            extras[ibb].found = 0;
        }
        if (extras[ibb].nitems == nitems_old) {
            fprintf(stderr, "Warning: this file contains an old buggy %s extension; it "
                    "has %i rather than %i items.  Proceeding anyway, but this is probably a "
                    "bug unless you are running the fix-bb program!\n",
                    KD_STR_BB, nitems_old, nitems_new);
        }
    }

	// you need either bounding boxes, compatibility nodes, or split position/dim
	if (!(extras[ibb].found ||
		  extras[inodes].found ||
		  (extras[isplit].found &&
		   (TTYPE_INTEGER || extras[isplitdim].found)))) {
		fprintf(stderr, "tree contains neither traditional nodes, bounding boxes nor split+dim data.\n");
		kdtree_fits_close(kdt);
		return NULL;
	}

	if ((TTYPE_INTEGER && !ETYPE_INTEGER) &&
		!(extras[irange].found)) {
		fprintf(stderr, "treee does not contain required range information.\n");
		kdtree_fits_close(kdt);
		return NULL;
	}

	bbtree = extras[ibb].found;

	if (extras[irange].found) {
		tempranges = extras[irange].ptr;
		kdt->minval = tempranges;
		kdt->maxval = tempranges + kdt->ndim;
		kdt->scale  = tempranges[kdt->ndim * 2];
		kdt->invscale = 1.0 / kdt->scale;
	}

	kdt->data.any = extras[idata].ptr;
	
	if (extras[isplit].found) {
        if (extras[isplitdim].found) {
            kdt->splitmask = UINT32_MAX;
        } else {
            compute_splitbits(kdt);
        }
	}

	if (extras[isplitdim].found) {
		kdt->splitdim = extras[isplitdim].ptr;
	}

	if (extras[isplit].found) {
		kdt->split.any = extras[isplit].ptr;
	}

	if (extras[ibb].found) {
		kdt->bb.any = extras[ibb].ptr;
	}

	if (extras[iperm].found) {
		kdt->perm = extras[iperm].ptr;
	}

	if (extras[ilr].found) {
		kdt->lr = extras[ilr].ptr;
	}

	if (extras[inodes].found) {
		kdt->nodes = extras[inodes].ptr;
	}

	if (nuserextras)
		// copy out...
		memcpy(userextras, extras + (Ne - nuserextras), nuserextras * sizeof(extra_table));

	return kdt;
}

int MANGLE(kdtree_write_fits)(kdtree_t* kd, char* fn, qfits_header* hdr, extra_table* ue, int nue) {
	extra_table* ext;
	int Ne;
	double tempranges[kd->ndim * 2 + 1];
	extra_table extras[10 + nue];
	int rtn;

	memset(extras, 0, sizeof(extras));
	ext = extras;

	if (!hdr)
		hdr = qfits_header_default();

	if (kd->nodes) {
		ext->ptr = kd->nodes;
		ext->name = KD_STR_NODES;
		ext->datasize = COMPAT_NODE_SIZE(kd);
		ext->nitems = kd->nnodes;
		fits_append_long_comment
			(hdr, "The table containing column \"%s\" contains \"legacy\" "
			 "kdtree nodes (kdtree_node_t structs).  These nodes contain two "
			 "%u-byte, native-endian unsigned ints, followed by a bounding-box, "
			 "which is two points in NDIM (=%u) dimensional space, stored as "
			 "native-endian doubles (%u bytes each).  The whole struct has size %u.",
			 KD_STR_NODES, (unsigned int)sizeof(unsigned int), kd->ndim, (unsigned int)sizeof(double),
			 (unsigned int)COMPAT_NODE_SIZE(kd));
		ext++;
	}
	if (kd->lr) {
		ext->ptr = kd->lr;
		ext->name = KD_STR_LR;
		ext->datasize = sizeof(u32);
		ext->nitems = kd->nbottom;
		fits_append_long_comment
			(hdr, "The \"%s\" table contains the kdtree \"LR\" array. "
			 "This array has one %u-byte, native-endian unsigned int for each "
			 "leaf node in the tree. For each node, it gives the index of the "
			 "rightmost data point owned by the node.",
			 KD_STR_LR, (unsigned int)sizeof(u32));
		ext++;
	}
	if (kd->perm) {
		ext->ptr = kd->perm;
		ext->name = KD_STR_PERM;
		ext->datasize = sizeof(u32);
		ext->nitems = kd->ndata;
		fits_append_long_comment
			(hdr, "The \"%s\" table contains the kdtree permutation array. "
			 "This array contains one %u-byte, native-endian unsigned int for "
			 "each data point in the tree. For each data point, it gives the "
			 "index that the data point had in the original array on which the "
			 "kdtree was built.", KD_STR_PERM, (unsigned int)sizeof(u32));
		ext++;
	}
	if (kd->bb.any) {
		ext->ptr = kd->bb.any;
		ext->name = KD_STR_BB;
		ext->datasize = sizeof(ttype) * kd->ndim * 2;
		ext->nitems = kd->nnodes;
		fits_append_long_comment
			(hdr, "The \"%s\" table contains the kdtree bounding-box array. "
			 "This array contains two %u-dimensional points, stored as %u-byte, "
			 "native-endian %ss, for each node in the tree. Each data "
			 "point owned by a node is contained within its bounding box.",
			 KD_STR_BB, (unsigned int)kd->ndim, (unsigned int)sizeof(ttype),
			 kdtree_kdtype_to_string(kdtree_treetype(kd)));
		ext++;
	}
	if (kd->split.any) {
		ext->ptr = kd->split.any;
		ext->name = KD_STR_SPLIT;
		ext->datasize = sizeof(ttype);
		ext->nitems = kd->ninterior;
		if (!kd->splitdim) {
			fits_append_long_comment
				(hdr, "The \"%s\" table contains the kdtree splitting-plane "
				 "boundaries, and also the splitting dimension, packed into "
				 "a %u-byte, native-endian %s, for each interior node in the tree. "
				 "The splitting dimension is packed into the low %u bit%s, and the "
				 "splitting location uses the remaining bits. "
				 "The left child of a node contains data points that lie on the "
				 "low side of the splitting plane, and the right child contains "
				 "data points on the high side of the plane.",
				 KD_STR_SPLIT, (unsigned int)sizeof(ttype),
				 kdtree_kdtype_to_string(kdtree_treetype(kd)),
				 kd->dimbits, (kd->dimbits > 1 ? "s" : ""));
		} else {
			fits_append_long_comment
				(hdr, "The \"%s\" table contains the kdtree splitting-plane "
				 "boundaries as %u-byte, native-endian %s, for each interior node in the tree. "
				 "The dimension along which the splitting-plane splits is stored in "
				 "a separate array. "
				 "The left child of a node contains data points that lie on the "
				 "low side of the splitting plane, and the right child contains "
				 "data points on the high side of the plane.",
				 KD_STR_SPLIT, (unsigned int)sizeof(ttype),
				 kdtree_kdtype_to_string(kdtree_treetype(kd)));
		}
		ext++;
	}
	if (kd->splitdim) {
		ext->ptr = kd->splitdim;
		ext->name = KD_STR_SPLITDIM;
		ext->datasize = sizeof(u8);
		ext->nitems = kd->ninterior;
		fits_append_long_comment
			(hdr, "The \"%s\" table contains the kdtree splitting-plane "
			 "dimensions as %u-byte unsigned ints, for each interior node in the tree. "
			 "The location of the splitting-plane along that dimension is stored "
			 "in a separate array. "
			 "The left child of a node contains data points that lie on the "
			 "low side of the splitting plane, and the right child contains "
			 "data points on the high side of the plane.",
			 KD_STR_SPLITDIM, (unsigned int)sizeof(u8));
		ext++;
	}
	if (kd->data.any) {
		ext->ptr = kd->data.any;
		ext->name = KD_STR_DATA;
		ext->datasize = sizeof(dtype) * kd->ndim;
		ext->nitems = kd->ndata;
		fits_append_long_comment
			(hdr, "The \"%s\" table contains the kdtree data. "
			 "It is stored as %u-dimensional, %u-byte native-endian %ss.",
			 KD_STR_DATA, (unsigned int)kd->ndim, (unsigned int)sizeof(dtype),
			 kdtree_kdtype_to_string(kdtree_datatype(kd)));
		ext++;
	}
	if (kd->minval && kd->maxval) {
		int d;
		memcpy(tempranges, kd->minval, kd->ndim * sizeof(double));
		memcpy(tempranges + kd->ndim, kd->maxval, kd->ndim * sizeof(double));
		tempranges[kd->ndim*2] = kd->scale;

		ext->ptr = tempranges;
		ext->name = KD_STR_RANGE;
		ext->datasize = sizeof(double);
		ext->nitems = (kd->ndim * 2 + 1);
		fits_append_long_comment
			(hdr, "The \"%s\" table contains the scaling parameters of the "
			 "kdtree.  This tells how to convert from the format of the data "
			 "to the internal format of the tree (and vice versa). "
			 "It is stored as an array "
			 "of %u-byte, native-endian doubles.  The first %u elements are "
			 "the lower bound of the data, the next %u elements are the upper "
			 "bound, and the final element is the scale, which says how many "
			 "tree units there are per data unit.",
			 KD_STR_RANGE, (unsigned int)sizeof(double), (unsigned int)kd->ndim, (unsigned int)kd->ndim);
		fits_append_long_comment
			(hdr, "For reference, here are the ranges of the data.  Note that "
			 "this is not used by the libkd software, it's just for human readers.");
		for (d=0; d<kd->ndim; d++)
			fits_append_long_comment
				(hdr, "  dim %i: [%g, %g]", d, kd->minval[d], kd->maxval[d]);
		fits_append_long_comment(hdr, "scale: %g", kd->scale);
		fits_append_long_comment(hdr, "1/scale: %g", kd->invscale);
		ext++;
	}

	if (nue) {
		// copy in user extras...
		memcpy(ext, ue, nue * sizeof(extra_table));
		ext += nue;
	}

	Ne = ext - extras;
	
	qfits_header_append(hdr, "KDT_EXT",  (char*)kdtree_kdtype_to_string(kdtree_exttype(kd)),  "kdtree: external type", NULL);
	qfits_header_append(hdr, "KDT_INT",  (char*)kdtree_kdtype_to_string(kdtree_treetype(kd)), "kdtree: type of the tree's structures", NULL);
	qfits_header_append(hdr, "KDT_DATA", (char*)kdtree_kdtype_to_string(kdtree_datatype(kd)), "kdtree: type of the data", NULL);

	rtn = kdtree_fits_common_write(kd, fn, hdr, extras, Ne);

	if (nue) {
		// copy out user extras...
		memcpy(ue, extras + Ne - nue, nue * sizeof(extra_table));
	}
	return rtn;
}

#endif
