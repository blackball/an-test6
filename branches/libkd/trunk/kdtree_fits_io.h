#ifndef KDTREE_FITS_IO_H
#define KDTREE_FITS_IO_H

#include "kdtree.h"
#include "qfits.h"

kdtree_t* kdtree_fits_read_file(char* fn);

int kdtree_fits_write_file(kdtree_t* kdtree, char* fn, qfits_header* hdr);


struct extra_table_info;
typedef struct extra_table_info extra_table;

kdtree_t* kdtree_fits_read_file_extras(char* fn, extra_table* extras, int nextras);

int kdtree_fits_write_file_extras(kdtree_t* kdtree, char* fn, qfits_header* hdr, extra_table* extras, int nextras);

struct extra_table_info {
	char* name;

	// set to non-zero if you know how big the table should be.
	int datasize;
	int nitems;

	// abort if this table isn't found?
	int required;

	// this gets called after the kdtree size has been discovered.
	void (*compute_tablesize)(kdtree_t* kd, struct extra_table_info* thistable);

	void* ptr;

	int found;
	int offset;
	int size;
};

#endif
