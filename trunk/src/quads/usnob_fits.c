#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "usnob_fits.h"
#include "fitsioutils.h"

static qfits_table* usnob_fits_get_table();
static usnob_fits* usnob_fits_new();

// These entries MUST be listed in the same order as usnob_fits_get_table()
// orders the columns.
enum {
	USNOB_RA,
	USNOB_DEC,
	USNOB_FLAGS,
	USNOB_SIGMA_RA,
	USNOB_SIGMA_DEC,
	USNOB_SIGMA_RA_FIT,
	USNOB_SIGMA_DEC_FIT,
	USNOB_EPOCH,
	USNOB_MU_PROB,
	USNOB_MU_RA,
	USNOB_MU_DEC,
	USNOB_SIGMA_MU_RA,
	USNOB_SIGMA_MU_DEC,
	USNOB_NDETECTIONS,
	USNOB_ID,

	USNOB_MAG0,
	USNOB_FIELD0,
	USNOB_SURVEY0,
	USNOB_STAR_GALAXY0,
	USNOB_XI_RESID0,
	USNOB_ETA_RESID0,
	USNOB_CALIB0,
	USNOB_PMM0,

	USNOB_MAG1,
	USNOB_FIELD1,
	USNOB_SURVEY1,
	USNOB_STAR_GALAXY1,
	USNOB_XI_RESID1,
	USNOB_ETA_RESID1,
	USNOB_CALIB1,
	USNOB_PMM1,

	USNOB_MAG2,
	USNOB_FIELD2,
	USNOB_SURVEY2,
	USNOB_STAR_GALAXY2,
	USNOB_XI_RESID2,
	USNOB_ETA_RESID2,
	USNOB_CALIB2,
	USNOB_PMM2,

	USNOB_MAG3,
	USNOB_FIELD3,
	USNOB_SURVEY3,
	USNOB_STAR_GALAXY3,
	USNOB_XI_RESID3,
	USNOB_ETA_RESID3,
	USNOB_CALIB3,
	USNOB_PMM3,

	USNOB_MAG4,
	USNOB_FIELD4,
	USNOB_SURVEY4,
	USNOB_STAR_GALAXY4,
	USNOB_XI_RESID4,
	USNOB_ETA_RESID4,
	USNOB_CALIB4,
	USNOB_PMM4
};

#define SET_OFFSET(ind, fld) { \
	usnob_entry x; \
    offsets[ind] = offsetof(usnob_entry, fld); \
    sizes[ind]   = sizeof(x.fld); \
}

