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

#include "nomad-fits.h"
#include "fitsioutils.h"

static qfits_table* nomad_fits_get_table();
static nomad_fits* nomad_fits_new();

// mapping between a struct field and FITS field.
struct fits_struct_pair {
	char* fieldname;
	char* units;
	int offset;
	int size;
	tfits_type fitstype;
};
typedef struct fits_struct_pair fitstruct;

static fitstruct nomad_fitstruct[NOMAD_FITS_COLUMNS];
static bool nomad_fitstruct_inited = 0;
static int NOMAD_FLAGS_INDEX;

#define SET_FIELDS(A, i, t, n, u, fld) { \
 nomad_entry x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(nomad_entry, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].fitstype=t; \
 i++; \
}

static void init_nomad_fitstruct() {
	fitstruct* fs = nomad_fitstruct;
	int i = 0;
	char* nil = " ";

 	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "RA",  "degrees", ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "DEC", "degrees", dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_RACOSDEC",    "arcsec", sigma_racosdec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_DEC",         "arcsec", sigma_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MU_RACOSDEC",       "arcsec/yr", mu_racosdec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MU_DEC",            "arcsec/yr", mu_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MU_RACOSDEC", "arcsec/yr", sigma_mu_racosdec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "SIGMA_MU_DEC",      "arcsec/yr", sigma_mu_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_RA",          "yr", epoch_ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "EPOCH_DEC",         "yr", epoch_dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_B",             nil,  mag_B);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_V",             nil,  mag_V);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_R",             nil,  mag_R);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_J",             nil,  mag_J);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_H",             nil,  mag_H);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "MAG_K",             nil,  mag_K);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "USNOB_ID",          nil,  usnob_id);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "TWOMASS_ID",        nil,  twomass_id);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "YB6_ID",            nil,  yb6_id);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "UCAC2_ID",          nil,  ucac2_id);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "TYCHO2_ID",         nil,  tycho2_id);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "ASTROMETRY_SRC",    nil,  astrometry_src);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "BLUE_SRC",          nil,  blue_src);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "VISUAL_SRC",        nil,  visual_src);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "RED_SRC",           nil,  red_src);

	NOMAD_FLAGS_INDEX = i;
	fs[i].fieldname = "FLAGS";
	fs[i].units = nil;
	fs[i].offset = -1;
	fs[i].size = 0;
	fs[i].fitstype = TFITS_BIN_TYPE_UNKNOWN;
	i++;

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "NOMAD_ID",         nil, nomad_id);

	assert(i == NOMAD_FITS_COLUMNS);
	nomad_fitstruct_inited = 1;
}

nomad_entry* nomad_fits_read_entry(nomad_fits* t) {
	nomad_entry* e = buffered_read(&t->br);
	if (!e)
		fprintf(stderr, "Failed to read a NOMAD catalog entry.\n");
	return e;
}

int nomad_fits_read_entries(nomad_fits* nomad, uint offset,
							uint count, nomad_entry* entries) {
	int i=-1, c;
	unsigned char* rawdata;

	if (!nomad_fitstruct_inited)
		init_nomad_fitstruct();

	for (c=0; c<NOMAD_FITS_COLUMNS; c++) {
		assert(nomad->columns[c] != -1);
		assert(nomad->table);

		// special-case
		if (c == NOMAD_FLAGS_INDEX) {
			rawdata = qfits_query_column_seq(nomad->table, nomad->columns[c],
											 offset, count);
			assert(rawdata);

			for (i=0; i<count; i++) {
				unsigned char flags1;
				unsigned char flags2;

				flags1 = rawdata[2*i + 0];
				flags2 = rawdata[2*i + 1];

				entries[i].usnob_fail         = (flags1 >> 7) & 0x1;
				entries[i].twomass_fail       = (flags1 >> 6) & 0x1;
				entries[i].tycho_astrometry   = (flags1 >> 5) & 0x1;
				entries[i].alt_radec          = (flags1 >> 4) & 0x1;
				entries[i].alt_ucac           = (flags1 >> 3) & 0x1;
				entries[i].alt_tycho          = (flags1 >> 2) & 0x1;
				entries[i].blue_o             = (flags1 >> 1) & 0x1;
				entries[i].red_e              = (flags1 >> 0) & 0x1;

				entries[i].twomass_only       = (flags2 >> 7) & 0x1;
				entries[i].hipp_astrometry    = (flags2 >> 6) & 0x1;
				entries[i].diffraction        = (flags2 >> 5) & 0x1;
				entries[i].confusion          = (flags2 >> 4) & 0x1;
				entries[i].bright_confusion   = (flags2 >> 3) & 0x1;
				entries[i].bright_artifact    = (flags2 >> 2) & 0x1;
				entries[i].standard           = (flags2 >> 1) & 0x1;
			}
			qfits_free(rawdata);
			continue;
		}
		assert(nomad->table->col[nomad->columns[c]].atom_size == nomad_fitstruct[c].size);

		qfits_query_column_seq_to_array
			(nomad->table, nomad->columns[c], offset, count,
			 ((unsigned char*)entries) + nomad_fitstruct[c].offset,
			 sizeof(nomad_entry));
	}
	return 0;
}

