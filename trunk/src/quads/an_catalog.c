#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "an_catalog.h"
#include "fitsioutils.h"
#include "starutil.h"

static qfits_table* an_catalog_get_table();
static an_catalog* an_catalog_new();

// mapping between a struct field and FITS field.
struct fits_struct_pair {
	char* fieldname;
	char* units;
	int offset;
	int size;
	tfits_type fitstype;
};
typedef struct fits_struct_pair fitstruct;

static fitstruct an_fitstruct[AN_FITS_COLUMNS];
static bool an_fitstruct_inited = 0;

#define SET_FIELDS(A, i, t, n, u, fld) { \
 an_entry x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(an_entry, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].fitstype=t; \
 i++; \
}

static void init_an_fitstruct() {
	fitstruct* fs = an_fitstruct;
	int i = 0, ob;
	char* nil = " ";

 	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "RA",  "degrees", ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "DEC", "degrees", dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MOTION_RA",   "mas/yr", motion_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MOTION_DEC",  "mas/yr", motion_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_RA",   "mas", sigma_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_DEC",  "mas", sigma_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MOTION_RA",   "mas", sigma_motion_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MOTION_DEC",  "mas", sigma_motion_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "NOBSERVATIONS", nil, nobs);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_K, "ID", nil, id);

	for (ob=0; ob<AN_N_OBSERVATIONS; ob++) {
		char fld[32];
		// note, we memleak these strings because we can't share the static
		// char array.
		sprintf(fld, "CATALOG_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, strdup(fld), nil, obs[ob].catalog);
		sprintf(fld, "BAND_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, strdup(fld), nil, obs[ob].band);
		sprintf(fld, "ID_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, strdup(fld), nil, obs[ob].id);
		sprintf(fld, "MAG_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, strdup(fld), nil, obs[ob].mag);
		sprintf(fld, "SIGMA_MAG_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, strdup(fld), nil, obs[ob].sigma_mag);
	}
	assert(i == AN_FITS_COLUMNS);
	an_fitstruct_inited = 1;
}

int an_catalog_read_entries(an_catalog* cat, uint offset,
							uint count, an_entry* entries) {
	int c;

	if (!an_fitstruct_inited)
		init_an_fitstruct();

	for (c=0; c<AN_FITS_COLUMNS; c++) {
		assert(cat->columns[c] != -1);
		assert(cat->table);
		assert(cat->table->col[cat->columns[c]].atom_size == an_fitstruct[c].size);

		qfits_query_column_seq_to_array
			(cat->table, cat->columns[c], offset, count,
			 ((unsigned char*)entries) + an_fitstruct[c].offset,
			 sizeof(an_entry));
	}
	return 0;
}

int an_catalog_write_entry(an_catalog* cat, an_entry* entry) {
	int c;
	FILE* fid = cat->fid;

	if (!an_fitstruct_inited)
		init_an_fitstruct();

	for (c=0; c<AN_FITS_COLUMNS; c++) {
		fitstruct* fs = an_fitstruct + c;
		if (fits_write_data(fid, ((unsigned char*)entry) + fs->offset,
							fs->fitstype)) {
			return -1;
		}
	}
	cat->nentries++;
	return 0;
}

int an_catalog_count_entries(an_catalog* cat) {
	return cat->table->nr;
}

int an_catalog_close(an_catalog* cat) {
	if (cat->fid) {
		fits_pad_file(cat->fid);
		if (fclose(cat->fid)) {
			fprintf(stderr, "Error closing AN FITS file: %s\n", strerror(errno));
			return -1;
		}
	}
	if (cat->table) {
		qfits_table_close(cat->table);
	}
	if (cat->header) {
		qfits_header_destroy(cat->header);
	}
	free(cat);
	return 0;
}

an_catalog* an_catalog_open(char* fn) {
	int i, nextens;
	qfits_table* table;
	int c;
	an_catalog* cat = NULL;
	int good = 0;

	if (!an_fitstruct_inited)
		init_an_fitstruct();

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	cat = an_catalog_new();

	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
		int c2;
		if (!qfits_is_table(fn, i))
			continue;
		table = qfits_table_open(fn, i);

		for (c=0; c<AN_FITS_COLUMNS; c++)
			cat->columns[c] = -1;

		for (c=0; c<table->nc; c++) {
			qfits_col* col = table->col + c;
			for (c2=0; c2<AN_FITS_COLUMNS; c2++) {
				if (cat->columns[c2] != -1) continue;
				// allow case-insensitive matches.
				if (strcasecmp(col->tlabel, an_fitstruct[c2].fieldname))
					continue;
				cat->columns[c2] = c;
			}
		}

		good = 1;
		for (c=0; c<AN_FITS_COLUMNS; c++) {
			if (cat->columns[c] == -1) {
				good = 0;
				break;
			}
		}
		if (good) {
			cat->table = table;
			break;
		}
		qfits_table_close(table);
	}
	if (!good) {
		fprintf(stderr, "an_catalog: didn't find the following required columns:\n    ");
		for (c=0; c<AN_FITS_COLUMNS; c++)
			if (cat->columns[c] == -1)
				fprintf(stderr, "%s  ", an_fitstruct[c].fieldname);
		fprintf(stderr, "\n");
		free(cat);
		return NULL;
	}
	cat->nentries = cat->table->nr;
	return cat;
}

an_catalog* an_catalog_open_for_writing(char* fn) {
	an_catalog* cat;
	cat = an_catalog_new();
	cat->fid = fopen(fn, "wb");
	if (!cat->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	cat->table = an_catalog_get_table();
	cat->header = qfits_table_prim_header_default();
	qfits_header_add(cat->header, "AN_CAT", "T", "This is an Astrometry.net catalog.", NULL);
	qfits_header_add(cat->header, "NOBJS", "0", "(filler)", NULL);
	return cat;
 bailout:
	if (cat) {
		free(cat);
	}
	return NULL;
}

int an_catalog_write_headers(an_catalog* cat) {
	qfits_header* table_header;
	char val[32];
	assert(cat->fid);
	assert(cat->header);
	sprintf(val, "%u", cat->nentries);
	qfits_header_mod(cat->header, "NOBJS", val, "Number of objects in this catalog.");
	qfits_header_dump(cat->header, cat->fid);
	table_header = qfits_table_ext_header_default(cat->table);
	qfits_header_dump(table_header, cat->fid);
	qfits_header_destroy(table_header);
	cat->header_end = ftello(cat->fid);
	return 0;
}

int an_catalog_fix_headers(an_catalog* cat) {
 	off_t offset;
	off_t old_header_end;

	if (!cat->fid) {
		fprintf(stderr, "an_catalog_fix_headers: fid is null.\n");
		return -1;
	}
	offset = ftello(cat->fid);
	fseeko(cat->fid, 0, SEEK_SET);
	old_header_end = cat->header_end;

	an_catalog_write_headers(cat);

	if (old_header_end != cat->header_end) {
		fprintf(stderr, "Warning: an_catalog header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)old_header_end,
				(unsigned long)cat->header_end);
		return -1;
	}
	fseek(cat->fid, offset, SEEK_SET);
	return 0;
}

an_catalog* an_catalog_new() {
	an_catalog* rtn = malloc(sizeof(an_catalog));
	if (!rtn) {
		fprintf(stderr, "Couldn't allocate memory for a an_catalog structure.\n");
		exit(-1);
	}
	memset(rtn, 0, sizeof(an_catalog));
	return rtn;
}

static qfits_table* an_catalog_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int col;
	qfits_table* table;

	if (!an_fitstruct_inited)
		init_an_fitstruct();

	// one big table: the sources.
	// dummy values here...
	datasize = 0;
	ncols = AN_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;
	for (col=0; col<AN_FITS_COLUMNS; col++) {
		fitstruct* fs = an_fitstruct + col;
		fits_add_column(table, col, fs->fitstype, 1, fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

int64_t an_catalog_get_id(int catversion, int64_t starid) {
	int64_t mask = 0x000000ffffffffffLL;
	int64_t catv = catversion;
	int64_t id;
	catv = (catv << 48);
	assert((catv & (~mask)) == 0);
	id = (starid & mask) | catv;
	return id;
}



