/*
  This file is part of libkd.
  Copyright 2006, Dustin Lang and Keir Mierle.

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

#ifndef KDTREE_FITS_IO_H
#define KDTREE_FITS_IO_H

#include "kdtree.h"
#include "qfits.h"

struct extra_table_info;
typedef struct extra_table_info extra_table;


kdtree_t* kdtree_fits_read(char* fn, qfits_header** p_hdr);

kdtree_t* kdtree_fits_read_extras(char* fn, qfits_header** p_hdr, extra_table* extras, int nextras);

int kdtree_fits_write(kdtree_t* kdtree, char* fn, qfits_header* hdr);

int kdtree_fits_write_extras(kdtree_t* kdtree, char* fn, qfits_header* hdr, extra_table* extras, int nextras);

void kdtree_fits_close(kdtree_t* kd);



struct extra_table_info {
	char* name;

	// set to non-zero if you know how big the table should be.
	int datasize;
	int nitems;

	// abort if this table isn't found?
	int required;

	// this gets called after the kdtree size has been discovered.
	void (*compute_tablesize)(kdtree_t* kd, struct extra_table_info* thistable);

	// the data
	void* ptr;

	int found;
	int offset;
	int size;

	// when writing: don't write this one.
	int dontwrite;
};


/* These shouldn't be called by user code; they are used internally. */
kdtree_t* kdtree_fits_common_read(char* fn, qfits_header** p_hdr, unsigned int treetype, extra_table* extras, int nextras);
int kdtree_fits_common_write(kdtree_t* kdtree, char* fn, qfits_header* hdr, extra_table* extras, int nextras);

#endif
