#include <string.h>
#include <errno.h>
#include <assert.h>

#include "xylist.h"
#include "fitsioutils.h"

xylist* xylist_open(char* fn) {
	xylist* ls = NULL;
	qfits_header* header;

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	ls = calloc(1, sizeof(xylist));
	if (!ls) {
		fprintf(stderr, "Couldn't allocate an xylist.\n");
		return NULL;
	}

	ls->fn = strdup(fn);
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS header from xylist %s.\n", fn);
        goto bailout;
	}
	ls->nfields = qfits_header_getint(header, "NFIELDS", -1);
	qfits_header_destroy(header);
	if (ls->nfields == -1) {
		fprintf(stderr, "Didn't find the NFIELDS header field in xylist file %s\n", fn);
		goto bailout;
	}

	if (qfits_query_n_ext(fn) <= ls->nfields) {
		fprintf(stderr, "xylist file %s supposedly has %u fields, but it only has %u FITS extensions.\n", fn,
				ls->nfields, qfits_query_n_ext(fn));
		goto bailout;
	}
	return ls;

 bailout:
	if (ls && ls->fn)
		free(ls->fn);
	if (ls)
		free(ls);
	return NULL;
}

static int xylist_find_field(xylist* ls, uint field) {
	int c;
	if (ls->table && ls->field == field)
		return 0;
	// the main FITS header is exteniosn 1
	// the first FITS table is extension 2
	if (ls->table) {
		qfits_table_close(ls->table);
		ls->table = NULL;
	}
	ls->table = qfits_table_open(ls->fn, field + 2);
	if (!ls->table) {
		fprintf(stderr, "FITS extension %i in file %s is not a table.\n",
				field+2, ls->fn);
		return -1;
	}
	// find the right column.
	ls->colnum = -1;
	for (c=0; c<ls->table->nc; c++) {
		qfits_col* col = ls->table->col + c;
		if ((strcasecmp(col->tlabel, "XY") == 0) &&
			(col->atom_type == TFITS_BIN_TYPE_D) &&
			(col->atom_nb == 2)) {
			ls->colnum = c;
			break;
		}
	}
	if (ls->colnum == -1) {
		fprintf(stderr, "xylist file %s: field %u: no column named \"XY\", type D(ouble) x 2.\n",
				ls->fn, field);
		return -1;
	}
	ls->field = field;
	return 0;
}

xy* xylist_get_field(xylist* ls, uint field) {
	int i, nobjs;
	double* vals;
	xy* rtn;
	nobjs = xylist_n_entries(ls, field);
	if (nobjs == -1) {
		fprintf(stderr, "Field %i couldn't be read.", field);
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

int xylist_n_entries(xylist* ls, uint field) {
	if (xylist_find_field(ls, field)) {
		return -1;
	}
	return ls->table->nr;
}

int xylist_read_entries(xylist* ls, uint field, uint offset, uint n,
						double* vals) {
	double* data;
	if (xylist_find_field(ls, field)) {
		return -1;
	}
	data = (double*)qfits_query_column_seq(ls->table, 0, offset, n);
	if (!data) {
		fprintf(stderr, "Failed to read data from xylist file %s, field "
				"%u, offset %u, n %u.\n", ls->fn, field, offset, n);
		return -1;
	}
	if (!ls->parity) {
		memcpy(vals, data, n * 2 * sizeof(double));
	} else {
		int i;
		for (i=0; i<n; i++) {
			vals[i*2 + 1] = data[i*2 + 0];
			vals[i*2 + 0] = data[i*2 + 1];
		}
	}
	free(data);
	return 0;
}

static qfits_table* xylist_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	qfits_table* table;
	char* nil = " ";
	datasize = 2 * sizeof(double);
	ncols = 1;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	fits_add_column(table, 0, TFITS_BIN_TYPE_D, 2, nil, "XY");
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

xylist* xylist_open_for_writing(char* fn) {
	xylist* ls;

	ls = calloc(1, sizeof(xylist));
	if (!ls) {
		fprintf(stderr, "Couldn't allocate memory for an xylist.\n");
		goto bailout;
	}
	ls->fn = strdup(fn);
	ls->fid = fopen(fn, "wb");
	if (!ls->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	ls->table = xylist_get_table();
	ls->header = qfits_table_prim_header_default();
	qfits_header_add(ls->header, "NFIELDS", "0", NULL, NULL);
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
	qfits_header_add(ls->header, "FIELDNUM", val, "This table is field number...", NULL);
	qfits_header_dump(table_header, ls->fid);
	qfits_header_destroy(table_header);
	ls->nfields++;
	return 0;
}

int xylist_write_entries(xylist* ls, double* vals, uint nvals) {
	uint i;
	for (i=0; i<nvals * 2; i++) {
		if (fits_write_data_D(ls->fid, vals[i])) {
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
	qfits_header_add(ls->header, "FIELDNUM", val, "This table is field number...", NULL);
	qfits_header_dump(table_header, ls->fid);
	qfits_header_destroy(table_header);
	fseeko(ls->fid, offset, SEEK_SET);
	fits_pad_file(ls->fid);
	return 0;
}

void xylist_close(xylist* ls) {
	if (ls->fid) {
		if (fclose(ls->fid)) {
			fprintf(stderr, "Failed to fclose xylist file %s\n", ls->fn);
		}
	}
	if (ls->table) {
		qfits_table_close(ls->table);
	}
	if (ls->fn) {
		free(ls->fn);
	}
	free(ls);
}

