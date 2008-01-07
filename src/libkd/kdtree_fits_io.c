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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/param.h>

#include "kdtree_fits_io.h"
#include "kdtree_internal.h"
#include "kdtree_mem.h"
#include "fitsioutils.h"
#include "qfits.h"

// is the given table name one of the above strings?
int kdtree_fits_column_is_kdtree(char* columnname) {
    return
        (strcmp(columnname, KD_STR_NODES) == 0) ||
        (strcmp(columnname, KD_STR_LR   ) == 0) ||
        (strcmp(columnname, KD_STR_PERM ) == 0) ||
        (strcmp(columnname, KD_STR_BB   ) == 0) ||
        (strcmp(columnname, KD_STR_SPLIT) == 0) ||
        (strcmp(columnname, KD_STR_SPLITDIM) == 0) ||
        (strcmp(columnname, KD_STR_DATA ) == 0) ||
        (strcmp(columnname, KD_STR_RANGE) == 0);
}

kdtree_t* kdtree_fits_read(char* fn, qfits_header** p_hdr) {
	return kdtree_fits_read_extras(fn, p_hdr, NULL, 0);
}

// declarations
KD_DECLARE(kdtree_read_fits, kdtree_t*, (char* fn, qfits_header** p_hdr, unsigned int treetype, extra_table* uextras, int nuextras));

/**
   This function reads FITS headers to try to determine which kind of tree
   is contained in the file, then calls the appropriate (mangled) function
   kdtree_read_fits() (defined in kdtree_internal_fits.c).  This, in turn,
   calls kdtree_fits_common_read(), then does some extras processing and
   returns.
 */
