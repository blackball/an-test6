#include <assert.h>

#include "usnob_fits.h"
#include "fitsioutils.h"

int usnob_fits_write_entry(FILE* fid, usnob_entry* entry) {
	int ob;
	unsigned char flags;

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
		fits_write_data_B(fid, entry->ndetections)) {
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

qfits_table* usnob_fits_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int ob;
	int col;
	qfits_table* table;

	// one big table: the sources.
	// dummy values here...
	datasize = 0;
	ncols = 54;
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
	fits_add_column(table, col++, TFITS_BIN_TYPE_X, 3, "(binary flags)", "FLAGS");
	// col 3: sig_RA (float)
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_RA");
	// col 4: sig_DEC (float)
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_DEC");

	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_RA_fit");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "sigma_DEC_fit");

	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Years", "epoch");

	// motion
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "(probability)", "mu_probability");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "mu_RA");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "mu_DEC");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "sigma_mu_RA");
	fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec/Yr", "sigma_mu_DEC");

	fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Num Detections");

	for (ob=0; ob<5; ob++) {
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Mag", "Magnitude");
		fits_add_column(table, col++, TFITS_BIN_TYPE_I, 1, "", "Field number");
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Survey number");
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Star/Galaxy");
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "xi residual");
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", "eta residual");
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Calibration source");
		fits_add_column(table, col++, TFITS_BIN_TYPE_J, 1, "", "PMM backpointer");
	}

	assert(col == ncols);

	table->tab_w = datasize = qfits_compute_table_width(table);
	return table;
}

