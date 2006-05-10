#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "tycho2_fits.h"
#include "fitsioutils.h"

static qfits_table* tycho2_fits_get_table();
static tycho2_fits* tycho2_fits_new();

// mapping between a struct field and FITS field.
struct fits_struct_pair {
	char* fieldname;
	char* units;
	int offset;
	int size;
	tfits_type fitstype;
};
typedef struct fits_struct_pair fitstruct;

static fitstruct tycho2_fitstruct[TYCHO2_FITS_COLUMNS];
static bool tycho2_fitstruct_inited = 0;
static int TYCHO2_FLAGS_INDEX;
static int TYCHO2_CCDM_INDEX;

#define SET_FIELDS(A, i, t, n, u, fld) { \
 tycho2_entry x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(tycho2_entry, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].fitstype=t; \
 i++; \
}

static void init_tycho2_fitstruct() {
	fitstruct* fs = tycho2_fitstruct;
	int i = 0;
	char* nil = " ";

 	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "RA",  "degrees", RA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "DEC", "degrees", DEC);
	TYCHO2_FLAGS_INDEX = i;
	fs[i].fieldname = "FLAGS";
	fs[i].units = nil;
	fs[i].offset = -1;
	fs[i].size = 0;
	fs[i].fitstype = TFITS_BIN_TYPE_UNKNOWN;
	i++;
	TYCHO2_CCDM_INDEX = i;
	fs[i].fieldname = "CCDM";
	fs[i].units = nil;
	fs[i].offset = -1;
	fs[i].size = 0;
	fs[i].fitstype = TFITS_BIN_TYPE_UNKNOWN;
	i++;
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "TYC1", nil, tyc1);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_I, "TYC2", nil, tyc2);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "TYC3", nil, tyc3);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "NOBSERVATIONS", nil, nobs);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "MEAN_RA",  "degrees", meanRA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "MEAN_DEC", "degrees", meanDEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "PM_RA",   "mas/yr", pmRA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "PM_DEC",  "mas/yr", pmDEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_RA",   "mas", sigma_RA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_DEC",  "mas", sigma_DEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MEAN_RA",   "mas", sigma_mRA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MEAN_DEC",  "mas", sigma_mDEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_PM_RA",   "mas", sigma_pmRA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_PM_DEC",  "mas", sigma_pmDEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_RA", "yr", epoch_RA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_DEC", "yr", epoch_DEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_MEAN_RA", "yr", epoch_mRA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_MEAN_DEC", "yr", epoch_mDEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_MEAN_RA", nil, goodness_mRA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_MEAN_DEC", nil, goodness_mDEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_PM_RA", nil, goodness_pmRA);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_PM_DEC", nil, goodness_pmDEC);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_BT", nil, mag_BT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_VT", nil, mag_VT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_HP", nil, mag_HP);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MAG_BT", nil, sigma_BT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MAG_VT", nil, sigma_VT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MAG_HP", nil, sigma_HP);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "PROX", "arcsec", prox);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "CORRELATION", nil, correlation);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "HIPPARCOS_ID", nil, hipparcos_id);

	assert(i == TYCHO2_FITS_COLUMNS);
	tycho2_fitstruct_inited = 1;
}