int usnob_fits_read_entries(usnob_fits* usnob, uint offset,
							uint count, usnob_entry* entries) {
	int i, c;
	unsigned char* rawdata;
	int offsets[USNOB_FITS_COLUMNS];
	int sizes[USNOB_FITS_COLUMNS];

	SET_OFFSET(USNOB_RA,  ra);
	SET_OFFSET(USNOB_DEC, dec)
	// flags
	SET_OFFSET(USNOB_SIGMA_RA, sigma_ra);
	SET_OFFSET(USNOB_SIGMA_DEC, sigma_dec);
	SET_OFFSET(USNOB_SIGMA_RA_FIT, sigma_ra_fit);
	SET_OFFSET(USNOB_SIGMA_DEC_FIT, sigma_dec_fit);
	SET_OFFSET(USNOB_EPOCH, epoch);
	SET_OFFSET(USNOB_MU_PROB, mu_prob);
	SET_OFFSET(USNOB_MU_RA, mu_ra);
	SET_OFFSET(USNOB_MU_DEC, mu_dec);
	SET_OFFSET(USNOB_SIGMA_MU_RA, sigma_mu_ra);
	SET_OFFSET(USNOB_SIGMA_MU_DEC, sigma_mu_dec);
	SET_OFFSET(USNOB_NDETECTIONS, ndetections);
	SET_OFFSET(USNOB_ID, usnob_id);

	SET_OFFSET(USNOB_MAG0,         obs[0].mag);
	SET_OFFSET(USNOB_FIELD0,       obs[0].field);
	SET_OFFSET(USNOB_SURVEY0,      obs[0].survey);
	SET_OFFSET(USNOB_STAR_GALAXY0, obs[0].star_galaxy);
	SET_OFFSET(USNOB_XI_RESID0,    obs[0].xi_resid);
	SET_OFFSET(USNOB_ETA_RESID0,   obs[0].eta_resid);
	SET_OFFSET(USNOB_CALIB0,       obs[0].calibration);
	SET_OFFSET(USNOB_PMM0,         obs[0].pmmscan);

	SET_OFFSET(USNOB_MAG1,         obs[1].mag);
	SET_OFFSET(USNOB_FIELD1,       obs[1].field);
	SET_OFFSET(USNOB_SURVEY1,      obs[1].survey);
	SET_OFFSET(USNOB_STAR_GALAXY1, obs[1].star_galaxy);
	SET_OFFSET(USNOB_XI_RESID1,    obs[1].xi_resid);
	SET_OFFSET(USNOB_ETA_RESID1,   obs[1].eta_resid);
	SET_OFFSET(USNOB_CALIB1,       obs[1].calibration);
	SET_OFFSET(USNOB_PMM1,         obs[1].pmmscan);

	SET_OFFSET(USNOB_MAG2,         obs[2].mag);
	SET_OFFSET(USNOB_FIELD2,       obs[2].field);
	SET_OFFSET(USNOB_SURVEY2,      obs[2].survey);
	SET_OFFSET(USNOB_STAR_GALAXY2, obs[2].star_galaxy);
	SET_OFFSET(USNOB_XI_RESID2,    obs[2].xi_resid);
	SET_OFFSET(USNOB_ETA_RESID2,   obs[2].eta_resid);
	SET_OFFSET(USNOB_CALIB2,       obs[2].calibration);
	SET_OFFSET(USNOB_PMM2,         obs[2].pmmscan);

	SET_OFFSET(USNOB_MAG3,         obs[3].mag);
	SET_OFFSET(USNOB_FIELD3,       obs[3].field);
	SET_OFFSET(USNOB_SURVEY3,      obs[3].survey);
	SET_OFFSET(USNOB_STAR_GALAXY3, obs[3].star_galaxy);
	SET_OFFSET(USNOB_XI_RESID3,    obs[3].xi_resid);
	SET_OFFSET(USNOB_ETA_RESID3,   obs[3].eta_resid);
	SET_OFFSET(USNOB_CALIB3,       obs[3].calibration);
	SET_OFFSET(USNOB_PMM3,         obs[3].pmmscan);

	SET_OFFSET(USNOB_MAG4,         obs[4].mag);
	SET_OFFSET(USNOB_FIELD4,       obs[4].field);
	SET_OFFSET(USNOB_SURVEY4,      obs[4].survey);
	SET_OFFSET(USNOB_STAR_GALAXY4, obs[4].star_galaxy);
	SET_OFFSET(USNOB_XI_RESID4,    obs[4].xi_resid);
	SET_OFFSET(USNOB_ETA_RESID4,   obs[4].eta_resid);
	SET_OFFSET(USNOB_CALIB4,       obs[4].calibration);
	SET_OFFSET(USNOB_PMM4,         obs[4].pmmscan);

	assert(sizeof(double) == 8);
	assert(sizeof(float) == 4);

	for (c=0; c<USNOB_FITS_COLUMNS; c++) {
		assert(usnob->columns[c] != -1);
		assert(usnob->table);
		rawdata = qfits_query_column_seq(usnob->table, usnob->columns[c],
										 offset, count);
		assert(rawdata);

		if (c == USNOB_FLAGS) {
			// special-case
			unsigned char flags;
			for (i=0; i<count; i++) {
				flags = rawdata[i];
				entries[i].diffraction_spike = (flags >> 7) & 0x1;
				entries[i].motion_catalog    = (flags >> 6) & 0x1;
				entries[i].ys4               = (flags >> 5) & 0x1;
			}
			continue;
		}

		assert(usnob->table->col[usnob->columns[c]].atom_size == sizes[c]);

		for (i=0; i<count; i++) {
			memcpy(((unsigned char*)(entries + i)) + offsets[c],
				   rawdata, sizes[c]);
		}
		free(rawdata);
	}
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
	/*
	  if (usnob->table_header) {
	  qfits_header_destroy(usnob->table_header);
	  }
	*/
	free(usnob);
	return 0;
}

