#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

//#include "matchfile.h"
#include "matchfile_new.h"
#include "fitsioutils.h"
#include "ioutils.h"

matchfile* new_matchfile() {
	matchfile* mf = calloc(1, sizeof(matchfile));
	if (!mf) {
		fprintf(stderr, "Couldn't allocate a new matchfile.");
		return NULL;
	}
	return mf;
}

matchfile* matchfile_open_for_writing(char* fn) {
	matchfile* mf;

	mf = new_matchfile();
	if (!mf)
		goto bailout;
	mf->fid = fopen(fn, "wb");
	if (!mf->fid) {
		fprintf(stderr, "Couldn't open file %s for quad FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

    // the header
    mf->header = qfits_table_prim_header_default();
	/*
	  fits_add_endian(mf->header);
	  fits_add_uint_size(mf->header);
	*/

	qfits_header_add(mf->header, "AN_FILE", MATCHFILE_AN_FILETYPE,
					 "This is a list of quad matches.", NULL);
	return mf;

 bailout:
	if (mf) {
		if (mf->fid)
			fclose(mf->fid);
		free(mf);
	}
	return NULL;
}

int matchfile_write_header(matchfile* mf) {
	// the main file header is quite minimal...
    qfits_header_dump(mf->header, mf->fid);
	mf->header_end = ftello(mf->fid);
	return 0;
}

int matchfile_fix_header(matchfile* mf) {
	off_t offset;
	off_t new_header_end;
	assert(mf->fid);
	offset = ftello(mf->fid);
	fseeko(mf->fid, 0, SEEK_SET);
    qfits_header_dump(mf->header, mf->fid);
	new_header_end = ftello(mf->fid);
	if (new_header_end != mf->header_end) {
		fprintf(stderr, "Warning: matchfile header used to end at %lu, "
				"now it ends at %lu.\n", (unsigned long)mf->header_end,
				(unsigned long)new_header_end);
		return -1;
	}
	fseek(mf->fid, offset, SEEK_SET);
	fits_pad_file(mf->fid);
	return 0;
}

// mapping between a struct field and FITS field.
struct fits_struct_pair {
	char* fieldname;
	char* units;
	int offset;
	int size;
	int ncopies;
	tfits_type fitstype;
};
typedef struct fits_struct_pair fitstruct;

static fitstruct matchfile_fitstruct[MATCHFILE_FITS_COLUMNS];
static bool matchfile_fitstruct_inited = 0;

#define SET_FIELDS(A, i, t, n, u, fld, nc) { \
 MatchObj x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(MatchObj, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].ncopies=nc; \
 A[i].fitstype=t; \
 i++; \
}

static void init_matchfile_fitstruct() {
	fitstruct* fs = matchfile_fitstruct;
	int i = 0;
	char* nil = " ";

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "quad", nil, quadno, 1);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "stars", nil, star, 4);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "fieldobjs", nil, field, 4);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "codeerr", nil, code_err, 1);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "mincorner", nil, sMin, 3);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "maxcorner", nil, sMax, 3);

	assert(i == MATCHFILE_FITS_COLUMNS);
	matchfile_fitstruct_inited = 1;
}

static qfits_table* matchfile_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int col;
	qfits_table* table;

	if (!matchfile_fitstruct_inited)
		init_matchfile_fitstruct();
	// dummy values here...
	datasize = 0;
	ncols = MATCHFILE_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;

	for (col=0; col<MATCHFILE_FITS_COLUMNS; col++) {
		fitstruct* fs = matchfile_fitstruct + col;
		fits_add_column(table, col, fs->fitstype, 1, fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

int matchfile_write_table(matchfile* mf, matchfile_entry* me) {
	int tablesize, ncols, nrows;
	if  (mf->meheader) {
		qfits_header_destroy(mf->meheader);
	}
	tablesize = ncols = nrows = 0;
	qfits_table* table = matchfile_get_table();
	qfits_header* tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, mf->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);
	//mf->header_end = ftello(mf->fid);
	return 0;
}

int matchfile_fix_table(matchfile* m) {
	return 0;
}





