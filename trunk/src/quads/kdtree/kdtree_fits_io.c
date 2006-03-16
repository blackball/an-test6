#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>

#include "qfits.h"
#include "kdtree_fits_io.h"

kdtree_t* kdtree_fits_read_file(char* fn) {
	FILE* fid;
	kdtree_t* kdtree;
	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read kdtree: %s\n",
				fn, strerror(errno));
		return NULL;
	}
	//kdtree = kdtree_fits_read(fid);
	fclose(fid);
	return kdtree;
}

/*
  kdtree_t* kdtree_fits_read(FILE* fin) {
  return NULL;
  }
*/

int kdtree_fits_write_file(kdtree_t* kdtree, char* fn) {
    //int nelems;
    int nodesize;
    //int nbytes;
	//int err = 0;
    int ncols, nrows;
    int datasize;
    int tablesize;
    qfits_table* table;
    qfits_header* header;
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
    sprintf(val, "%i", kdtree->ndata);
    qfits_header_add(header, "kdtree_ndata", val, "kdtree: number of data points", NULL);
    sprintf(val, "%i", kdtree->ndim);
    qfits_header_add(header, "kdtree_ndim", val, "kdtree: number of dimensions", NULL);
    sprintf(val, "%i", kdtree->nnodes);
    qfits_header_add(header, "kdtree_nnodes", val, "kdtree: number of nodes", NULL);

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
                   "(no units)", "(no nullval)", "(no display)",
                   0, 0, 0, 0,
                   0);
    qfits_save_table_hdrdump(&dataptr, table, header);
    qfits_header_dump(header, fid);
    qfits_table_append_xtension(fid, table, &dataptr);
    qfits_table_close(table);
    qfits_header_destroy(header);

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
                   "(no units)", "(no nullval)", "(no display)",
                   0, 0, 0, 0,
                   0);
    qfits_table_append_xtension(fid, table, &dataptr);
    qfits_table_close(table);

    // third table: the permutation vector.
    datasize = sizeof(int);
    dataptr = kdtree->perm;
    ncols = 1;
    nrows = kdtree->ndata;
    tablesize = datasize * nrows * ncols;

    table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
    qfits_col_fill(table->col,
                   datasize, 0, 1, TFITS_BIN_TYPE_A,
                   "kdtree_perm",
                   "(no units)", "(no nullval)", "(no display)",
                   0, 0, 0, 0,
                   0);
    qfits_table_append_xtension(fid, table, &dataptr);
    qfits_table_close(table);

    if (fclose(fid)) {
        fprintf(stderr, "Couldn't close file %s after writing kdtree: %s\n",
                fn, strerror(errno));
        return -1;
    }
    return 0;
}

int kdtree_fits_write(FILE* fout, kdtree_t* kdtree) {


    /*
      if (!err) err |= write_u8(fout, 2);
      if (!err) err |= write_u8(fout, sizeof(real));
      if (!err) err |= write_u8(fout, sizeof(unsigned int));
      if (!err) err |= write_u8(fout, sizeof(kdtree_node_t));
      if (!err) err |= write_u32(fout, ENDIAN_DETECTOR);
      if (!err) err |= write_u32(fout, nodesize);
      if (!err) err |= write_u32(fout, kdtree->ndata);
      if (!err) err |= write_u32(fout, kdtree->ndim);
      if (!err) err |= write_u32(fout, kdtree->nnodes);
      if (err) {
      fprintf(stderr, "Couldn't write kdtree header.\n");
      return 1;
      }
      if (write_uints(fout, kdtree->perm, kdtree->ndata)) {
      fprintf(stderr, "Couldn't write kdtree permutation vector.\n");
      return 1;
      }
      nelems = kdtree->ndata * kdtree->ndim;
      if (write_reals(fout, kdtree->data, nelems)) {
      fprintf(stderr, "Couldn't write kdtree data.\n");
      return 1;
      }
      nbytes = nodesize * kdtree->nnodes;
      if (fwrite(kdtree->tree, 1, nbytes, fout) != nbytes) {
      fprintf(stderr, "Couldn't write kdtree nodes\n");
      return 1;
      }
    */
    return 0;
}



void kdtree_fits_close(kdtree_t* kd) {
	assert(kd);
	munmap(kd->mmapped, kd->mmapped_size);
	free(kd);
}
