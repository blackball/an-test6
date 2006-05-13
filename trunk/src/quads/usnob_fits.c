#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "usnob_fits.h"
#include "fitsioutils.h"

static qfits_table* usnob_fits_get_table();
static usnob_fits* usnob_fits_new();

// mapping between a struct field and FITS field.
struct fits_struct_pair {
	char* fieldname;
	char* units;
	int offset;
	int size;
	tfits_type fitstype;
};
typedef struct fits_struct_pair fitstruct;

static fitstruct usnob_fitstruct[USNOB_FITS_COLUMNS];
static bool usnob_fitstruct_inited = 0;
static int USNOB_FLAGS_INDEX;

#define SET_FIELDS(A, i, t, n, u, fld) { \
 usnob_entry x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(usnob_entry, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].fitstype=t; \
 i++; \
}

static void init_usnob_fitstruct() {
	fitstruct* fs = usnob_fitstruct;
	int i = 0, ob;
	char* nil = " ";

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "RA",  "degrees", ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "DEC", "degrees", dec);
	USNOB_FLAGS_INDEX = i;
	fs[i].fieldname = "FLAGS";
	fs[i].units = nil;
	fs[i].offset = -1;
	fs[i].size = 0;
	fs[i].fitstype = TFITS_BIN_TYPE_UNKNOWN;
	i++;
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_RA",  "arcsec", sigma_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_DEC", "arcsec", sigma_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_RA_FIT",  "arcsec", sigma_ra_fit);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_DEC_FIT", "arcsec", sigma_dec_fit);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH", "yr", epoch);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MU_PROBABILITY", nil, mu_prob);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MU_RA",  "arcsec/yr", mu_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MU_DEC", "arcsec/yr", mu_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MU_RA",  "arcsec/yr", sigma_mu_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MU_DEC", "arcsec/yr", sigma_mu_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "NUM_DETECTIONS", nil, ndetections);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "USNOB_ID", nil, usnob_id);

	for (ob=0; ob<5; ob++) {
		// note, we will leak the strdup'd memory, because the string value isn't constant
		// and we can't share it.  this only gets called once and it's a couple of hundred bytes,
		// no big deal.
		char field[256];
		sprintf(field, "MAGNITUDE_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, strdup(field), "mag", obs[ob].mag);
		sprintf(field, "FIELD_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, strdup(field), nil, obs[ob].field);
		sprintf(field, "SURVEY_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, strdup(field), nil, obs[ob].survey);
		sprintf(field, "STAR_GALAXY_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, strdup(field), nil, obs[ob].star_galaxy);
		sprintf(field, "XI_RESIDUAL_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, strdup(field), "arcsec", obs[ob].xi_resid);
		sprintf(field, "ETA_RESIDUAL_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, strdup(field), "arcsec", obs[ob].eta_resid);
		sprintf(field, "CALIBRATION_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, strdup(field), nil, obs[ob].calibration);
		sprintf(field, "PMM_%i", ob);
		SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, strdup(field), nil, obs[ob].pmmscan);
	}
	assert(i == USNOB_FITS_COLUMNS);
	usnob_fitstruct_inited = 1;
}

int usnob_fits_read_entries(usnob_fits* usnob, uint offset,
							uint count, usnob_entry* entries) {
	int i, c;
	unsigned char* rawdata;

	if (!usnob_fitstruct_inited)
		init_usnob_fitstruct();

	for (c=0; c<USNOB_FITS_COLUMNS; c++) {
		assert(usnob->columns[c] != -1);
		assert(usnob->table);

		if (c == USNOB_FLAGS_INDEX) {
			// special-case
			unsigned char flags;
			rawdata = qfits_query_column_seq(usnob->table, usnob->columns[c],
											 offset, count);
			assert(rawdata);
			for (i=0; i<count; i++) {
				flags = rawdata[i];
				entries[i].diffraction_spike = (flags >> 7) & 0x1;
				entries[i].motion_catalog    = (flags >> 6) & 0x1;
				entries[i].ys4               = (flags >> 5) & 0x1;
			}
			free(rawdata);
			continue;
		}

		assert(usnob->table->col[usnob->columns[c]].atom_size == 
			   usnob_fitstruct[c].size);

		qfits_query_column_seq_to_array
			(usnob->table, usnob->columns[c], offset, count,
			 ((unsigned char*)entries) + usnob_fitstruct[c].offset,
			 sizeof(usnob_entry));
	}
	return 0;
}

int usnob_fits_write_entry(usnob_fits* usnob, usnob_entry* entry) {
	int c;
	unsigned char flags;
	FILE* fid = usnob->fid;

	if (!usnob_fitstruct_inited)
		init_usnob_fitstruct();

	flags =
		(entry->diffraction_spike << 7) |
		(entry->motion_catalog    << 6) |
		(entry->ys4               << 5);

	for (c=0; c<USNOB_FITS_COLUMNS; c++) {
		fitstruct* fs = usnob_fitstruct + c;
		if (c == USNOB_FLAGS_INDEX) {
			if (fits_write_data_X(fid, flags)) {
				return -1;
			}
			continue;
		}
		if (fits_write_data(fid, ((unsigned char*)entry) + fs->offset,
							fs->fitstype)) {
			return -1;
		}
	}
	usnob->nentries++;
	return 0;
}

int usnob_fits_count_entries(usnob_fits* usnob) {
	return usnob->table->nr;
}

int usnob_fits_close(usnob_fits* usnob) {
	if (usnob->fid) {
		fits_pad_file(usnob->fid);
		if (fclose(usnob->fid)) {
			fprintf(stderr, "Error closing USNO-B FITS file: %s\n", strerror(errno));
			return -1;
		}
	}
	if (usnob->table) {
		qfits_table_close(usnob->table);
	}
	if (usnob->header) {
		qfits_header_destroy(usnob->header);
	}
	free(usnob);
	return 0;
}

usnob_fits* usnob_fits_open(char* fn) {
	int i, nextens;
	qfits_table* table;
	int c;
	usnob_fits* usnob = NULL;
	int good = 0;

	if (!usnob_fitstruct_inited)
		init_usnob_fitstruct();

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	usnob = usnob_fits_new();

	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
		int c2;
		if (!qfits_is_table(fn, i))
			continue;
		table = qfits_table_open(fn, i);

		for (c=0; c<USNOB_FITS_COLUMNS; c++)
			usnob->columns[c] = -1;

		for (c=0; c<table->nc; c++) {
			qfits_col* col = table->col + c;
			for (c2=0; c2<USNOB_FITS_COLUMNS; c2++) {
				if (usnob->columns[c2] != -1) continue;
				// allow case-insensitive matches.
				if (strcasecmp(col->tlabel, usnob_fitstruct[c2].fieldname)) continue;
				usnob->columns[c2] = c;
			}
		}

		good = 1;
		for (c=0; c<USNOB_FITS_COLUMNS; c++) {
			if (usnob->columns[c] == -1) {
				good = 0;
				break;
			}
		}
		if (good) {
			usnob->table = table;
			break;
		}
		qfits_table_close(table);
	}

	if (!good) {
		fprintf(stderr, "usnob_fits: didn't find the following required columns:\n    ");
		for (c=0; c<USNOB_FITS_COLUMNS; c++)
			if (usnob->columns[c] == -1)
				fprintf(stderr, "%s  ", usnob_fitstruct[c].fieldname);
		fprintf(stderr, "\n");

		free(usnob);
		return NULL;
	}

	usnob->nentries = usnob->table->nr;

	return usnob;
}

usnob_fits* usnob_fits_open_for_writing(char* fn) {
	usnob_fits* usnob;

	usnob = usnob_fits_new();
	usnob->fid = fopen(fn, "wb");
	if (!usnob->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	usnob->table = usnob_fits_get_table();
	usnob->header = qfits_table_prim_header_default();
	return usnob;

 bailout:
	if (usnob) {
		free(usnob);
	}
	return NULL;
}

int usnob_fits_write_headers(usnob_fits* usnob) {
	qfits_header* table_header;
	assert(usnob->fid);
	assert(usnob->header);
	qfits_header_dump(usnob->header, usnob->fid);
	table_header = qfits_table_ext_header_default(usnob->table);
	qfits_header_dump(table_header, usnob->fid);
	qfits_header_destroy(table_header);
	usnob->data_offset = ftello(usnob->fid);
	return 0;
}

int usnob_fits_fix_headers(usnob_fits* usnob) {
	off_t offset, datastart;
	qfits_header* table_header;

	assert(usnob->fid);

	offset = ftello(usnob->fid);
	fseeko(usnob->fid, 0, SEEK_SET);

	assert(usnob->header);

	usnob->table->nr = usnob->nentries;

	qfits_header_dump(usnob->header, usnob->fid);
	table_header = qfits_table_ext_header_default(usnob->table);
	qfits_header_dump(table_header, usnob->fid);
	qfits_header_destroy(table_header);

	datastart = ftello(usnob->fid);
	if (datastart != usnob->data_offset) {
		fprintf(stderr, "Error: USNO-B FITS header size changed: was %u, but is now %u.  Corruption is likely!\n",
				(uint)usnob->data_offset, (uint)datastart);
		return -1;
	}

	fseek(usnob->fid, offset, SEEK_SET);
	return 0;
}

usnob_fits* usnob_fits_new() {
	usnob_fits* rtn = malloc(sizeof(usnob_fits));
	if (!rtn) {
		fprintf(stderr, "Couldn't allocate memory for a usnob_fits structure.\n");
		exit(-1);
	}
	memset(rtn, 0, sizeof(usnob_fits));
	return rtn;
}

static qfits_table* usnob_fits_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int col;
	qfits_table* table;
	char* nil = " ";

	if (!usnob_fitstruct_inited)
		init_usnob_fitstruct();

	// one big table: the sources.
	// dummy values here...
	datasize = 0;
	ncols = USNOB_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;
	for (col=0; col<USNOB_FITS_COLUMNS; col++) {
		fitstruct* fs = usnob_fitstruct + col;
		if (col == USNOB_FLAGS_INDEX) {
			// col 2: flags (bit array)
			//  -diffraction_spike
			//  -motion_catalog
			//  -ys4
			//  -reject star ?
			//  -Tycho2 star ?
			fits_add_column(table, col, TFITS_BIN_TYPE_X, 3, nil, "FLAGS");
			continue;
		}
		fits_add_column(table, col, fs->fitstype, 1, fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

