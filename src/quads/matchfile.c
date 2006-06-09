#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "matchfile.h"
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
	fits_pad_file(mf->fid);
	return 0;
}

/*
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
*/

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
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "mincorner", nil, sMin, 3);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "maxcorner", nil, sMax, 3);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "noverlap", nil, noverlap, 1);

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

int matchfile_start_table(matchfile* mf, matchfile_entry* me) {
	char val[64];
	if (mf->table)
		qfits_table_close(mf->table);
	if (mf->tableheader)
		qfits_header_destroy(mf->tableheader);
	mf->nrows = 0;
	mf->table = matchfile_get_table();
	mf->tableheader = qfits_table_ext_header_default(mf->table);

	sprintf(val, "%u", me->fieldnum);
	qfits_header_add(mf->tableheader, "FIELD", val, "Field number.", NULL);
	sprintf(val, "%g", me->codetol);
	qfits_header_add(mf->tableheader, "CODETOL", val, "Code match tolerance.", NULL);
	qfits_header_add(mf->tableheader, "PARITY", (me->parity ? "Y" : "N"),
					 "Were field coordinates flipped?", NULL);
	qfits_header_add(mf->tableheader, "INDEX", me->indexpath ? me->indexpath : "", " ", NULL);
	qfits_header_add(mf->tableheader, "", NULL, "Path of the index used", NULL);
	qfits_header_add(mf->tableheader, "FLDPATH", me->fieldpath ? me->fieldpath : "", " ", NULL);
	qfits_header_add(mf->tableheader, "", NULL, "Path of the field file", NULL);
	return 0;
}

int matchfile_write_table(matchfile* mf) {
	assert(mf->tableheader);
	mf->tableheader_start = ftello(mf->fid);
	qfits_header_dump(mf->tableheader, mf->fid);
	mf->tableheader_end = ftello(mf->fid);
	return 0;
}

int matchfile_fix_table(matchfile* mf) {
	int i;
	qfits_header* newhdr;
	off_t off;
	off_t newoff;

	assert(mf->table);
	assert(mf->tableheader);

	// update the number of rows in the table
	mf->table->nr = mf->nrows;
	// get the new table header
	newhdr = qfits_table_ext_header_default(mf->table);
	// update any of the header fields that have changed.
	for (i=0; i<newhdr->n; i++) {
		char key[FITS_LINESZ+1];
		char val[FITS_LINESZ+1];
		char com[FITS_LINESZ+1];
		qfits_header_getitem(newhdr, i, key, val, com, NULL);
		qfits_header_mod(mf->tableheader, key, val, com);
	}
	off = ftello(mf->fid);
	fseeko(mf->fid, mf->tableheader_start, SEEK_SET);
	qfits_header_dump(mf->tableheader, mf->fid);
	newoff = ftello(mf->fid);
	if (newoff != mf->tableheader_end) {
		fprintf(stderr, "Error: matchfile table header used to end at file offset %u, "
				"now it ends at %u: data corruption is possible (and likely!).\n",
				(uint)mf->tableheader_end, (uint)newoff);
		return -1;
	}
	// back to where we started!
	fseeko(mf->fid, off, SEEK_SET);
	// add padding!
	fits_pad_file(mf->fid);
	fdatasync(fileno(mf->fid));
	return 0;
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
	if (mf->tableheader)
		qfits_header_destroy(mf->tableheader);
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
	mf->extension = 0;
	mf->fn = strdup(fn);
	return mf;

 bailout:
	if (mf)
		free(mf);
	if (fid)
		fclose(fid);
	return NULL;
}