int nomad_fits_write_entry(nomad_fits* nomad, nomad_entry* entry) {
	int c;
	unsigned char flags[3];
	FILE* fid = nomad->fid;

	if (!nomad_fitstruct_inited)
		init_nomad_fitstruct();

	flags[0] =
		(entry->usnob_fail        ? (1 << 7) : 0) |
		(entry->twomass_fail      ? (1 << 6) : 0) |
		(entry->tycho_astrometry  ? (1 << 5) : 0) |
		(entry->alt_radec         ? (1 << 4) : 0) |
		(entry->alt_ucac          ? (1 << 3) : 0) |
		(entry->alt_tycho         ? (1 << 2) : 0) |
		(entry->blue_o            ? (1 << 1) : 0) |
		(entry->red_e             ? (1 << 0) : 0);

	flags[1] =
		(entry->twomass_only      ? (1 << 7) : 0) |
		(entry->hipp_astrometry   ? (1 << 6) : 0) |
		(entry->diffraction       ? (1 << 5) : 0) |
		(entry->confusion         ? (1 << 4) : 0) |
		(entry->bright_confusion  ? (1 << 3) : 0) |
		(entry->bright_artifact   ? (1 << 2) : 0) |
		(entry->standard          ? (1 << 1) : 0);

	for (c=0; c<NOMAD_FITS_COLUMNS; c++) {
		fitstruct* fs = nomad_fitstruct + c;
		if (c == NOMAD_FLAGS_INDEX) {
			if (fits_write_data_X(fid, flags[0]) ||
				fits_write_data_X(fid, flags[1])) {
				return -1;
			}
			continue;
		}
		if (fits_write_data(fid, ((unsigned char*)entry) + fs->offset,
							fs->fitstype)) {
			return -1;
		}
	}
	nomad->nentries++;
	return 0;
}

int nomad_fits_count_entries(nomad_fits* nomad) {
	return nomad->table->nr;
}

int nomad_fits_close(nomad_fits* nomad) {
	if (nomad->fid) {
		fits_pad_file(nomad->fid);
		if (fclose(nomad->fid)) {
			fprintf(stderr, "Error closing NOMAD FITS file: %s\n", strerror(errno));
			return -1;
		}
	}
	if (nomad->table) {
		qfits_table_close(nomad->table);
	}
	if (nomad->header) {
		qfits_header_destroy(nomad->header);
	}
	buffered_read_free(&nomad->br);
	free(nomad);
	return 0;
}

