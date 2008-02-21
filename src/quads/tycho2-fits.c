/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "tycho2-fits.h"
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

 	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "RA",  "degrees", ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "DEC", "degrees", dec);
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
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "MEAN_RA",  "deg", mean_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "MEAN_DEC", "deg", mean_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "PM_RA",   "arcsec/yr", pm_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "PM_DEC",  "arcsec/yr", pm_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_RA",   "deg", sigma_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_DEC",  "deg", sigma_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MEAN_RA",   "deg", sigma_mean_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MEAN_DEC",  "deg", sigma_mean_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_PM_RA",   "arcsec/yr", sigma_pm_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_PM_DEC",  "arcsec/yr", sigma_pm_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_RA", "yr", epoch_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_DEC", "yr", epoch_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_MEAN_RA", "yr", epoch_mean_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_MEAN_DEC", "yr", epoch_mean_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_MEAN_RA", nil, goodness_mean_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_MEAN_DEC", nil, goodness_mean_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_PM_RA", nil, goodness_pm_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GOODNESS_PM_DEC", nil, goodness_pm_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_BT", nil, mag_BT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_VT", nil, mag_VT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_HP", nil, mag_HP);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MAG_BT", nil, sigma_BT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MAG_VT", nil, sigma_VT);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MAG_HP", nil, sigma_HP);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "PROX", "arcsec", prox);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "CORRELATION", nil, correlation);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "HIPPARCOS_ID", nil, hipparcos_id);

	assert(i == TYCHO2_FITS_COLUMNS);
	tycho2_fitstruct_inited = 1;
}

tycho2_entry* tycho2_fits_read_entry(tycho2_fits* t) {
	tycho2_entry* e = buffered_read(&t->br);
	if (!e)
		fprintf(stderr, "Failed to read a Tycho-2 catalog entry.\n");
	return e;
}

int tycho2_fits_read_entries(tycho2_fits* tycho2, uint offset,
							uint count, tycho2_entry* entries) {
	int i=-1, c;
	unsigned char* rawdata;

	if (!tycho2_fitstruct_inited)
		init_tycho2_fitstruct();

	for (c=0; c<TYCHO2_FITS_COLUMNS; c++) {
		assert(tycho2->columns[c] != -1);
		assert(tycho2->table);

		// special-cases
		if ((c == TYCHO2_FLAGS_INDEX) ||
			(c == TYCHO2_CCDM_INDEX)) {

			rawdata = qfits_query_column_seq(tycho2->table, tycho2->columns[c],
											 offset, count);
			assert(rawdata);

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
			}
			if (c == TYCHO2_CCDM_INDEX) {
				for (i=0; i<count; i++) {
					memcpy(entries[i].hip_ccdm, rawdata + i*3, 3);
					entries[i].hip_ccdm[3] = '\0';
				}
			}
			qfits_free(rawdata);
			continue;
		}
		assert(tycho2->table->col[tycho2->columns[c]].atom_size == tycho2_fitstruct[c].size);

		qfits_query_column_seq_to_array
			(tycho2->table, tycho2->columns[c], offset, count,
			 ((unsigned char*)entries) + tycho2_fitstruct[c].offset,
			 sizeof(tycho2_entry));
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
	tycho2->table->nr++;
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
	buffered_read_free(&tycho2->br);
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

	if (!qfits_is_fits(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	tycho2 = tycho2_fits_new();

	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
		if (!qfits_is_table(fn, i))
			continue;
		table = qfits_table_open(fn, i);

		good = 1;
		for (c=0; c<TYCHO2_FITS_COLUMNS; c++) {
			tycho2->columns[c] = fits_find_column(table, tycho2_fitstruct[c].fieldname);
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
	tycho2->br.ntotal = tycho2->table->nr;
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
	qfits_header_add(tycho2->header, "TYCHO_2", "T", "This is a Tycho-2 catalog.", NULL);
	qfits_header_add(tycho2->header, "NOBJS", "0", "", NULL);
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
	fits_header_mod_int(tycho2->header, "NOBJS", tycho2_fits_count_entries(tycho2), "Number of objects in this catalog.");
	qfits_header_dump(tycho2->header, tycho2->fid);
	table_header = qfits_table_ext_header_default(tycho2->table);
	qfits_header_dump(table_header, tycho2->fid);
	qfits_header_destroy(table_header);
	tycho2->header_end = ftello(tycho2->fid);
	return 0;
}

int tycho2_fits_fix_headers(tycho2_fits* tycho2) {
 	off_t offset;
	off_t old_header_end;
	offset = ftello(tycho2->fid);
	fseeko(tycho2->fid, 0, SEEK_SET);
	old_header_end = tycho2->header_end;
	tycho2_fits_write_headers(tycho2);
	if (old_header_end != tycho2->header_end) {
		fprintf(stderr, "Warning: TYCHO-2 FITS header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)old_header_end,
				(unsigned long)tycho2->header_end);
		return -1;
	}
	fseek(tycho2->fid, offset, SEEK_SET);
	return 0;
}

static int tycho2_fits_refill_buffer(void* userdata, void* buffer, uint offset, uint n) {
	tycho2_fits* cat = userdata;
	tycho2_entry* en = buffer;
	return tycho2_fits_read_entries(cat, offset, n, en);
}

tycho2_fits* tycho2_fits_new() {
	tycho2_fits* rtn = calloc(1, sizeof(tycho2_fits));
	if (!rtn) {
		fprintf(stderr, "Couldn't allocate memory for a tycho2_fits structure.\n");
		exit(-1);
	}
	rtn->br.blocksize = 1000;
	rtn->br.elementsize = sizeof(tycho2_entry);
	rtn->br.refill_buffer = tycho2_fits_refill_buffer;
	rtn->br.userdata = rtn;
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

