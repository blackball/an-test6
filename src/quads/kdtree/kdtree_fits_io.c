#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>

#include "qfits.h"
#include "kdtree_fits_io.h"
#include "mathutil.h"
#include "ioutils.h"
#include "fitsioutils.h"

kdtree_t* kdtree_fits_read_file(char* fn) {
	FILE* fid;
	kdtree_t* kdtree = NULL;
	qfits_header* header;
	uint ndata, ndim, nnodes;
	int offnodes, offdata, offperm;
	int sizenodes, sizedata, sizeperm;
    int nodesize;
	int size;
	void* map;
	int realsz;
	bool hasperm;

	if (!is_fits_file(fn)) {
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
		fprintf(stderr, "Couldn't read startree FITS header from %s.\n", fn);
		fclose(fid);
		return NULL;
	}

    if (fits_check_endian(header) ||
        fits_check_uint_size(header)) {
		fprintf(stderr, "File %s was written with wrong endianness or uint size.\n", fn);
        fclose(fid);
        return NULL;
    }

	ndim   = qfits_header_getint(header, "NDIM", -1);
	ndata  = qfits_header_getint(header, "NDATA", -1);
	nnodes = qfits_header_getint(header, "NNODES", -1);
	realsz = qfits_header_getint(header, "REAL_SZ", -1);

	qfits_header_destroy(header);

	if ((ndim == -1) || (ndata == -1) || (nnodes == -1)) {
		fprintf(stderr, "Couldn't find NDIM, NDATA, or NNODES entries in the startree FITS header.\n");
		fclose(fid);
		return NULL;
	}
	//fprintf(stderr, "ndim %u, ndata %u, nnodes %u.\n", ndim, ndata, nnodes);

	if (sizeof(real) != realsz) {
		fprintf(stderr, "File was written with sizeof(real)=%i, but currently sizeof(real)=%i.\n",
				realsz, sizeof(real));
		fclose(fid);
		return NULL;
	}

    if (fits_find_table_column(fn, "kdtree_nodes", &offnodes, &sizenodes) ||
        fits_find_table_column(fn, "kdtree_data",  &offdata , &sizedata )) {
		fprintf(stderr, "Couldn't find nodes, or data.\n");
		fclose(fid);
		return NULL;
	}
	if (fits_find_table_column(fn, "kdtree_perm",  &offperm , &sizeperm )) {
		hasperm = FALSE;
    } else
		// haha, you have a funny hairstyle.
		hasperm = TRUE;

	/*
	  fprintf(stderr, "nodes offset %i, size %i\n", offnodes, sizenodes);
	  fprintf(stderr, "data  offset %i, size %i\n", offdata, sizedata);
	  fprintf(stderr, "perm  offset %i, size %i\n", offperm, sizeperm);
	*/

    nodesize = sizeof(kdtree_node_t) + sizeof(real) * ndim * 2;

	if ((fits_blocks_needed(nodesize * nnodes) != sizenodes) ||
		(hasperm && (fits_blocks_needed(ndata * sizeof(int)) != sizeperm)) ||
		(fits_blocks_needed(ndata * ndim * sizeof(real)) != sizedata)) {
		fprintf(stderr, "Node, data, or permutation size doesn't jive!");
		fprintf(stderr, "  nodes (%i -> %i vs %i -> %i)\n",
				nodesize*nnodes, fits_blocks_needed(nodesize*nnodes),
				sizenodes, fits_blocks_needed(sizenodes));
		fprintf(stderr, "  data (%i -> %i vs %i)\n",
				ndata * ndim * sizeof(real), fits_blocks_needed(ndata*ndim*sizeof(real)),
				sizedata);
		if (hasperm)
			fprintf(stderr, "  perm (%i -> %i vs %i)\n",
					ndata * sizeof(int), fits_blocks_needed(ndata * sizeof(int)), sizeperm);
		fclose(fid);
		return NULL;
	}

	// launch!
	size = offnodes + sizenodes;
	size = imax(size, offdata + sizedata);
	if (hasperm)
		size = imax(size, offperm + sizeperm);

	map = mmap(0, size, PROT_READ, MAP_SHARED, fileno(fid), 0);
	fclose(fid);
	if (map == MAP_FAILED) {
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
		return NULL;
	}

    kdtree = malloc(sizeof(kdtree_t));
    if (!kdtree) {
		fprintf(stderr, "Couldn't allocate kdtree.\n");
		return NULL;
    }

    kdtree->ndata  = ndata;
    kdtree->ndim   = ndim;
    kdtree->nnodes = nnodes;
	kdtree->mmapped = map;
	kdtree->mmapped_size = size;
	if (hasperm)
		kdtree->perm = (unsigned int*) (map + offperm);
	else
		kdtree->perm = NULL;
	kdtree->data = (real*)         (map + offdata);
	kdtree->tree = (kdtree_node_t*)(map + offnodes);

	return kdtree;
}