int tycho2_fits_read_entries(tycho2_fits* tycho2, uint offset,
							uint count, tycho2_entry* entries) {
	int i=-1, c;
	unsigned char* rawdata;

	if (!tycho2_fitstruct_inited)
		init_tycho2_fitstruct();

	for (c=0; c<TYCHO2_FITS_COLUMNS; c++) {
		unsigned char* src, *dst;
		int srcstride, dststride, size;
		assert(tycho2->columns[c] != -1);
		assert(tycho2->table);
		rawdata = qfits_query_column_seq(tycho2->table, tycho2->columns[c],
										 offset, count);
		assert(rawdata);

		// special-cases
		if (c == TYCHO2_FLAGS_INDEX) {
			unsigned char flags;
			for (i=0; i<count; i++) {
				flags = rawdata[i];
				entries[i].photo_center           = (flags >> 7) & 0x1;
				entries[i].no_motion              = (flags >> 6) & 0x1;
				entries[i].tycho1_star            = (flags >> 5) & 0x1;
				entries[i].double_star            = (flags >> 4) & 0x1;
				entries[i].photo_center_treatment = (flags >> 3) & 0x1;
				entries[i].hipparcos_star         = (flags >> 2) & 0x1;

			}
			free(rawdata);
			continue;
		}
		if (c == TYCHO2_CCDM_INDEX) {
			for (i=0; i<count; i++) {
				memcpy(entries[i].hip_ccdm, rawdata + i*3, 3);
				entries[i].hip_ccdm[3] = '\0';
			}
			free(rawdata);
			continue;
		}

		assert(tycho2->table->col[tycho2->columns[c]].atom_size == tycho2_fitstruct[c].size);

		dst = ((unsigned char*)entries) + tycho2_fitstruct[c].offset;
		src = rawdata;
		dststride = sizeof(tycho2_entry);
		srcstride = tycho2_fitstruct[c].size;
		size = srcstride;
		for (i=0; i<count; i++) {
			/*
			  memcpy(((unsigned char*)(entries + i)) + tycho2_fitstruct[c].offset,
			  rawdata + (i * tycho2_fitstruct[c].size), tycho2_fitstruct[c].size);
			*/
			memcpy(dst, src, size);
			dst += dststride;
			src += srcstride;
		}
		free(rawdata);
	}
	return 0;
}

int tycho2_fits_write_entry(tycho2_fits* tycho2, tycho2_entry* entry) {
	int c;
	unsigned char flags;
	FILE* fid = tycho2->fid;

	if (!tycho2_fitstruct_inited)
		init_tycho2_fitstruct();

	flags =
		(entry->photo_center           << 7) |
		(entry->no_motion              << 6) |
		(entry->tycho1_star            << 5) |
		(entry->double_star            << 4) |
		(entry->photo_center_treatment << 3) |
		(entry->hipparcos_star         << 2);

	for (c=0; c<TYCHO2_FITS_COLUMNS; c++) {
		fitstruct* fs = tycho2_fitstruct + c;
		if (c == TYCHO2_FLAGS_INDEX) {
			if (fits_write_data_X(fid, flags)) {
				return -1;
			}
			continue;
		}
		if (c == TYCHO2_CCDM_INDEX) {
			if (fits_write_data_A(fid, entry->hip_ccdm[0]) ||
				fits_write_data_A(fid, entry->hip_ccdm[1]) ||
				fits_write_data_A(fid, entry->hip_ccdm[2])) {
				return -1;
			}
			continue;
		}
		if (fits_write_data(fid, ((unsigned char*)entry) + fs->offset,
							fs->fitstype)) {
			return -1;
		}
	}
	tycho2->nentries++;
	return 0;
}

int tycho2_fits_count_entries(tycho2_fits* tycho2) {
	return tycho2->table->nr;
}

int tycho2_fits_close(tycho2_fits* tycho2) {
	if (tycho2->fid) {
		fits_pad_file(tycho2->fid);
		if (fclose(tycho2->fid)) {
			fprintf(stderr, "Error closing Tycho-2 FITS file: %s\n", strerror(errno));
			return -1;
		}
	}
	if (tycho2->table) {
		qfits_table_close(tycho2->table);
	}
	if (tycho2->header) {
		qfits_header_destroy(tycho2->header);
	}
	free(tycho2);
	return 0;
}

