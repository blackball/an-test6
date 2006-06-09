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
	idfile* qf = malloc(sizeof(idfile));
	if (!qf) {
		fprintf(stderr, "Couldn't malloc a idfile struct: %s\n", strerror(errno));
		return NULL;
	}
	memset(qf, 0, sizeof(idfile));
	return qf;
}

idfile* idfile_open(char* fn, int modifiable)
{
	FILE* fid = NULL;
	qfits_header* header = NULL;
	idfile* qf = NULL;
	int off, sizeanids;
	int size;
	void* map;
	int mode, flags;

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		goto bailout;
	}
	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read id file: %s\n", fn, strerror(errno));
		goto bailout;
	}
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS header from %s.\n", fn);
		goto bailout;
	}

	if (fits_check_endian(header)) {
		fprintf(stderr, "File %s was written with wrong endianness or uint size.\n", fn);
		goto bailout;
	}

	qf = new_idfile();
	if (!qf)
		goto bailout;

	qf->numstars = qfits_header_getint(header, "NSTARS", -1);
	qfits_header_destroy(header);

	if (qf->numstars == -1) {
		fprintf(stderr, "Couldn't find NUMSTARS of stars entries in FITS header.");
		goto bailout;
	}
	fprintf(stderr, "nstars %u\n", qf->numstars);

	if (fits_find_table_column(fn, "ids", &off, &sizeanids)) {
		fprintf(stderr, "Couldn't find \"ids\" column in FITS file.");
		goto bailout;
	}

	if (fits_blocks_needed(qf->numstars * sizeof(uint64_t)) != sizeanids) {
		fprintf(stderr, "Number of stars promised does jive with the table size: %u vs %u.\n",
		        fits_blocks_needed(qf->numstars * sizeof(uint64_t)), sizeanids);
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
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
		goto bailout;
	}

	qf->anidarray = (uint64_t*)(map + off);
	qf->mmap_base = map;
	qf->mmap_size = size;
	return qf;

bailout:
	if (qf)
		free(qf);
	if (fid)
		fclose(fid);
	return NULL;
}

int idfile_close(idfile* qf)
{
	int rtn = 0;
	if (qf->mmap_base)
		if (munmap(qf->mmap_base, qf->mmap_size)) {
			fprintf(stderr, "Error munmapping idfile: %s\n", strerror(errno));
			rtn = -1;
		}
	if (qf->fid) {
		fits_pad_file(qf->fid);
		if (fclose(qf->fid)) {
			fprintf(stderr, "Error closing idfile: %s\n", strerror(errno));
			rtn = -1;
		}
	}
	if (qf->header)
		qfits_header_destroy(qf->header);

	free(qf);
	return rtn;
}

idfile* idfile_open_for_writing(char* fn)
{
	idfile* qf;
	char val[256];

	qf = new_idfile();
	if (!qf)
		goto bailout;
	qf->fid = fopen(fn, "wb");
	if (!qf->fid) {
		fprintf(stderr, "Couldn't open file %s for FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

	// the header
	qf->header = qfits_table_prim_header_default();
	fits_add_endian(qf->header);

	// These may be placeholder values...
	sprintf(val, "%u", qf->numstars);
	qfits_header_add(qf->header, "AN_FILE", "ID", "This file lists Astrometry.net star IDs for catalog stars.", NULL);
	qfits_header_add(qf->header, "NSTARS", val, "Number of stars used.", NULL);
	qfits_header_add(qf->header, "COMMENT", "This is a flat array of ANIDs for each catalog star.", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", " (each A.N id is a native-endian uint64)", NULL, NULL);

	return qf;

bailout:
	if (qf) {
		if (qf->fid)
			fclose(qf->fid);
		free(qf);
	}
	return NULL;
}

int idfile_write_anid(idfile* qf, uint64_t anid)
{
	if (!qf->fid) {
		fprintf(stderr, "idfile_fits_write_anid: fid is null.\n");
		return -1;
	}
	if (fwrite(&anid, sizeof(uint64_t), 1, qf->fid) == 1) {
		qf->numstars++;
		return 0;
	}
	fprintf(stderr, "idfile_fits_write_anid: failed to write: %s\n", strerror(errno));
	return -1;
}

int idfile_fix_header(idfile* qf)
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

	if (!qf->fid) {
		fprintf(stderr, "idfile_fix_header: fid is null.\n");
		return -1;
	}

	offset = ftello(qf->fid);
	fseeko(qf->fid, 0, SEEK_SET);

	// fill in the real values...
	sprintf(val, "%u", qf->numstars);
	qfits_header_mod(qf->header, "NSTARS", val, "Number of stars.");

	dataptr = NULL;
	datasize = sizeof(uint64_t);
	ncols = 1;
	nrows = qf->numstars;
	tablesize = datasize * nrows * ncols;
	fn = "";
	table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "ids",
	               "", "", "", 0, 0, 0, 0, 0);
	qfits_header_dump(qf->header, qf->fid);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, qf->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);

	new_header_end = ftello(qf->fid);

	if (new_header_end != qf->header_end) {
		fprintf(stderr, "Warning: idfile header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)qf->header_end,
		        (unsigned long)new_header_end);
		return -1;
	}

	fseek(qf->fid, offset, SEEK_SET);

	fits_pad_file(qf->fid);

	return 0;
}

int idfile_write_header(idfile* qf)
{
	// first table: the quads.
	int datasize = sizeof(uint64_t);
	int ncols = 1;
	// may be dummy
	int nrows = qf->numstars;
	int tablesize = datasize * nrows * ncols;
	qfits_table* table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "ids", "", "", "", 0, 0, 0, 0, 0);
	qfits_header_dump(qf->header, qf->fid);
	qfits_header* tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, qf->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);
	qf->header_end = ftello(qf->fid);
	return 0;
}

uint64_t idfile_get_anid(idfile* qf, uint starid) 
{
	if (starid >= qf->numstars) {
		fprintf(stderr, "Requested quadid %i, but number of quads is %i. SKY IS FALLING\n",
		        starid, qf->numstars);
		assert(0);
		return *(int*)0x0; /* explode gracefully */
	}

	return qf->anidarray[starid];
}


