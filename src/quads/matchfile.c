#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "matchfile.h"
#include "fitsioutils.h"
#include "ioutils.h"

static int find_table(matchfile* mf);
static qfits_table* matchfile_get_table();

void matchobj_compute_derived(MatchObj* mo) {
	int mx;
	int i;
	matchobj_compute_overlap(mo);
	mx = 0;
	for (i=0; i<4; i++)
		if (mo->field[i] > mx) mx = mo->field[i];
	mo->objs_tried = mx+1;
}

void matchobj_compute_overlap(MatchObj* mo) {
	if (!mo->ninfield) {
		fprintf(stderr, "Warning: matchobj_compute_overlap: ninfield = 0.\n");
		mo->overlap = 0.0;
		return;
	}
	if (mo->ninfield < 4) {
		mo->overlap = 0.0;
		return;
	}
	// at least four should match since the transformation is calculated
	// based on projecting one quad...
	mo->overlap = (mo->noverlap - 4) / (double)(mo->ninfield - 4);
	if (mo->overlap < 0.0)
		mo->overlap = 0.0;
}

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
	qfits_header_add(mf->header, "AN_FILE", MATCHFILE_AN_FILETYPE,
					 "This is a list of quad matches.", NULL);
	mf->table = matchfile_get_table();
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
	qfits_header* tablehdr;
	// the main file header:
    qfits_header_dump(mf->header, mf->fid);
	mf->table->nr = mf->nrows;
	tablehdr = qfits_table_ext_header_default(mf->table);
    qfits_header_dump(tablehdr, mf->fid);
    qfits_header_destroy(tablehdr);
	mf->header_end = ftello(mf->fid);
	//fits_pad_file(mf->fid);
	return 0;
}