kdtree_t* kdtree_fits_read_extras(char* fn, qfits_header** p_hdr, extra_table* extras, int nextras) {
	qfits_header* header;
	unsigned int ext_type, int_type, data_type;
	unsigned int tt;
	char* str;
	kdtree_t* kd = NULL;
	int legacy = 0;

	if (!qfits_is_fits(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS header from %s.\n", fn);
		return NULL;
	}

	str = qfits_header_getstr(header, "KDT_EXT");
	ext_type = kdtree_kdtype_parse_ext_string(qfits_pretty_string(str));

	str = qfits_header_getstr(header, "KDT_INT");
	int_type = kdtree_kdtype_parse_tree_string(qfits_pretty_string(str));

	str = qfits_header_getstr(header, "KDT_DATA");
	data_type = kdtree_kdtype_parse_data_string(qfits_pretty_string(str));

	/*
	  printf("kdtree types: external %s, internal %s, data %s.\n",
	  kdtree_kdtype_to_string(ext_type),
	  kdtree_kdtype_to_string(int_type),
	  kdtree_kdtype_to_string(data_type));
	*/

	if ((ext_type == KDT_NULL) &&
		(int_type == KDT_NULL) &&
		(data_type == KDT_NULL)) {
		// if it's got a REAL_SZ header, it might be a legacy kdtree.
		int realsz = qfits_header_getint(header, "REAL_SZ", 0);
		if (realsz == sizeof(double)) {
			//printf("it might be a legacy kdtree!\n");
			legacy = 1;
			// select KDTT_DOUBLE
			int_type  = KDT_TREE_DOUBLE;
			data_type = KDT_DATA_DOUBLE;
			ext_type  = KDT_EXT_DOUBLE;
		}
	}

	// default: external world is doubles.
	if (ext_type == KDT_NULL) {
		ext_type = KDT_EXT_DOUBLE;
	}

	qfits_header_destroy(header);

	tt = kdtree_kdtypes_to_treetype(ext_type, int_type, data_type);
	//printf("kd treetype %#x\n", tt);

	KD_DISPATCH(kdtree_read_fits, tt, kd = , (fn, p_hdr, tt, extras, nextras));

	return kd;
}

KD_DECLARE(kdtree_write_fits, int, (kdtree_t* kd, char* fn, qfits_header* hdr, extra_table* ue, int nue));

/**
   This function calls the appropriate (mangled) function
   kdtree_write_fits(), which in turn calls kdtree_fits_common_write()
   which does the actual writing.
 */
int kdtree_fits_write_extras(kdtree_t* kd, char* fn, qfits_header* hdr, extra_table* userextras, int nuserextras) {
	int rtn = -1;
	KD_DISPATCH(kdtree_write_fits, kd->treetype, rtn = , (kd, fn, hdr, userextras, nuserextras));
	return rtn;
}

int kdtree_fits_write(kdtree_t* kdtree, char* fn, qfits_header* hdr) {
	return kdtree_fits_write_extras(kdtree, fn, hdr, NULL, 0);
}

kdtree_t* kdtree_fits_common_read(char* fn, qfits_header** p_hdr, unsigned int treetype, extra_table* extras, int nextras) {
	FILE* fid;
	kdtree_t* kdtree = NULL;
	qfits_header* header;
	unsigned int ndata, ndim, nnodes;
	int size;
	unsigned char* map;
	int i;

	if (!qfits_is_fits(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read kdtree: %s\n",
				fn, strerror(errno));
		return NULL;
	}

	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS header from %s.\n", fn);
		fclose(fid);
		return NULL;
	}

    if (fits_check_endian(header)) {
		fprintf(stderr, "File %s was written with wrong endianness.\n", fn);
        fclose(fid);
        return NULL;
    }

	ndim   = qfits_header_getint(header, "NDIM", -1);
	ndata  = qfits_header_getint(header, "NDATA", -1);
	nnodes = qfits_header_getint(header, "NNODES", -1);

	if (p_hdr)
		*p_hdr = header;
	else
		qfits_header_destroy(header);

	if ((ndim == -1) || (ndata == -1) || (nnodes == -1)) {
		fprintf(stderr, "Couldn't find NDIM, NDATA, or NNODES entries in the startree FITS header.\n");
		fclose(fid);
		return NULL;
	}

	for (i=0; i<nextras; i++) {
		extra_table* tab = extras + i;
		if (fits_find_table_column(fn, tab->name, &tab->offset, &tab->size, NULL)) {
			if (tab->required) {
				fprintf(stderr, "Failed to find table %s in file %s.\n", tab->name, fn);
				fclose(fid);
				return NULL;
			}
			tab->found = 0;
		} else
			tab->found = 1;
	}

    kdtree = CALLOC(1, sizeof(kdtree_t));
    if (!kdtree) {
		fprintf(stderr, "Couldn't allocate kdtree.\n");
		return NULL;
    }
    kdtree->ndata  = ndata;
    kdtree->ndim   = ndim;
    kdtree->nnodes = nnodes;
	kdtree->nbottom = (nnodes+1)/2;
	kdtree->ninterior = nnodes - kdtree->nbottom;
    kdtree->nlevels = kdtree_nnodes_to_nlevels(nnodes);
	kdtree->treetype = treetype;

	size = 0;
	for (i=0; i<nextras; i++) {
		extra_table* tab = extras + i;
		int tablesize;
		int col;
		qfits_table* table;
		int ds;

		if (!tab->found)
			continue;
		if (tab->compute_tablesize)
			tab->compute_tablesize(kdtree, tab);

		table = fits_get_table_column(fn, tab->name, &col);
		if (tab->nitems) {
			if (tab->nitems != table->nr) {
				fprintf(stderr, "Table %s in file %s: expected %i data items, found %i.\n",
						tab->name, fn, tab->nitems, table->nr);
				FREE(kdtree);
				qfits_table_close(table);
				return NULL;
			}
		} else {
			tab->nitems = table->nr;
		}
		ds = table->col[col].atom_nb * table->col[col].atom_size;
		if (tab->datasize) {
			if (tab->datasize != ds) {
				fprintf(stderr, "Table %s in file %s: expected data size %i, found %i.\n",
						tab->name, fn, tab->datasize, ds);
				FREE(kdtree);
				qfits_table_close(table);
				return NULL;
			}
		} else {
			tab->datasize = ds;
		}
		qfits_table_close(table);

		tablesize = tab->datasize * tab->nitems;
		if (fits_bytes_needed(tablesize) != tab->size) {
			fprintf(stderr, "The size of table %s in file %s doesn't jive with what's expected: %i vs %i.\n",
					tab->name, fn, fits_bytes_needed(tablesize), tab->size);
			FREE(kdtree);
			return NULL;
		}

		size = MAX(size, tab->offset + tab->size);
	}

	// launch!
	map = mmap(0, size, PROT_READ, MAP_SHARED, fileno(fid), 0);
	fclose(fid);
	if (map == MAP_FAILED) {
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
		FREE(kdtree);
		return NULL;
	}

	kdtree->mmapped = map;
	kdtree->mmapped_size = size;

	for (i=0; i<nextras; i++) {
		extra_table* tab = extras + i;
		if (!tab->found)
			continue;
		tab->ptr = (map + tab->offset);
	}

	return kdtree;
}

