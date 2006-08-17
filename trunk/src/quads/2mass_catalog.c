#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "2mass_catalog.h"
#include "fitsioutils.h"
#include "starutil.h"

static qfits_table* twomass_catalog_get_table();
static twomass_catalog* twomass_catalog_new();

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

static fitstruct twomass_fitstruct[TWOMASS_FITS_COLUMNS];
static bool twomass_fitstruct_inited = 0;

#define SET_ARRAY(A, i, t, n, u, fld, na) { \
 twomass_entry x; \
 A[i].fieldname=n; \
 A[i].units=u; \
 A[i].offset=offsetof(twomass_entry, fld); \
 A[i].size=sizeof(x.fld); \
 A[i].fitstype=t; \
 A[i].ncopies = na; \
 i++; \
}
#define SET_FIELDS(A, i, t, n, u, fld) SET_ARRAY(A,i,t,n,u,fld,1)
#define SET_SPECIAL(A, i, t, n, u)

static void init_twomass_fitstruct() {
	fitstruct* fs = twomass_fitstruct;
	int i = 0;
	char* nil = " ";

 	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "RA",  "degrees", ra);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "DEC", "degrees", dec);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "ERR_MAJOR", "arcsec", err_major);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "ERR_MINOR", "arcsec", err_minor);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "ERR_ANGLE", "degrees", err_angle);
	SET_ARRAY (fs, i, TFITS_BIN_TYPE_A, "DESIGNATION", nil, designation, 17);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "J_MAG", "mag", j_m);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "J_CMSIG", "mag", j_cmsig);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "J_MSIGCOM", "mag", j_msigcom);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "J_SNR", nil, j_snr);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "H_MAG", "mag", h_m);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "H_CMSIG", "mag", h_cmsig);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "H_MSIGCOM", "mag", h_msigcom);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "H_SNR", nil, h_snr);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "K_MAG", "mag", k_m);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "K_CMSIG", "mag", k_cmsig);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "K_MSIGCOM", "mag", k_msigcom);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "K_SNR", nil, k_snr);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "PROX", "arcsec", proximity);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "PROX_KEY", nil, prox_key);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_J, "KEY", nil, key);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GLON", "degrees", glon);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "GLAT", "degrees", glat);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "X_SCAN", "arcsec", x_scan);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_D, "JDATE", "julian date", jdate);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "J_PSFCHI", nil, j_psfchi);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "J_M_STDAP", "mag", j_m_stdap);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "J_MSIG_STDAP", "mag", j_msig_stdap);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "H_PSFCHI", nil, h_psfchi);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "H_M_STDAP", "mag", h_m_stdap);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "H_MSIG_STDAP", "mag", h_msig_stdap);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "K_PSFCHI", nil, k_psfchi);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "K_M_STDAP", "mag", k_m_stdap);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "K_MSIG_STDAP", "mag", k_msig_stdap);

	SET_FIELDS(fs, i, TFITS_BIN_TYPE_B, "PHI_OPT", "degrees", phi_opt);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "DIST_OPT", "arcsec", dist_opt);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "B_M_OPT", "mag", b_m_opt);
	SET_FIELDS(fs, i, TFITS_BIN_TYPE_E, "VR_M_OPT", "mag", vr_m_opt);





	assert(i == TWOMASS_FITS_COLUMNS);
	twomass_fitstruct_inited = 1;
}


twomass_catalog* twomass_catalog_open(char* fn) {
	int i, nextens;
	qfits_table* table;
	int c;
	twomass_catalog* cat = NULL;
	int good = 0;

	if (!twomass_fitstruct_inited)
		init_twomass_fitstruct();

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		return NULL;
	}

	cat = twomass_catalog_new();

	// find a table containing all the columns needed...
	// (and find the indices of the columns we need.)
	nextens = qfits_query_n_ext(fn);
	if (nextens == -1) {
		fprintf(stderr, "Couldn't find the number of extensions in file %s.\n", fn);
		return NULL;
	}
	for (i=0; i<=nextens; i++) {
		if (!qfits_is_table(fn, i))
			continue;
		table = qfits_table_open(fn, i);

		for (c=0; c<TWOMASS_FITS_COLUMNS; c++)
			cat->columns[c] = fits_find_column(table, twomass_fitstruct[c].fieldname);
		good = 1;
		for (c=0; c<TWOMASS_FITS_COLUMNS; c++) {
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
		fprintf(stderr, "twomass_catalog: didn't find the following required columns:\n    ");
		for (c=0; c<TWOMASS_FITS_COLUMNS; c++)
			if (cat->columns[c] == -1)
				fprintf(stderr, "%s  ", twomass_fitstruct[c].fieldname);
		fprintf(stderr, "\n");
		free(cat);
		return NULL;
	}
	cat->nentries = cat->table->nr;
	cat->br.ntotal = cat->nentries;
	return cat;
}