tycho2_fits* tycho2_fits_open(char* fn) {
	int i, nextens;
	qfits_table* table;
	int c;
	tycho2_fits* tycho2 = NULL;
	int good = 0;

	if (!tycho2_fitstruct_inited)
		init_tycho2_fitstruct();

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	tycho2 = tycho2_fits_new();

	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
		int c2;
		if (!qfits_is_table(fn, i))
			continue;
		table = qfits_table_open(fn, i);

		for (c=0; c<TYCHO2_FITS_COLUMNS; c++)
			tycho2->columns[c] = -1;

		for (c=0; c<table->nc; c++) {
			qfits_col* col = table->col + c;
			for (c2=0; c2<TYCHO2_FITS_COLUMNS; c2++) {
				if (tycho2->columns[c2] != -1) continue;
				// allow case-insensitive matches.
				if (strcasecmp(col->tlabel, tycho2_fitstruct[c2].fieldname))
					continue;
				tycho2->columns[c2] = c;
			}
		}

		good = 1;
		for (c=0; c<TYCHO2_FITS_COLUMNS; c++) {
			if (tycho2->columns[c] == -1) {
				good = 0;
				break;
			}
		}
		if (good) {
			tycho2->table = table;
			break;
		}
		qfits_table_close(table);
	}

	if (!good) {
		fprintf(stderr, "tycho2_fits: didn't find the following required columns:\n    ");
		for (c=0; c<TYCHO2_FITS_COLUMNS; c++)
			if (tycho2->columns[c] == -1)
				fprintf(stderr, "%s  ", tycho2_fitstruct[c].fieldname);
		fprintf(stderr, "\n");

		free(tycho2);
		return NULL;
	}

	return tycho2;
}

tycho2_fits* tycho2_fits_open_for_writing(char* fn) {
	tycho2_fits* tycho2;

	tycho2 = tycho2_fits_new();
	tycho2->fid = fopen(fn, "wb");
	if (!tycho2->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	tycho2->table = tycho2_fits_get_table();
	tycho2->header = qfits_table_prim_header_default();
	return tycho2;

 bailout:
	if (tycho2) {
		free(tycho2);
	}
	return NULL;
}

int tycho2_fits_write_headers(tycho2_fits* tycho2) {
	qfits_header* table_header;
	assert(tycho2->fid);
	assert(tycho2->header);
	qfits_header_dump(tycho2->header, tycho2->fid);
	table_header = qfits_table_ext_header_default(tycho2->table);
	qfits_header_dump(table_header, tycho2->fid);
	qfits_header_destroy(table_header);
	tycho2->data_offset = ftello(tycho2->fid);
	return 0;
}

int tycho2_fits_fix_headers(tycho2_fits* tycho2) {
	off_t offset, datastart;
	qfits_header* table_header;

	assert(tycho2->fid);

	offset = ftello(tycho2->fid);
	fseeko(tycho2->fid, 0, SEEK_SET);

	assert(tycho2->header);

	tycho2->table->nr = tycho2->nentries;

	qfits_header_dump(tycho2->header, tycho2->fid);
	table_header = qfits_table_ext_header_default(tycho2->table);
	qfits_header_dump(table_header, tycho2->fid);
	qfits_header_destroy(table_header);

	datastart = ftello(tycho2->fid);
	if (datastart != tycho2->data_offset) {
		fprintf(stderr, "Error: Tycho-2 FITS header size changed: was %u, but is now %u.  Corruption is likely!\n",
				(uint)tycho2->data_offset, (uint)datastart);
		return -1;
	}

	fseek(tycho2->fid, offset, SEEK_SET);
	return 0;
}

tycho2_fits* tycho2_fits_new() {
	tycho2_fits* rtn = malloc(sizeof(tycho2_fits));
	if (!rtn) {
		fprintf(stderr, "Couldn't allocate memory for a tycho2_fits structure.\n");
		exit(-1);
	}
	memset(rtn, 0, sizeof(tycho2_fits));
	return rtn;
}

static qfits_table* tycho2_fits_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int col;
	qfits_table* table;
	char* nil = " ";

	if (!tycho2_fitstruct_inited)
		init_tycho2_fitstruct();

	// one big table: the sources.
	// dummy values here...
	datasize = 0;
	ncols = TYCHO2_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;
	for (col=0; col<TYCHO2_FITS_COLUMNS; col++) {
		fitstruct* fs = tycho2_fitstruct + col;
		if (col == TYCHO2_FLAGS_INDEX) {
			fits_add_column(table, col, TFITS_BIN_TYPE_X, 6, nil, fs->fieldname);
			continue;
		}
		if (col == TYCHO2_CCDM_INDEX) {
			fits_add_column(table, col, TFITS_BIN_TYPE_A, 3, nil, fs->fieldname);
			continue;
		}
		fits_add_column(table, col, fs->fitstype, 1, fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