int matchfile_fix_header(matchfile* mf) {
	off_t offset;
	off_t old_offset;
	offset = ftello(mf->fid);
	fseeko(mf->fid, 0, SEEK_SET);
	old_offset = mf->header_end;
	matchfile_write_header(mf);
	if (mf->header_end != old_offset) {
		fprintf(stderr, "Error: matchfile %s: header used to end at %i, but now it ends at %i.  Data corruction is likely to have resulted.\n",
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
	int ncopies;
	tfits_type fitstype;
	bool required;
};
typedef struct fits_struct_pair fitstruct;

static fitstruct matchfile_fitstruct[MATCHFILE_FITS_COLUMNS];
static bool matchfile_fitstruct_inited = 0;

#define SET_FIELDS(A, i, t, n, u, fld, nc, req) { \
 MatchObj x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(MatchObj, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].ncopies=nc; \
 A[i].fitstype=t; \
 A[i].required = req; \
 i++; \
}

static void init_matchfile_fitstruct() {
	fitstruct* fs = matchfile_fitstruct;
	int i = 0;
	char* nil = " ";

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "quad", nil, quadno, 1, TRUE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "stars", nil, star, 4, TRUE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "fieldobjs", nil, field, 4, TRUE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "codeerr", nil, code_err, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "mincorner", nil, sMin, 3, TRUE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "maxcorner", nil, sMax, 3, TRUE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "noverlap", nil, noverlap, 1, TRUE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "nconflict", nil, nconflict, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "ninfield", nil, ninfield, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "fieldnum", nil, fieldnum, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "fieldid", nil, fieldfile, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "indexid", nil, indexid, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "healpix", nil, healpix, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_X, "parity", nil, parity, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "qtried", nil, quads_tried, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "qmatched", nil, quads_matched, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "qscaleok", nil, quads_scaleok, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "nverified", nil, nverified, 1, FALSE);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "timeused", nil, timeused, 1, FALSE);

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
		fits_add_column(table, col, fs->fitstype, fs->ncopies,
						fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

int matchfile_write_match(matchfile* mf, MatchObj* mo) {
	int c;
	if (!matchfile_fitstruct_inited)
		init_matchfile_fitstruct();
	for (c=0; c<MATCHFILE_FITS_COLUMNS; c++) {
		int i;
		int atomsize;
		fitstruct* fs = matchfile_fitstruct + c;
		atomsize = fs->size / fs->ncopies;
		for (i=0; i<fs->ncopies; i++) {
			if (fits_write_data(mf->fid, ((unsigned char*)mo) + fs->offset + (i * atomsize), fs->fitstype)) {
				return -1;
			}
		}
	}
	mf->nrows++;
	return 0;
}

int matchfile_close(matchfile* mf) {
	if (mf->fid) {
		fits_pad_file(mf->fid);
		if (fclose(mf->fid)) {
			fprintf(stderr, "Error closing matchfile FITS file: %s\n", strerror(errno));
			return -1;
		}
	}
	if (mf->table)
		qfits_table_close(mf->table);
	if (mf->header)
		qfits_header_destroy(mf->header);
	if (mf->fn)
		free(mf->fn);
	if (mf->buffer)
		free(mf->buffer);

	free(mf);
	return 0;
}

matchfile* matchfile_open(char* fn) {
	matchfile* mf = NULL;
	qfits_header* header;
	FILE* fid;

	if (!matchfile_fitstruct_inited)
		init_matchfile_fitstruct();
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

	mf = new_matchfile();
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

static int find_table(matchfile* mf) {
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
			fprintf(stderr, "matchfile: extention %i isn't a table.\n", ext);
			continue;
		}
		table = qfits_table_open(mf->fn, ext);
		if (!table) {
			fprintf(stderr, "matchfile: failed to open table: file %s, extension %i.\n", mf->fn, ext);
			return -1;
		}
		good = 1;
		for (c=0; c<MATCHFILE_FITS_COLUMNS; c++) {
			mf->columns[c] = -1;
			for (c2=0; c2<table->nc; c2++) {
				qfits_col* col = table->col + c2;
				// allow case-insensitive matches.
				if (strcasecmp(col->tlabel, matchfile_fitstruct[c].fieldname))
					continue;
				mf->columns[c] = c2;
				break;
			}
			if (matchfile_fitstruct[c].required && (mf->columns[c] == -1)) {
				fprintf(stderr, "matchfile: didn't find column \"%s\"", matchfile_fitstruct[c].fieldname);
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
		fprintf(stderr, "matchfile: didn't find the following required columns:\n    ");
		for (c=0; c<MATCHFILE_FITS_COLUMNS; c++)
			if (matchfile_fitstruct[c].required &&
				(mf->columns[c] == -1))
				fprintf(stderr, "%s  ", matchfile_fitstruct[c].fieldname);
		fprintf(stderr, "\n");
		return -1;
	}
	mf->nrows = mf->table->nr;
	// reset buffered reading data
	mf->nbuff = mf->off = mf->buffind = 0;
	return 0;
}

int matchfile_read_matches(matchfile* mf, MatchObj* mo, 
						   uint offset, uint n) {
	int c;
	int i;
	if (!matchfile_fitstruct_inited)
		init_matchfile_fitstruct();

	for (c=0; c<MATCHFILE_FITS_COLUMNS; c++) {
		if (mf->columns[c] == -1)
			continue;
		assert(mf->table);
		assert(mf->table->col[mf->columns[c]].atom_size ==
			   matchfile_fitstruct[c].size / matchfile_fitstruct[c].ncopies);

		qfits_query_column_seq_to_array
			(mf->table, mf->columns[c], offset, n,
			 ((unsigned char*)mo) + matchfile_fitstruct[c].offset,
			 sizeof(MatchObj));
	}
	for (i=0; i<n; i++)
		matchobj_compute_derived(mo + i);
	return 0;
}

pl* matchfile_get_matches_for_field(matchfile* mf, uint field) {
	pl* list = pl_new(256);
	for (;;) {
		MatchObj* mo = matchfile_buffered_read_match(mf);
		MatchObj* copy;
		if (!mo) break;
		if (mo->fieldnum != field) {
			// push back the newly-read entry...
			matchfile_buffered_read_pushback(mf);
			break;
		}
		copy = malloc(sizeof(MatchObj));
		memcpy(copy, mo, sizeof(MatchObj));
		pl_append(list, copy);
	}
	return list;
}

MatchObj* matchfile_buffered_read_match(matchfile* mf) {
	const int BLOCK = 1000;
	MatchObj* mo;

	if (!mf->buffer) {
		mf->buffer = malloc(BLOCK * sizeof(MatchObj));
		mf->nbuff = mf->off = mf->buffind = 0;
	}
	if (mf->buffind == mf->nbuff) {
		// read a new block!
		uint n = BLOCK;
		// the new block to read starts after the current block...
		mf->off += mf->nbuff;
		if (n + mf->off > mf->nrows)
			n = mf->nrows - mf->off;
		if (!n) {
			//fprintf(stderr, "matchfile_buffered_read_match: matchfile contains no more matches.\n");
			return NULL;
		}
		memset(mf->buffer, 0, BLOCK * sizeof(MatchObj));
		if (matchfile_read_matches(mf, mf->buffer, mf->off, n)) {
			fprintf(stderr, "matchfile_buffered_read_match: Error filling buffer!\n");
			return NULL;
		}
		mf->nbuff = n;
		mf->buffind = 0;
	}

	mo = mf->buffer + mf->buffind;
	mf->buffind++;
	return mo;
}

int matchfile_buffered_read_pushback(matchfile* mf) {
	if (!mf->buffind)
		return -1;
	mf->buffind--;
	return 0;
}