nomad_fits* nomad_fits_open(char* fn) {
	int i, nextens;
	qfits_table* table;
	int c;
	nomad_fits* nomad = NULL;
	int good = 0;

	if (!nomad_fitstruct_inited)
		init_nomad_fitstruct();

	if (!qfits_is_fits(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	nomad = nomad_fits_new();

	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
		if (!qfits_is_table(fn, i))
			continue;
		table = qfits_table_open(fn, i);

		good = 1;
		for (c=0; c<NOMAD_FITS_COLUMNS; c++) {
			nomad->columns[c] = fits_find_column(table, nomad_fitstruct[c].fieldname);
			if (nomad->columns[c] == -1) {
				good = 0;
				break;
			}
		}
		if (good) {
			nomad->table = table;
			break;
		}
		qfits_table_close(table);
	}

	if (!good) {
		fprintf(stderr, "nomad_fits: didn't find the following required columns:\n    ");
		for (c=0; c<NOMAD_FITS_COLUMNS; c++)
			if (nomad->columns[c] == -1)
				fprintf(stderr, "%s  ", nomad_fitstruct[c].fieldname);
		fprintf(stderr, "\n");

		free(nomad);
		return NULL;
	}
	nomad->nentries = nomad->table->nr;
	nomad->br.ntotal = nomad->nentries;
	return nomad;
}

nomad_fits* nomad_fits_open_for_writing(char* fn) {
	nomad_fits* nomad;

	nomad = nomad_fits_new();
	nomad->fid = fopen(fn, "wb");
	if (!nomad->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	nomad->table = nomad_fits_get_table();
	nomad->header = qfits_table_prim_header_default();
	qfits_header_add(nomad->header, "NOMAD", "T", "This is a NOMAD catalog.", NULL);
	qfits_header_add(nomad->header, "NOBJS", "0", "", NULL);
	qfits_header_add(nomad->header, "COMMENT", "The FLAGS variable is composed of 15 boolean values packed into 2 bytes.", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "  Byte 0:", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x80: UBBIT / usnob_fail", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x40: TMBIT / twomass_fail", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x20: TYBIT / tycho_astrometry", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x10: XRBIT / alt_radec", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x08: IUCBIT / alt_ucac", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x04: ITYBIT / alt_tycho", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x02: OMAGBIT / blue_o", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x01: EMAGBIT / red_e", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "  Byte 1:", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x80: TMONLY / twomass_only", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x40: HIPAST / hipp_astrometry", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x20: SPIKE / diffraction", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x10: TYCONF / confusion", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x08: BSCONF / bright_confusion", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x04: BSART / bright_artifact", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x02: USEME / standard", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "    0x01: unused", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "  Note that the ITMBIT and EXCAT bits were not set for any entry in the ", NULL, NULL);
	qfits_header_add(nomad->header, "COMMENT", "  released NOMAD catalog, so were not included here.", NULL, NULL);
	return nomad;

 bailout:
	if (nomad) {
		free(nomad);
	}
	return NULL;
}

int nomad_fits_write_headers(nomad_fits* nomad) {
	qfits_header* table_header;
	assert(nomad->fid);
	assert(nomad->header);
	fits_header_mod_int(nomad->header, "NOBJS", nomad->nentries, "Number of objects in this catalog.");
	qfits_header_dump(nomad->header, nomad->fid);
	nomad->table->nr = nomad->nentries;
	table_header = qfits_table_ext_header_default(nomad->table);
	qfits_header_dump(table_header, nomad->fid);
	qfits_header_destroy(table_header);
	nomad->header_end = ftello(nomad->fid);
	return 0;
}

int nomad_fits_fix_headers(nomad_fits* nomad) {
 	off_t offset;
	off_t old_header_end;
	offset = ftello(nomad->fid);
	fseeko(nomad->fid, 0, SEEK_SET);
	old_header_end = nomad->header_end;
	nomad_fits_write_headers(nomad);
	if (old_header_end != nomad->header_end) {
		fprintf(stderr, "Warning: NOMAD FITS header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)old_header_end,
				(unsigned long)nomad->header_end);
		return -1;
	}
	fseek(nomad->fid, offset, SEEK_SET);
	return 0;
}

static int nomad_fits_refill_buffer(void* userdata, void* buffer, uint offset, uint n) {
	nomad_fits* cat = userdata;
	nomad_entry* en = buffer;
	return nomad_fits_read_entries(cat, offset, n, en);
}

nomad_fits* nomad_fits_new() {
	nomad_fits* rtn = calloc(1, sizeof(nomad_fits));
	if (!rtn) {
		fprintf(stderr, "Couldn't allocate memory for a nomad_fits structure.\n");
		exit(-1);
	}
	rtn->br.blocksize = 1000;
	rtn->br.elementsize = sizeof(nomad_entry);
	rtn->br.refill_buffer = nomad_fits_refill_buffer;
	rtn->br.userdata = rtn;
	return rtn;
}

static qfits_table* nomad_fits_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int col;
	qfits_table* table;
	char* nil = " ";

	if (!nomad_fitstruct_inited)
		init_nomad_fitstruct();

	// one big table: the sources.
	// dummy values here...
	datasize = 0;
	ncols = NOMAD_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;
	for (col=0; col<NOMAD_FITS_COLUMNS; col++) {
		fitstruct* fs = nomad_fitstruct + col;
		if (col == NOMAD_FLAGS_INDEX) {
			// "15" in the function call below is the number of BIT values
			// in the flags array.
			fits_add_column(table, col, TFITS_BIN_TYPE_X, 15, nil, fs->fieldname);
			continue;
		}
		fits_add_column(table, col, fs->fitstype, 1, fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