usnob_fits* usnob_fits_open(char* fn) {
	int i, nextens;
	char* colnames[USNOB_FITS_COLUMNS];
	qfits_table* table;
	int c;
	usnob_fits* usnob = NULL;
	int good = 0;

	memset(colnames, 0, USNOB_FITS_COLUMNS * sizeof(char*));

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	// grab the column names.
	table = usnob_fits_get_table();
	assert(table->nc == USNOB_FITS_COLUMNS);
	for (c=0; c<table->nc; c++)
		colnames[c] = strdup(table->col[c].tlabel);
	qfits_table_close(table);

	usnob = usnob_fits_new();

	// find a table containing all the columns needed...
	// (actually, find the indices of the columns we need.)
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
				if (strcasecmp(col->tlabel, colnames[c2])) continue;
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
				fprintf(stderr, "%s  ", colnames[c]);
		fprintf(stderr, "\n");
	}

	// clean up
	for (c=0; c<USNOB_FITS_COLUMNS; c++)
		free(colnames[c]);
	if (!good) {
		free(usnob);
		usnob = NULL;
	}
	return usnob;
}

int usnob_fits_write_entry(usnob_fits* usnob, usnob_entry* entry) {
	int ob;
	unsigned char flags;
	FILE* fid = usnob->fid;

	flags =
		(entry->diffraction_spike << 7) |
		(entry->motion_catalog    << 6) |
		(entry->ys4               << 5);

	if (fits_write_data_D(fid, entry->ra) ||
		fits_write_data_D(fid, entry->dec) ||
		fits_write_data_X(fid, flags) ||
		fits_write_data_E(fid, entry->sigma_ra) ||
		fits_write_data_E(fid, entry->sigma_dec) ||
		fits_write_data_E(fid, entry->sigma_ra_fit) ||
		fits_write_data_E(fid, entry->sigma_dec_fit) ||
		fits_write_data_E(fid, entry->epoch) ||
		fits_write_data_E(fid, entry->mu_prob) ||
		fits_write_data_E(fid, entry->mu_ra) ||
		fits_write_data_E(fid, entry->mu_dec) ||
		fits_write_data_E(fid, entry->sigma_mu_ra) ||
		fits_write_data_E(fid, entry->sigma_mu_dec) ||
		fits_write_data_B(fid, entry->ndetections) ||
		// NOTE, this field is slightly hacky since we're using it as an
		// unsigned int 32, but FITS actually only supports signed int 32.
		fits_write_data_J(fid, entry->usnob_id)) {
		return -1;
	}
	for (ob=0; ob<5; ob++) {
		if (fits_write_data_E(fid, entry->obs[ob].mag) ||
			fits_write_data_I(fid, entry->obs[ob].field) ||
			fits_write_data_B(fid, entry->obs[ob].survey) ||
			fits_write_data_B(fid, entry->obs[ob].star_galaxy) ||
			fits_write_data_E(fid, entry->obs[ob].xi_resid) ||
			fits_write_data_E(fid, entry->obs[ob].eta_resid) ||
			fits_write_data_B(fid, entry->obs[ob].calibration) ||
			fits_write_data_J(fid, entry->obs[ob].pmmscan)) {
			return -1;
		}
	}

	usnob->nentries++;

	return 0;
}