int matchfile_next_table(matchfile* mf, matchfile_entry* me) {
	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	qfits_table* table;
	int good = 0;
	int c;
	int nextens = qfits_query_n_ext(mf->fn);
	if (mf->table) {
		qfits_table_close(mf->table);
		mf->table = NULL;
	}
	if (mf->tableheader) {
		qfits_header_destroy(mf->tableheader);
		mf->tableheader = NULL;
	}

	mf->extension++;
	if (mf->extension > nextens) {
		return 1;
	}

	for (; mf->extension<=nextens; mf->extension++) {
		int c2;
		if (!qfits_is_table(mf->fn, mf->extension))
			continue;
		table = qfits_table_open(mf->fn, mf->extension);
		if (!table) {
			fprintf(stderr, "matchfile: failed to open table: file %s, extension %i.\n",
					mf->fn, mf->extension);
			return -1;
		}

		for (c=0; c<MATCHFILE_FITS_COLUMNS; c++)
			mf->columns[c] = -1;
		for (c=0; c<table->nc; c++) {
			qfits_col* col = table->col + c;
			for (c2=0; c2<MATCHFILE_FITS_COLUMNS; c2++) {
				if (mf->columns[c2] != -1) continue;
				// allow case-insensitive matches.
				if (strcasecmp(col->tlabel, matchfile_fitstruct[c2].fieldname))
					continue;
				mf->columns[c2] = c;
			}
		}
		good = 1;
		for (c=0; c<MATCHFILE_FITS_COLUMNS; c++) {
			if (mf->columns[c] == -1) {
				good = 0;
				break;
			}
		}
		if (good) {
			mf->table = table;
			mf->tableheader = qfits_header_readext(mf->fn, mf->extension);
			break;
		}
		qfits_table_close(table);
	}
	if (!good) {
		fprintf(stderr, "matchfile: didn't find the following required columns:\n    ");
		for (c=0; c<MATCHFILE_FITS_COLUMNS; c++)
			if (mf->columns[c] == -1)
				fprintf(stderr, "%s  ", matchfile_fitstruct[c].fieldname);
		fprintf(stderr, "\n");
		return -1;
	}
	mf->nrows = mf->table->nr;
	// reset buffered reading data
	mf->nbuff = mf->off = mf->buffind = 0;

	me->fieldnum = qfits_header_getint(mf->tableheader, "FIELD", -1);
	me->codetol = qfits_header_getdouble(mf->tableheader, "CODETOL", 0.0);
	me->parity = qfits_header_getboolean(mf->tableheader, "PARITY", 0);
	me->indexpath = strdup(qfits_header_getstr(mf->tableheader, "INDEX"));
	me->fieldpath = strdup(qfits_header_getstr(mf->tableheader, "FLDPATH"));

	return 0;
}

int matchfile_read_matches(matchfile* mf, MatchObj* mo, 
						   uint offset, uint n) {
	int c;

	if (!matchfile_fitstruct_inited)
		init_matchfile_fitstruct();

	for (c=0; c<MATCHFILE_FITS_COLUMNS; c++) {
		assert(mf->columns[c] != -1);
		assert(mf->table);
		assert(mf->table->col[mf->columns[c]].atom_size ==
			   matchfile_fitstruct[c].size / matchfile_fitstruct[c].ncopies);

		qfits_query_column_seq_to_array
			(mf->table, mf->columns[c], offset, n,
			 ((unsigned char*)mo) + matchfile_fitstruct[c].offset,
			 sizeof(MatchObj));
	}
	return 0;
}

MatchObj* matchfile_buffered_read_match(matchfile* mf) {
	int BLOCK = 1000;
	MatchObj* mo;

	if (!mf->buffer) {
		mf->buffer = malloc(BLOCK * sizeof(MatchObj));
		mf->nbuff = mf->off = mf->buffind = 0;
	}
	if (mf->buffind == mf->nbuff) {
		memset(mf->buffer, 0, BLOCK * sizeof(MatchObj));
		// read a new block!
		uint n = BLOCK;
		// the new block to read starts after the current block...
		mf->off += mf->nbuff;
		fprintf(stderr, "off=%i, n=%i, nrows=%i\n", mf->off, n, mf->nrows);
		if (n + mf->off > mf->nrows)
			n = mf->nrows - mf->off;
		fprintf(stderr, "n=%i\n", n);
		if (!n) {
			fprintf(stderr, "matchfile_buffered_read_match: matchfile contains no more matches.\n");
			return NULL;
		}
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
