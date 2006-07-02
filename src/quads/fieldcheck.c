#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "fieldcheck.h"
#include "fitsioutils.h"
#include "ioutils.h"

static int find_table(fieldcheck_file* mf);
static qfits_table* fieldcheck_file_get_table();

fieldcheck_file* new_fieldcheck_file() {
	fieldcheck_file* mf = calloc(1, sizeof(fieldcheck_file));
	if (!mf) {
		fprintf(stderr, "Couldn't allocate a new fieldcheck_file.");
		return NULL;
	}
	return mf;
}

fieldcheck_file* fieldcheck_file_open_for_writing(char* fn) {
	fieldcheck_file* mf;

	mf = new_fieldcheck_file();
	if (!mf)
		goto bailout;
	mf->fid = fopen(fn, "wb");
	if (!mf->fid) {
		fprintf(stderr, "Couldn't open file %s for quad FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

    // the header
    mf->header = qfits_table_prim_header_default();
	qfits_header_add(mf->header, "AN_FILE", FIELDCHECK_AN_FILETYPE,
					 "This is a list of RDLS field summaries.", NULL);
	mf->table = fieldcheck_file_get_table();
	return mf;

 bailout:
	if (mf) {
		if (mf->fid)
			fclose(mf->fid);
		free(mf);
	}
	return NULL;
}

int fieldcheck_file_write_header(fieldcheck_file* mf) {
	qfits_header* tablehdr;
	// the main file header:
    qfits_header_dump(mf->header, mf->fid);
	mf->table->nr = mf->nrows;
	tablehdr = qfits_table_ext_header_default(mf->table);
    qfits_header_dump(tablehdr, mf->fid);
    qfits_header_destroy(tablehdr);
	mf->header_end = ftello(mf->fid);
	return 0;
}

int fieldcheck_file_fix_header(fieldcheck_file* mf) {
	off_t offset;
	off_t old_offset;
	offset = ftello(mf->fid);
	fseeko(mf->fid, 0, SEEK_SET);
	old_offset = mf->header_end;
	fieldcheck_file_write_header(mf);
	if (mf->header_end != old_offset) {
		fprintf(stderr, "Error: fieldcheck_file %s: header used to end at %i, but now it ends at %i.  Data corruction is likely to have resulted.\n",
				mf->fn, (int)old_offset, (int)mf->header_end);
		return -1;
	}
	fseeko(mf->fid, offset, SEEK_SET);
	return 0;
}

// mapping between a struct field and FITS field.
struct fits_struct_pair {
	char* fieldname;
	char* units;
	int offset;
	int size;
	tfits_type fitstype;
};
typedef struct fits_struct_pair fitstruct;

static fitstruct fieldcheck_file_fitstruct[FIELDCHECK_FITS_COLUMNS];
static int fieldcheck_file_fitstruct_inited = 0;

#define SET_FIELDS(A, i, t, n, u, fld) { \
 fieldcheck x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(fieldcheck, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].fitstype=t; \
 i++; \
}

static void init_fieldcheck_file_fitstruct() {
	fitstruct* fs = fieldcheck_file_fitstruct;
	int i = 0;
	char* nil = " ";

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "filenum", nil, filenum);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "fieldnum", nil, fieldnum);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "ra", "degrees", ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "dec", "degrees", dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "radius", "arcsec", radius);

	assert(i == FIELDCHECK_FITS_COLUMNS);
	fieldcheck_file_fitstruct_inited = 1;
}