static int fits_add_column(qfits_table* table, int column, tfits_type type,
						   int ncopies, char* units, char* label) {
	int atomsize;
	int colsize;
	switch (type) {
	case TFITS_BIN_TYPE_A:
	case TFITS_BIN_TYPE_X:
	case TFITS_BIN_TYPE_L:
	case TFITS_BIN_TYPE_B:
		atomsize = 1;
		break;
	case TFITS_BIN_TYPE_I:
		atomsize = 2;
		break;
	case TFITS_BIN_TYPE_J:
	case TFITS_BIN_TYPE_E:
		atomsize = 4;
		break;
		//case TFITS_BIN_TYPE_K:
	case TFITS_BIN_TYPE_D:
		atomsize = 8;
		break;
	default:
		fprintf(stderr, "Unknown atom size for type %i.\n", type);
		return -1;
	}
	if (type == TFITS_BIN_TYPE_X)
		// bit field: convert bits to bytes, rounding up.
		ncopies = (ncopies + 7) / 8;
	colsize = atomsize * ncopies;

	qfits_col_fill(table->col + column, ncopies, 0, atomsize, type, label, units,
				   "", "", 0, 0, 0, 0, table->tab_w);
	table->tab_w += colsize;
	return 0;
}

static qfits_table* usnob_fits_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int ob;
	int col;
	qfits_table* table;
	char* nil = " ";

	// one big table: the sources.
	// dummy values here...
	datasize = 0;
	ncols = USNOB_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;
	col = 0;
	// col 0: RA (double)
	fits_add_column(table, col++, TFITS_BIN_TYPE_D, 1, "Degrees", "RA");
	// col 1: DEC (double)
	fits_add_column(table, col++, TFITS_BIN_TYPE_D, 1, "Degrees", "DEC");
	// col 2: flags (bit array)
	//  -diffraction_spike
	//  -motion_catalog
	//  -ys4
	//  -reject star ?
	//  -Tycho2 star ?
	fits_add_column(table, col++, TFITS_BIN_TYPE_X, 3, nil, "FLAGS");
	// col 3: sig_RA (float)
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "SIGMA_RA");
	// col 4: sig_DEC (float)
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "SIGMA_DEC");

	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "SIGMA_RA_FIT");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "SIGMA_DEC_FIT");

	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Years", "EPOCH");

	// motion
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, nil, "MU_PROBABILITY");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "MU_RA");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "MU_DEC");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "SIGMA_MU_RA");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "SIGMA_MU_DEC");

	fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, nil, "NUM_DETECTIONS");
	fits_add_column(table, col++, TFITS_BIN_TYPE_J, 1, nil, "USNOB_ID");

	for (ob=0; ob<5; ob++) {
		char field[256];
		sprintf(field, "MAGNITUDE_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Mag", field);
		sprintf(field, "FIELD_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_I, 1, nil, field);
		sprintf(field, "SURVEY_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, nil, field);
		sprintf(field, "STAR_GALAXY_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, nil, field);
		sprintf(field, "XI_RESIDUAL_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", field);
		sprintf(field, "ETA_RESIDUAL_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", field);
		sprintf(field, "CALIBRATION_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, nil, field);
		sprintf(field, "PMM_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_J, 1, nil, field);
	}

	assert(col == ncols);

	table->tab_w = qfits_compute_table_width(table);
	return table;
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
	//usnob->table_header = qfits_table_ext_header_default(usnob->table);
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
	//assert(usnob->table_header);
	qfits_header_dump(usnob->header, usnob->fid);
	table_header = qfits_table_ext_header_default(usnob->table);
	//qfits_header_dump(usnob->table_header, usnob->fid);
	qfits_header_dump(table_header, usnob->fid);
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
	//assert(usnob->table_header);

	usnob->table->nr = usnob->nentries;

	qfits_header_dump(usnob->header, usnob->fid);
	table_header = qfits_table_ext_header_default(usnob->table);
	//qfits_header_dump(usnob->table_header, usnob->fid);
	qfits_header_dump(table_header, usnob->fid);

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