twomass_catalog* twomass_catalog_open_for_writing(char* fn) {
	twomass_catalog* cat;
	cat = twomass_catalog_new();
	cat->fid = fopen(fn, "wb");
	if (!cat->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	cat->table = twomass_catalog_get_table();
	cat->header = qfits_table_prim_header_default();
	qfits_header_add(cat->header, "2MASS_CAT", "T", "This is the 2-MASS catalog.", NULL);
	qfits_header_add(cat->header, "NOBJS", "0", "(filler)", NULL);
	return cat;
 bailout:
	if (cat) {
		free(cat);
	}
	return NULL;
}

int twomass_catalog_write_headers(twomass_catalog* cat) {
	qfits_header* table_header;
	char val[32];
	assert(cat->fid);
	assert(cat->header);
	sprintf(val, "%u", cat->nentries);
	qfits_header_mod(cat->header, "NOBJS", val, "Number of objects in this catalog.");
	qfits_header_dump(cat->header, cat->fid);
	cat->table->nr = cat->nentries;
	table_header = qfits_table_ext_header_default(cat->table);
	qfits_header_dump(table_header, cat->fid);
	qfits_header_destroy(table_header);
	cat->header_end = ftello(cat->fid);
	return 0;
}

int twomass_catalog_fix_headers(twomass_catalog* cat) {
 	off_t offset;
	off_t old_header_end;

	if (!cat->fid) {
		fprintf(stderr, "twomass_catalog_fix_headers: fid is null.\n");
		return -1;
	}
	offset = ftello(cat->fid);
	fseeko(cat->fid, 0, SEEK_SET);
	old_header_end = cat->header_end;

	twomass_catalog_write_headers(cat);

	if (old_header_end != cat->header_end) {
		fprintf(stderr, "Warning: twomass_catalog header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)old_header_end,
				(unsigned long)cat->header_end);
		return -1;
	}
	fseek(cat->fid, offset, SEEK_SET);
	return 0;
}

static int twomass_catalog_refill_buffer(void* userdata, void* buffer, uint offset, uint n) {
	twomass_catalog* cat = userdata;
	twomass_entry* en = buffer;
	return twomass_catalog_read_entries(cat, offset, n, en);
}

twomass_catalog* twomass_catalog_new() {
	twomass_catalog* cat = calloc(1, sizeof(twomass_catalog));
	if (!cat) {
		fprintf(stderr, "Couldn't allocate memory for a twomass_catalog structure.\n");
		exit(-1);
	}
	cat->br.blocksize = 1000;
	cat->br.elementsize = sizeof(twomass_entry);
	cat->br.refill_buffer = twomass_catalog_refill_buffer;
	cat->br.userdata = cat;
	return cat;
}

static qfits_table* twomass_catalog_get_table() {
	uint datasize;
	uint ncols, nrows, tablesize;
	int col;
	qfits_table* table;

	if (!twomass_fitstruct_inited)
		init_twomass_fitstruct();

	// one big table: the sources.
	// dummy values here...
	datasize = 0;
	ncols = TWOMASS_FITS_COLUMNS;
	nrows = 0;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	table->tab_w = 0;
	for (col=0; col<TWOMASS_FITS_COLUMNS; col++) {
		fitstruct* fs = twomass_fitstruct + col;
		fits_add_column(table, col, fs->fitstype, 1, fs->units, fs->fieldname);
	}
	table->tab_w = qfits_compute_table_width(table);
	return table;
}

int twomass_catalog_read_entries(twomass_catalog* cat, uint offset,
								 uint count, twomass_entry* entries) {
	int c;
	if (!twomass_fitstruct_inited)
		init_twomass_fitstruct();
	for (c=0; c<TWOMASS_FITS_COLUMNS; c++) {
		assert(cat->columns[c] != -1);
		assert(cat->table);
		assert(cat->table->col[cat->columns[c]].atom_size == twomass_fitstruct[c].size);
		qfits_query_column_seq_to_array
			(cat->table, cat->columns[c], offset, count,
			 ((unsigned char*)entries) + twomass_fitstruct[c].offset,
			 sizeof(twomass_entry));
	}
	return 0;
}

twomass_entry* twomass_catalog_read_entry(twomass_catalog* cat) {
	twomass_entry* e = buffered_read(&cat->br);
	if (!e)
		fprintf(stderr, "Failed to read an Astrometry.net catalog entry.\n");
	return e;
}

int twomass_catalog_count_entries(twomass_catalog* cat) {
	return cat->table->nr;
}

int twomass_catalog_close(twomass_catalog* cat) {
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
	buffered_read_free(&cat->br);
	free(cat);
	return 0;
}

int twomass_catalog_write_entry(twomass_catalog* cat, twomass_entry* entry) {
	int c;
	FILE* fid = cat->fid;
	if (!twomass_fitstruct_inited)
		init_twomass_fitstruct();
	for (c=0; c<TWOMASS_FITS_COLUMNS; c++) {
		fitstruct* fs = twomass_fitstruct + c;
		if (fits_write_data(fid, ((unsigned char*)entry) + fs->offset,
							fs->fitstype))
			return -1;
	}
	cat->nentries++;
	return 0;
}
