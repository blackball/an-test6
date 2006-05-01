#include <assert.h>
#include <stddef.h>

#include "usnob_fits.h"
#include "fitsioutils.h"

/*
  struct usnob_column_name {
  char colname[32];
  int offset;
  int size;
  int special;
  };

  #define USNOB_COL(fld) { \
  .colname=#fld, \
  .offset=offsetof(usnob_entry, fld), \
  .size=sizeof(usnob_entry.fld), \
  .special=0 }

  static struct usnob_column_name usnob_cols[USNOB_FITS_COLUMNS] = {
  USNOB_COL(ra),
  USNOB_COL(dec),
  { .colname="Flags", .offset=-1, .size=-1, special=1 },
  USNOB_COL(sigma_ra),
  USNOB_COL(sigma_dec),
  USNOB_COL(sigma_ra_fit),
  USNOB_COL(sigma_dec_fit),
  USNOB_COL(epoch),
  USNOB_COL(mu_probability),
  USNOB_COL(mu_RA),
  USNOB_COL(mu_DEC),
  USNOB_COL(sigma_mu_RA),
  USNOB_COL(sigma_mu_DEC),
  USNOB_COL(),
  };
*/



// These entries MUST be listed in the same order as usnob_fits_get_table()
// orders the tables.
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
// number of fields per observation
//#define USNOB_FIELDS_PER_OBS 8
//#define USNOB_START_PER_OBS USNOB_MAG0


#define SET_OFFSET(ind, fld) { \
	usnob_entry x; \
    offsets[ind] = offsetof(usnob_entry, fld); \
    sizes[ind]   = sizeof(x.fld); \
}
//    sizes[ind]   = sizeof(usnob_entry.fld);

int usnob_fits_read_entries(usnob_fits_file* usnob, uint offset,
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
		rawdata = qfits_query_column_seq(usnob->table, usnob->columns[c],
										 offset, count);

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

		/*
		  if (c >= USNOB_START_PER_OBS) {
		  int obs, off;
		  obs = (c - USNOB_START_PER_OBS) / USNOB_FIELDS_PER_OBS;
		  off = (c - USNOB_START_PER_OBS) % USNOB_FIELDS_PER_OBS;
		  offset = obs * sizeof(offsets[USNOB_START_PER_OBS + off];
		  }
		  }
		*/
		/*
		  switch (c) {
		  // D format fields: ra, dec
		  case USNOB_RA:
		  offset = offsetof(usnob_entry, ra);
		  break;
		  case USNOB_DEC:
		  offset = offsetof(usnob_entry, dec);
		  size = sizeof(double);
		  case USNOB_DEC:
		*/
		for (i=0; i<count; i++) {
			memcpy(((unsigned char*)(entries + i)) + offsets[c],
				   rawdata, sizes[c]);
		}
	}
	return 0;




	/*
	  ixnay, can't assume we wrote the file!


	  if (fits_read_data_D(fid, entry->ra) ||
	  fits_read_data_D(fid, entry->dec) ||
	  fits_read_data_X(fid, flags) ||
	  fits_read_data_E(fid, entry->sigma_ra) ||
	  fits_read_data_E(fid, entry->sigma_dec) ||
	  fits_read_data_E(fid, entry->sigma_ra_fit) ||
	  fits_read_data_E(fid, entry->sigma_dec_fit) ||
	  fits_read_data_E(fid, entry->epoch) ||
	  fits_read_data_E(fid, entry->mu_prob) ||
	  fits_read_data_E(fid, entry->mu_ra) ||
	  fits_read_data_E(fid, entry->mu_dec) ||
	  fits_read_data_E(fid, entry->sigma_mu_ra) ||
	  fits_read_data_E(fid, entry->sigma_mu_dec) ||
	  fits_read_data_B(fid, entry->ndetections)) {
	  return -1;
	  }
	  for (ob=0; ob<5; ob++) {
	  if (fits_read_data_E(fid, entry->obs[ob].mag) ||
	  fits_read_data_I(fid, entry->obs[ob].field) ||
	  fits_read_data_B(fid, entry->obs[ob].survey) ||
	  fits_read_data_B(fid, entry->obs[ob].star_galaxy) ||
	  fits_read_data_E(fid, entry->obs[ob].xi_resid) ||
	  fits_read_data_E(fid, entry->obs[ob].eta_resid) ||
	  fits_read_data_B(fid, entry->obs[ob].calibration) ||
	  fits_read_data_J(fid, entry->obs[ob].pmmscan)) {
	  return -1;
	  }
	  }
	  return 0;
	*/
}

usnob_fits_file* usnob_fits_open(char* fn) {
	int i, nextens;
	char* colnames[USNOB_FITS_COLUMNS];
	qfits_table* table;
	int c;
	usnob_fits_file* rtnval = NULL;
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

	rtnval = malloc(sizeof(usnob_fits_file));

	// find a table containing all the columns needed...
	// (actually, find the indices of the columns we need.)
	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
		qfits_table* table;
		int c2;
		if (!qfits_is_table(fn, i))
			continue;
		table = qfits_table_open(fn, i);

		for (c=0; c<USNOB_FITS_COLUMNS; c++)
			rtnval->columns[c] = -1;

		for (c=0; c<table->nc; c++) {
			qfits_col* col = table->col + c;
			for (c2=0; c2<USNOB_FITS_COLUMNS; c2++) {
				if (rtnval->columns[c2] != -1) continue;
				if (strcmp(col->tlabel, colnames[c2])) continue;
				rtnval->columns[c2] = c;
			}
		}

		good = 1;
		for (c=0; c<USNOB_FITS_COLUMNS; c++) {
			if (rtnval->columns[c] == -1) {
				good = 0;
				break;
			}
		}
		if (good) {
			rtnval->table = table;
			break;
		}
		qfits_table_close(table);
	}

	// clean up
	for (c=0; c<USNOB_FITS_COLUMNS; c++)
		free(colnames[c]);
	if (!good) {
		free(rtnval);
		rtnval = NULL;
	}
	return rtnval;
}

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
	fits_add_column(table, col++, TFITS_BIN_TYPE_X, 3, "(binary flags)", "Flags");
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

	fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", "Num_Detections");

	for (ob=0; ob<5; ob++) {
		char field[256];
		sprintf(field, "Magnitude_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Mag", field);
		sprintf(field, "Field_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_I, 1, "", field);
		sprintf(field, "Survey_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", field);
		sprintf(field, "Star_Galaxy_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", field);
		sprintf(field, "xi_residual_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", field);
		sprintf(field, "eta_residual_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_E, 1, "Arcsec", field);
		sprintf(field, "Calibration_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_B, 1, "", field);
		sprintf(field, "PMM_%i", ob);
		fits_add_column(table, col++, TFITS_BIN_TYPE_J, 1, "", field);
	}

	assert(col == ncols);

	table->tab_w = qfits_compute_table_width(table);
	return table;
}

	
