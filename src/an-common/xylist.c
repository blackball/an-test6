/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "xylist.h"
#include "fitsioutils.h"

static xylist* xylist_new() {
	xylist* ls;
	ls = calloc(1, sizeof(xylist));
	if (!ls) {
		fprintf(stderr, "Couldn't allocate an xylist.\n");
		return NULL;
	}
	ls->xtype = ls->ytype = TFITS_BIN_TYPE_E;
	ls->xname = "X";
	ls->yname = "Y";
	return ls;
}

xylist* xylist_open(const char* fn) {
	xylist* ls = NULL;
	qfits_header* header;

	if (!qfits_is_fits(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	ls = xylist_new();
	if (!ls)
		return NULL;

	ls->fn = strdup(fn);
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS header from xylist %s.\n", fn);
        goto bailout;
	}

	ls->antype = qfits_pretty_string(qfits_header_getstr(header, "AN_FILE"));
	ls->header = header;
	ls->nfields = qfits_query_n_ext(fn);

	return ls;

 bailout:
	if (ls && ls->fn)
		free(ls->fn);
	if (ls)
		free(ls);
	return NULL;
}

int xylist_n_fields(xylist* ls) {
	return ls->nfields;
}

static int xylist_find_field(xylist* ls, uint field) {
	qfits_col* col;
	if (ls->table && ls->field == field)
		return 0;
	if (ls->table) {
		qfits_table_close(ls->table);
		ls->table = NULL;
	}
	// the main FITS header is extension 0
	// the first FITS table is extension 1
	ls->table = qfits_table_open(ls->fn, field + 1);
	if (!ls->table) {
		fprintf(stderr, "FITS extension %i in file %s is not a table (or there was an error opening the file).\n",
				field+1, ls->fn);
		return -1;
	}
	// find the right columns.
	ls->xcol = fits_find_column(ls->table, ls->xname);
	if (ls->xcol == -1) {
		fprintf(stderr, "xylist %s: no column named \"%s\".\n", ls->fn, ls->xname);
		return -1;
	}
	col = ls->table->col + ls->xcol;
	if (!(((col->atom_type == TFITS_BIN_TYPE_D) ||
		   (col->atom_type == TFITS_BIN_TYPE_E)) &&
		  (col->atom_nb == 1))) {
		fprintf(stderr, "xylist %s: column \"%s\" is not of type D or E (double/float).\n", ls->fn, ls->xname);
		return -1;
	}
	ls->xtype = col->atom_type;
	ls->ycol = fits_find_column(ls->table, ls->yname);
	if (ls->ycol == -1) {
		fprintf(stderr, "xylist %s: no column named \"%s\".\n", ls->fn, ls->yname);
		return -1;
	}
	col = ls->table->col + ls->ycol;
	if (!(((col->atom_type == TFITS_BIN_TYPE_D) ||
		   (col->atom_type == TFITS_BIN_TYPE_E)) &&
		  (col->atom_nb == 1))) {
		fprintf(stderr, "xylist %s: column \"%s\" is not of type D or E (double/float).\n", ls->fn, ls->yname);
		return -1;
	}
	ls->ytype = col->atom_type;

	ls->field = field;
	return 0;
}

xy* xylist_get_field(xylist* ls, uint field) {
	int i, nobjs;
	double* vals;
	xy* rtn;
	nobjs = xylist_n_entries(ls, field);
	if (nobjs == -1) {
		fprintf(stderr, "Field %i couldn't be read.\n", field);
		return NULL;
	}
	rtn = mk_xy(nobjs);
	vals = malloc(2 * nobjs * sizeof(double));
	xylist_read_entries(ls, field, 0, nobjs, vals);
	for (i=0; i<nobjs; i++) {
		xy_setx(rtn, i, vals[i*2]);
		xy_sety(rtn, i, vals[i*2 + 1]);
	}
	free(vals);
	return rtn;
}

qfits_header* xylist_get_field_header(xylist* ls, uint field) {
	qfits_header* hdr;
	hdr = qfits_header_readext(ls->fn, field+1);
	if (!hdr)
		fprintf(stderr, "Failed to read FITS header for extension %i in file %s.\n", field+1, ls->fn);
	return hdr;
}

int xylist_n_entries(xylist* ls, uint field) {
	if (xylist_find_field(ls, field)) {
		return -1;
	}
	return ls->table->nr;
}

int xylist_read_entries(xylist* ls, uint field, uint offset, uint n,
						double* vals) {
	double* ddata;
	float* fdata = NULL;
	uint i;
	if (xylist_find_field(ls, field)) {
		return -1;
	}
	ddata = vals + (ls->parity ? 1 : 0);
	if (ls->xtype == TFITS_BIN_TYPE_D) {
		qfits_query_column_seq_to_array(ls->table, ls->xcol, offset, n,
										(unsigned char*)ddata,
										2 * sizeof(double));
	} else if (ls->xtype == TFITS_BIN_TYPE_E) {
		fdata = malloc(n * sizeof(float));
		qfits_query_column_seq_to_array(ls->table, ls->xcol, offset, n,
										(unsigned char*)fdata,
										sizeof(float));
		for (i=0; i<n; i++)
			ddata[i*2] = fdata[i];
	}

	ddata = vals + (ls->parity ? 0 : 1);
	if (ls->ytype == TFITS_BIN_TYPE_D) {
		qfits_query_column_seq_to_array(ls->table, ls->ycol, offset, n,
										(unsigned char*)ddata,
										2 * sizeof(double));
	} else if (ls->ytype == TFITS_BIN_TYPE_E) {
		fdata = realloc(fdata, n * sizeof(float));
		qfits_query_column_seq_to_array(ls->table, ls->ycol, offset, n,
										(unsigned char*)fdata,
										sizeof(float));
		for (i=0; i<n; i++)
			ddata[i*2] = fdata[i];
	}

	free(fdata);
	return 0;
}

static qfits_table* xylist_get_table(xylist* ls) {
	uint ncols, nrows, tablesize;
	qfits_table* table;
	char* nil = " ";
	ncols = 2;
	nrows = 0;
	tablesize = 0;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	fits_add_column(table, 0, ls->xtype, 1, (ls->xunits ? ls->xunits : nil), ls->xname);
	fits_add_column(table, 1, ls->ytype, 1, (ls->yunits ? ls->yunits : nil), ls->yname);
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

xylist* xylist_open_for_writing(char* fn) {
	xylist* ls;

	ls = xylist_new();
	if (!ls)
		goto bailout;

	ls->fn = strdup(fn);
	ls->fid = fopen(fn, "wb");
	if (!ls->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	ls->antype = AN_FILETYPE_XYLS;
	ls->header = qfits_table_prim_header_default();
	qfits_header_add(ls->header, "NFIELDS", "0", NULL, NULL);
	qfits_header_add(ls->header, "AN_FILE", ls->antype, NULL, NULL);
	return ls;
 bailout:
	if (ls)
		free(ls);
	return NULL;
}

int xylist_write_header(xylist* ls) {
	assert(ls->fid);
	assert(ls->header);
	qfits_header_dump(ls->header, ls->fid);
	ls->data_offset = ftello(ls->fid);
	ls->table = xylist_get_table(ls);
	return 0;
}

int xylist_fix_header(xylist* ls) {
	off_t offset, datastart;
	char val[32];
	assert(ls->fid);
	assert(ls->header);
	offset = ftello(ls->fid);
	fseeko(ls->fid, 0, SEEK_SET);
	sprintf(val, "%u", ls->nfields);
	qfits_header_mod(ls->header, "NFIELDS", val, "Number of fields");
	qfits_header_mod(ls->header, "AN_FILE", ls->antype, "Astrometry.net file type");
	qfits_header_dump(ls->header, ls->fid);
	datastart = ftello(ls->fid);
	if (datastart != ls->data_offset) {
		fprintf(stderr, "Error: xylist header size changed: was %u, but is now %u.  Corruption is likely!\n",
				(uint)ls->data_offset, (uint)datastart);
		return -1;
	}
	fseeko(ls->fid, offset, SEEK_SET);
	return 0;
}

int xylist_write_new_field(xylist* ls) {
	char val[32];
	qfits_header* table_header;
	ls->table_offset = ftello(ls->fid);
	ls->table->nr = 0;
	table_header = qfits_table_ext_header_default(ls->table);
	sprintf(val, "%u", ls->nfields);
	qfits_header_add(table_header, "FIELDNUM", val, "This table is field number...", NULL);
	qfits_header_dump(table_header, ls->fid);
	qfits_header_destroy(table_header);
	ls->nfields++;
	return 0;
}

int xylist_write_entries(xylist* ls, double* vals, uint nvals) {
	uint i;
	for (i=0; i<nvals; i++) {
		if (ls->xtype == TFITS_BIN_TYPE_D) {
			if (fits_write_data_D(ls->fid, vals[i*2+0]))
				return -1;
		} else {
			if (fits_write_data_E(ls->fid, vals[i*2+0]))
				return -1;
		}
		if (ls->ytype == TFITS_BIN_TYPE_D) {
			if (fits_write_data_D(ls->fid, vals[i*2+1]))
				return -1;
		} else {
			if (fits_write_data_E(ls->fid, vals[i*2+1]))
				return -1;
		}
	}
	ls->table->nr += nvals;
	return 0;
}

int xylist_fix_field(xylist* ls) {
	off_t offset;
	char val[32];
	qfits_header* table_header;
	offset = ftello(ls->fid);
	fseeko(ls->fid, ls->table_offset, SEEK_SET);
	table_header = qfits_table_ext_header_default(ls->table);
	sprintf(val, "%u", ls->nfields - 1);
	qfits_header_add(table_header, "FIELDNUM", val, "This table is field number...", NULL);
	qfits_header_dump(table_header, ls->fid);
	qfits_header_destroy(table_header);
	fseeko(ls->fid, offset, SEEK_SET);
	fits_pad_file(ls->fid);
	return 0;
}

int xylist_close(xylist* ls) {
	if (ls->fid) {
		if (fclose(ls->fid)) {
			fprintf(stderr, "Failed to fclose xylist file %s: %s\n", ls->fn, strerror(errno));
			return -1;
		}
	}
	if (ls->table)
		qfits_table_close(ls->table);
	if (ls->header)
		qfits_header_destroy(ls->header);
	free(ls->fn);
	free(ls);
	return 0;
}