int kdtree_fits_common_write(kdtree_t* kdtree, char* fn, qfits_header* hdr, extra_table* extras, int nextras) {
    int ncols, nrows;
    int datasize;
    int tablesize;
    qfits_table* table;
    qfits_header* header;
    qfits_header* tablehdr;
    FILE* fid;
    void* dataptr;
	int i;

    fid = fopen(fn, "wb");
    if (!fid) {
        fprintf(stderr, "Couldn't open file %s to write kdtree: %s\n",
                fn, strerror(errno));
        return -1;
    }

    // we create a new header and copy in the user's header items.
    header = qfits_table_prim_header_default();
    fits_add_endian(header);
    fits_header_add_int(header, "NDATA", kdtree->ndata, "kdtree: number of data points");
    fits_header_add_int(header, "NDIM", kdtree->ndim, "kdtree: number of dimensions");
    fits_header_add_int(header, "NNODES", kdtree->nnodes, "kdtree: number of nodes");

	if (hdr) {
		int i;
		char key[FITS_LINESZ+1];
		char val[FITS_LINESZ+1];
		char com[FITS_LINESZ+1];
		char lin[FITS_LINESZ+1];
		for (i=0; i<hdr->n; i++) {
			qfits_header_getitem(hdr, i, key, val, com, lin);
			qfits_header_add(header, key, val, com, lin);
		}
	}

    qfits_header_dump(header, fid);
    qfits_header_destroy(header);

	for (i=0; i<nextras; i++) {
		extra_table* tab = extras + i;
		if (tab->dontwrite)
			continue;
		datasize = tab->datasize;
		dataptr  = tab->ptr;
		ncols = 1;
		nrows = tab->nitems;
		tablesize = datasize * nrows * ncols;
		table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
		qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
					   tab->name, "", "", "", 0, 0, 0, 0, 0);
		tablehdr = qfits_table_ext_header_default(table);
		qfits_header_dump(tablehdr, fid);
		qfits_header_destroy(tablehdr);
		qfits_table_close(table);
		if ((fwrite(dataptr, 1, tablesize, fid) != tablesize) ||
			fits_pad_file(fid)) {
			fprintf(stderr, "Failed to write kdtree table %s: %s\n", tab->name, strerror(errno));
			return -1;
		}
	}

    if (fclose(fid)) {
        fprintf(stderr, "Couldn't close file %s after writing kdtree: %s\n",
                fn, strerror(errno));
        return -1;
    }
    return 0;
}

void kdtree_fits_close(kdtree_t* kd) {
	if (!kd) return;
	munmap(kd->mmapped, kd->mmapped_size);
	FREE(kd);
}
