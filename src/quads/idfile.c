#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "qfits.h"
#include "fitsioutils.h"
#include "idfile.h"
#include "starutil.h"
#include "fileutil.h"
#include "ioutils.h"

static idfile* new_idfile()
{
	idfile* id = calloc(1, sizeof(idfile));
	if (!id) {
		fflush(stdout);
		fprintf(stderr, "Couldn't malloc a idfile struct: %s\n", strerror(errno));
		return NULL;
	}
	id->healpix = -1;
	return id;
}

idfile* idfile_open(char* fn, int modifiable)
{
	FILE* fid = NULL;
	idfile* id = NULL;
	int off, sizeanids;
	int size;
	void* map;
	int mode, flags;
	qfits_header* header = NULL;

	if (!qfits_is_fits(fn)) {
		fflush(stdout);
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		goto bailout;
	}
	fid = fopen(fn, "rb");
	if (!fid) {
		fflush(stdout);
		fprintf(stderr, "Couldn't open file %s to read id file: %s\n", fn, strerror(errno));
		goto bailout;
	}
	header = qfits_header_read(fn);
	if (!header) {
		fflush(stdout);
		fprintf(stderr, "Couldn't read FITS header from %s.\n", fn);
		goto bailout;
	}

	if (fits_check_endian(header)) {
		fflush(stdout);
		fprintf(stderr, "File %s was written with wrong endianness or uint size.\n", fn);
		goto bailout;
	}

	id = new_idfile();
	if (!id)
		goto bailout;

	id->header = header;
	id->numstars = qfits_header_getint(header, "NSTARS", -1);
	id->healpix = qfits_header_getint(header, "HEALPIX", -1);

	if (id->numstars == -1) {
		fflush(stdout);
		fprintf(stderr, "Couldn't find NUMSTARS of stars entries in FITS header.");
		goto bailout;
	}
	//fprintf(stderr, "nstars %u\n", id->numstars);

	if (fits_find_table_column(fn, "ids", &off, &sizeanids)) {
		fflush(stdout);
		fprintf(stderr, "Couldn't find \"ids\" column in FITS file.");
		goto bailout;
	}

	if (fits_blocks_needed(id->numstars * sizeof(uint64_t)) != sizeanids) {
		fflush(stdout);
		fprintf(stderr, "Number of stars promised does jive with the table size: %u vs %u.\n",
		        fits_blocks_needed(id->numstars * sizeof(uint64_t)), sizeanids);
		goto bailout;
	}

	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}
	size = off + sizeanids;
	map = mmap(0, size, mode, flags, fileno(fid), 0);
	fclose(fid);
	fid = NULL;
	if (map == MAP_FAILED) {
		fflush(stdout);
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
		goto bailout;
	}

	id->anidarray = (uint64_t*)(map + off);
	id->mmap_base = map;
	id->mmap_size = size;
	return id;

bailout:
	if (header)
		qfits_header_destroy(header);
	if (id)
		free(id);
	if (fid)
		fclose(fid);
	return NULL;
}

int idfile_close(idfile* id)
{
	int rtn = 0;
	if (id->mmap_base)
		if (munmap(id->mmap_base, id->mmap_size)) {
			fflush(stdout);
			fprintf(stderr, "Error munmapping idfile: %s\n", strerror(errno));
			rtn = -1;
		}
	if (id->fid) {
		fits_pad_file(id->fid);
		if (fclose(id->fid)) {
			fflush(stdout);
			fprintf(stderr, "Error closing idfile: %s\n", strerror(errno));
			rtn = -1;
		}
	}
	if (id->header)
		qfits_header_destroy(id->header);

	free(id);
	return rtn;
}

idfile* idfile_open_for_writing(char* fn)
{
	idfile* id;

	id = new_idfile();
	if (!id)
		goto bailout;
	id->fid = fopen(fn, "wb");
	if (!id->fid) {
		fflush(stdout);
		fprintf(stderr, "Couldn't open file %s for FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

	// the header
	id->header = qfits_table_prim_header_default();
	fits_add_endian(id->header);

	// These are be placeholder values...
	qfits_header_add(id->header, "AN_FILE", "ID", "This file lists Astrometry.net star IDs for catalog stars.", NULL);
	qfits_header_add(id->header, "NSTARS", "0", "Number of stars used.", NULL);
	qfits_header_add(id->header, "HEALPIX", "-1", "Healpix covered by this file.", NULL);
	qfits_header_add(id->header, "COMMENT", "This is a flat array of ANIDs for each catalog star.", NULL, NULL);
	qfits_header_add(id->header, "COMMENT", " (each A.N id is a native-endian uint64)", NULL, NULL);

	return id;

bailout:
	if (id) {
		if (id->fid)
			fclose(id->fid);
		free(id);
	}
	return NULL;
}

int idfile_write_anid(idfile* id, uint64_t anid)
{
	if (!id->fid) {
		fflush(stdout);
		fprintf(stderr, "idfile_fits_write_anid: fid is null.\n");
		return -1;
	}
	if (fwrite(&anid, sizeof(uint64_t), 1, id->fid) == 1) {
		id->numstars++;
		return 0;
	}
	fflush(stdout);
	fprintf(stderr, "idfile_fits_write_anid: failed to write: %s\n", strerror(errno));
	return -1;
}

int idfile_fix_header(idfile* id)
{
	off_t offset;
	off_t new_header_end;
	qfits_table* table;
	qfits_header* tablehdr;
	char val[256];
	void* dataptr;
	uint datasize;
	uint ncols, nrows, tablesize;
	char* fn;

	if (!id->fid) {
		fflush(stdout);
		fprintf(stderr, "idfile_fix_header: fid is null.\n");
		return -1;
	}

	offset = ftello(id->fid);
	fseeko(id->fid, 0, SEEK_SET);

	// fill in the real values...
	sprintf(val, "%u", id->numstars);
	qfits_header_mod(id->header, "NSTARS", val, "Number of stars.");
	sprintf(val, "%i", id->healpix);
	qfits_header_mod(id->header, "HEALPIX", val, "Healpix covered by this file.");

	dataptr = NULL;
	datasize = sizeof(uint64_t);
	ncols = 1;
	nrows = id->numstars;
	tablesize = datasize * nrows * ncols;
	fn = "";
	table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "ids",
	               "", "", "", 0, 0, 0, 0, 0);
	qfits_header_dump(id->header, id->fid);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, id->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);

	new_header_end = ftello(id->fid);

	if (new_header_end != id->header_end) {
		fflush(stdout);
		fprintf(stderr, "Warning: idfile header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)id->header_end,
		        (unsigned long)new_header_end);
		return -1;
	}

	fseek(id->fid, offset, SEEK_SET);

	fits_pad_file(id->fid);

	return 0;
}

int idfile_write_header(idfile* id)
{
	// first table: the quads.
	int datasize = sizeof(uint64_t);
	int ncols = 1;
	// may be dummy
	int nrows = id->numstars;
	int tablesize = datasize * nrows * ncols;
	qfits_table* table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "ids", "", "", "", 0, 0, 0, 0, 0);
	qfits_header_dump(id->header, id->fid);
	qfits_header* tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, id->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);
	id->header_end = ftello(id->fid);
	return 0;
}

uint64_t idfile_get_anid(idfile* id, uint starid) 
{
	if (starid >= id->numstars) {
		fflush(stdout);
		fprintf(stderr, "Requested quadid %i, but number of quads is %i. SKY IS FALLING\n",
		        starid, id->numstars);
		assert(0);
		return *(int*)0x0; /* explode gracefully */
	}
	return id->anidarray[starid];
}