static qfits_table* fieldcheck_file_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int col;
	qfits_table* table;

	if (!fieldcheck_file_fitstruct_inited)
		init_fieldcheck_file_fitstruct();
	// dummy values here...
	datasize = 0;
	ncols = FIELDCHECK_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;

	for (col=0; col<FIELDCHECK_FITS_COLUMNS; col++) {
		fitstruct* fs = fieldcheck_file_fitstruct + col;
		fits_add_column(table, col, fs->fitstype, 1, fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

int fieldcheck_file_write_entry(fieldcheck_file* mf, fieldcheck* fc) {
	int c;
	if (!fieldcheck_file_fitstruct_inited)
		init_fieldcheck_file_fitstruct();
	for (c=0; c<FIELDCHECK_FITS_COLUMNS; c++) {
		int atomsize;
		fitstruct* fs = fieldcheck_file_fitstruct + c;
		atomsize = fs->size;
		if (fits_write_data(mf->fid, ((unsigned char*)fc) + fs->offset, fs->fitstype)) {
			return -1;
		}
	}
	mf->nrows++;
	return 0;
}

int fieldcheck_file_close(fieldcheck_file* mf) {
	if (mf->fid) {
		fits_pad_file(mf->fid);
		if (fclose(mf->fid)) {
			fprintf(stderr, "Error closing fieldcheck_file FITS file: %s\n", strerror(errno));
			return -1;
		}
	}
	if (mf->table)
		qfits_table_close(mf->table);
	if (mf->header)
		qfits_header_destroy(mf->header);
	if (mf->fn)
		free(mf->fn);
	free(mf);
	return 0;
}

fieldcheck_file* fieldcheck_file_open(char* fn) {
	fieldcheck_file* mf = NULL;
	qfits_header* header;
	FILE* fid;

	if (!fieldcheck_file_fitstruct_inited)
		init_fieldcheck_file_fitstruct();
	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read matches: %s\n", fn, strerror(errno));
		return NULL;
	}
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS quad header from %s.\n", fn);
		goto bailout;
	}

	mf = new_fieldcheck_file();
	mf->fid = fid;
	mf->header = header;
	mf->fn = fn;
	if (find_table(mf)) {
		fprintf(stderr, "Couldn't find an appropriate FITS table in %s.\n", fn);
		goto bailout;
	}
	mf->nrows = mf->table->nr;
	mf->fn = strdup(fn);
	return mf;

 bailout:
	if (mf)
		free(mf);
	if (fid)
		fclose(fid);
	return NULL;
}

static int find_table(fieldcheck_file* mf) {
	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	qfits_table* table;
	int good = 0;
	int c;
	int nextens = qfits_query_n_ext(mf->fn);
	int ext;

	for (ext=1; ext<=nextens; ext++) {
		int c2;
		if (!qfits_is_table(mf->fn, ext)) {
			fprintf(stderr, "fieldcheck_file: extention %i isn't a table.\n", ext);
			continue;
		}
		table = qfits_table_open(mf->fn, ext);
		if (!table) {
			fprintf(stderr, "fieldcheck_file: failed to open table: file %s, extension %i.\n", mf->fn, ext);
			return -1;
		}
		good = 1;
		for (c=0; c<FIELDCHECK_FITS_COLUMNS; c++) {
			mf->columns[c] = -1;
			for (c2=0; c2<table->nc; c2++) {
				qfits_col* col = table->col + c2;
				// allow case-insensitive matches.
				if (strcasecmp(col->tlabel, fieldcheck_file_fitstruct[c].fieldname))
					continue;
				mf->columns[c] = c2;
				break;
			}
			if (mf->columns[c] == -1) {
				fprintf(stderr, "fieldcheck_file: didn't find column \"%s\"", fieldcheck_file_fitstruct[c].fieldname);
				good = 0;
				break;
			}
		}
		if (good) {
			mf->table = table;
			break;
		}
		qfits_table_close(table);
	}
	if (!good) {
		fprintf(stderr, "fieldcheck_file: didn't find the following required columns:\n    ");
		for (c=0; c<FIELDCHECK_FITS_COLUMNS; c++)
			if (mf->columns[c] == -1)
				fprintf(stderr, "%s  ", fieldcheck_file_fitstruct[c].fieldname);
		fprintf(stderr, "\n");
		return -1;
	}
	mf->nrows = mf->table->nr;
	return 0;
}

int fieldcheck_file_read_entries(fieldcheck_file* mf, fieldcheck* fc, 
								 uint offset, uint n) {
	int c;
	if (!fieldcheck_file_fitstruct_inited)
		init_fieldcheck_file_fitstruct();
	
	for (c=0; c<FIELDCHECK_FITS_COLUMNS; c++) {
		if (mf->columns[c] == -1)
			continue;
		assert(mf->table);
		assert(mf->table->col[mf->columns[c]].atom_size ==
			   fieldcheck_file_fitstruct[c].size / fieldcheck_file_fitstruct[c].ncopies);

		qfits_query_column_seq_to_array
			(mf->table, mf->columns[c], offset, n,
			 ((unsigned char*)fc) + fieldcheck_file_fitstruct[c].offset,
			 sizeof(fieldcheck));
	}
	return 0;
}