int kdtree_fits_write_file(kdtree_t* kdtree, char* fn, qfits_header* hdr) {
    int nodesize;
    int ncols, nrows;
    int datasize;
    int tablesize;
    qfits_table* table;
    qfits_header* header;
    qfits_header* tablehdr;
    FILE* fid;
    void* dataptr;
    char val[256];

    fid = fopen(fn, "wb");
    if (!fid) {
        fprintf(stderr, "Couldn't open file %s to write kdtree: %s\n",
                fn, strerror(errno));
        return -1;
    }

    // the header
    header = qfits_table_prim_header_default();
    fits_add_endian(header);
    fits_add_uint_size(header);
	sprintf(val, "%i", sizeof(real));
	qfits_header_add(header, "REAL_SZ", val, "sizeof(real)", NULL);
    sprintf(val, "%i", kdtree->ndata);
    qfits_header_add(header, "NDATA", val, "kdtree: number of data points", NULL);
    sprintf(val, "%i", kdtree->ndim);
    qfits_header_add(header, "NDIM", val, "kdtree: number of dimensions", NULL);
    sprintf(val, "%i", kdtree->nnodes);
    qfits_header_add(header, "NNODES", val, "kdtree: number of nodes", NULL);
	qfits_header_add(header, "COMMENT", "The first extension contains the kdtree node ", NULL, NULL);
	qfits_header_add(header, "COMMENT", " structs.", NULL, NULL);
	qfits_header_add(header, "COMMENT", "The second extension contains the kdtree data, ", NULL, NULL);
	qfits_header_add(header, "COMMENT", " stored in native-endian, 8-byte doubles.", NULL, NULL);
	qfits_header_add(header, "COMMENT", "The third extension, if it exists,", NULL, NULL);
	qfits_header_add(header, "COMMENT", " contains the kdtree permutation array,", NULL, NULL);
	qfits_header_add(header, "COMMENT", " stored as native-endian 4-byte unsigned", NULL, NULL);
	qfits_header_add(header, "COMMENT", " ints.  If it doesn't exist, the permutation", NULL, NULL);
	qfits_header_add(header, "COMMENT", " array is assumed to be the identity.", NULL, NULL);

	if (hdr) {
		int i;
		char key[FITS_LINESZ+1];
		char val[FITS_LINESZ+1];
		char com[FITS_LINESZ+1];
		char lin[FITS_LINESZ+1];
		//printf("Adding %i header entries\n", hdr->n);
		for (i=0; i<hdr->n; i++) {
			qfits_header_getitem(hdr, i, key, val, com, lin);
			//printf("Adding header key %s\n", key);
			qfits_header_add(header, key, val, com, lin);
		}
	}

    qfits_header_dump(header, fid);
    qfits_header_destroy(header);

    // first table: the kdtree structs.
    nodesize = sizeof(kdtree_node_t) + sizeof(real) * kdtree->ndim * 2;
    dataptr = kdtree->tree;
    datasize = nodesize;
    ncols = 1;
    nrows = kdtree->nnodes;
    tablesize = datasize * nrows * ncols;
    table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
    qfits_col_fill(table->col,
                   datasize, 0, 1, TFITS_BIN_TYPE_A,
                   "kdtree_nodes",
				   "", "", "",
                   0, 0, 0, 0,
                   0);
    //qfits_save_table_hdrdump(&dataptr, table, header);
    //qfits_table_append_xtension(fid, table, &dataptr);
 	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, fid);
	qfits_header_destroy(tablehdr);
	qfits_table_close(table);
	if ((fwrite(dataptr, 1, tablesize, fid) != tablesize) ||
		fits_pad_file(fid)) {
		fprintf(stderr, "Failed to write kdtree nodes: %s\n", strerror(errno));
		return -1;
	}

    // second table: the data.
    datasize = kdtree->ndim * sizeof(real);
    dataptr = kdtree->data;
    ncols = 1;
    nrows = kdtree->ndata;
    tablesize = datasize * nrows * ncols;

    table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
    qfits_col_fill(table->col,
                   datasize, 0, 1, TFITS_BIN_TYPE_A,
                   "kdtree_data",
				   "", "", "",
                   0, 0, 0, 0,
                   0);
    //qfits_table_append_xtension(fid, table, &dataptr);
    //qfits_table_close(table);
 	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, fid);
	qfits_header_destroy(tablehdr);
	qfits_table_close(table);
	if ((fwrite(dataptr, 1, tablesize, fid) != tablesize) ||
		fits_pad_file(fid)) {
		fprintf(stderr, "Failed to write kdtree data: %s\n", strerror(errno));
		return -1;
	}

    // third table: the permutation vector.
	if (kdtree->perm) {
		datasize = sizeof(int);
		dataptr = kdtree->perm;
		ncols = 1;
		nrows = kdtree->ndata;
		tablesize = datasize * nrows * ncols;

		table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
		qfits_col_fill(table->col,
					   datasize, 0, 1, TFITS_BIN_TYPE_A,
					   "kdtree_perm",
					   "", "", "",
					   0, 0, 0, 0,
					   0);
		//qfits_table_append_xtension(fid, table, &dataptr);
		//qfits_table_close(table);
		tablehdr = qfits_table_ext_header_default(table);
		qfits_header_dump(tablehdr, fid);
		qfits_header_destroy(tablehdr);
		qfits_table_close(table);
		if ((fwrite(dataptr, 1, tablesize, fid) != tablesize) ||
			fits_pad_file(fid)) {
			fprintf(stderr, "Failed to write kdtree perm: %s\n", strerror(errno));
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
	assert(kd);
	munmap(kd->mmapped, kd->mmapped_size);
	free(kd);
}
