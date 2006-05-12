#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "catalog.h"
#include "fitsioutils.h"
#include "fileutil.h"
#include "ioutils.h"
#include "mathutil.h"

int catalog_write_to_file(catalog* cat, char* fn)
{
	FILE *catfid = NULL;
	catfid = fopen(fn, "wb");
	if (!catfid) {
		fprintf(stderr, "Couldn't open catalog output file %s: %s\n",
		        fn, strerror(errno));
		return -1;
	}

	char val[256];
	qfits_table* table;
	qfits_header* tablehdr;
	qfits_header* hdr;
	uint datasize;
	uint ncols, nrows, tablesize;
	// the header
	hdr = qfits_table_prim_header_default();
	fits_add_endian(hdr);
	fits_add_double_size(hdr);
	sprintf(val, "%u", cat->numstars);
	qfits_header_add(hdr, "NSTARS", val, "Number of stars used.", NULL);
	qfits_header_add(hdr, "AN_FILE", "OBJS", "This file has a list of object positions.", NULL);
	qfits_header_dump(hdr, catfid);
	qfits_header_destroy(hdr);

	// first table: the star locations.
	datasize = DIM_STARS * sizeof(double);
	ncols = 1;
	// may be dummy
	nrows = cat->numstars;
	tablesize = datasize * nrows * ncols;
	table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "xyz", "", "", "", 0, 0, 0, 0, 0);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, catfid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);

	if (fwrite(cat->stars, sizeof(double), cat->numstars*DIM_STARS, catfid) !=
	        cat->numstars * DIM_STARS) {
		fprintf(stderr, "Failed to write catalog data to file %s: %s.\n",
		        fn, strerror(errno));
		fclose(catfid);
		return -1;
	}

	fits_pad_file(catfid);

	if (fclose(catfid)) {
		fprintf(stderr, "Couldn't close catalog file %s: %s\n",
		        fn, strerror(errno));
		return -1;
	}
	return 0;
}

int catalog_write_header(catalog* cat)
{
	qfits_table* table;
	qfits_header* tablehdr;
	char val[256];
	uint datasize;
	uint ncols, nrows, tablesize;
	char* fn;

	if (!cat->fid) {
		fprintf(stderr, "catalog_write_header: fid is null.\n");
		return -1;
	}

	// fill in the real values...
	sprintf(val, "%u", cat->numstars);
	qfits_header_mod(cat->header, "NSTARS", val, "Number of stars.");

	datasize = DIM_STARS * sizeof(double);
	ncols = 1;
	nrows = cat->numstars;
	tablesize = datasize * nrows * ncols;
	fn = "";
	table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "xyz",
	               "", "", "", 0, 0, 0, 0, 0);
	qfits_header_dump(cat->header, cat->fid);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, cat->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);
	return 0;
}

int catalog_fix_header(catalog* cat)
{
 	off_t offset;
	off_t old_header_end;

	if (!cat->fid) {
		fprintf(stderr, "quadfile_fits_fix_header: fid is null.\n");
		return -1;
	}
	offset = ftello(cat->fid);
	fseeko(cat->fid, 0, SEEK_SET);
	old_header_end = cat->header_end;

	catalog_write_header(cat);

	if (old_header_end != cat->header_end) {
		fprintf(stderr, "Warning: objfile header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)old_header_end,
				(unsigned long)cat->header_end);
		return -1;
	}
	fseek(cat->fid, offset, SEEK_SET);
	return 0;
}

void catalog_compute_radecminmax(catalog* cat)
{
	double ramin, ramax, decmin, decmax;
	int i;
	ramin = 1e100;
	ramax = -1e100;
	decmin = 1e100;
	decmax = -1e100;
	for (i = 0; i < cat->numstars; i++) {
		double* xyz;
		double ra, dec;
		xyz = catalog_get_star(cat, i);
		ra = xy2ra(xyz[0], xyz[1]);
		dec = z2dec(xyz[2]);
		if (ra > ramax)
			ramax = ra;
		if (ra < ramin)
			ramin = ra;
		if (dec > decmax)
			decmax = dec;
		if (dec < decmin)
			decmin = dec;
	}
	cat->ramin = ramin;
	cat->ramax = ramax;
	cat->decmin = decmin;
	cat->decmax = decmax;
}

double* catalog_get_base(catalog* cat)
{
	return cat->stars;
}

catalog* catalog_open(char* catfn, int modifiable)
{
	FILE *catfid = NULL;
	catalog* cat;
	int mode, flags;
	int offxyz = -1;

	cat = malloc(sizeof(catalog));
	if (!cat) {
		fprintf(stderr, "catalog_open: malloc failed.\n");
		return cat;
	}

	cat->fid = NULL;

	catfid = fopen(catfn, "rb");
	if (!catfid) {
		fprintf(stderr, "Couldn't open file %s to read quads: %s\n", catfn, strerror(errno));
		goto bail;
	}

	int sizexyz;
	qfits_header* header;

	if (!is_fits_file(catfn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", catfn);
		goto bail;
	}
	header = qfits_header_read(catfn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS header from %s.\n", catfn);
		goto bail;
	}
	cat->numstars = qfits_header_getint(header, "NSTARS", -1);
	if (fits_check_endian(header) ||
		fits_check_double_size(header)) {
		fprintf(stderr, "File %s was written with wrong endianness or double size.\n", catfn);
		qfits_header_destroy(header);
		goto bail;
	}
	qfits_header_destroy(header);
	if (cat->numstars == -1) {
		fprintf(stderr, "Couldn't find NSTARS header in file %s\n", catfn);
		goto bail;
	}

	if (fits_find_table_column(catfn, "xyz", &offxyz, &sizexyz)) {
		fprintf(stderr, "Couldn't find \"xyz\" column in FITS file.");
		goto bail;
	}

	if (fits_blocks_needed(cat->numstars * sizeof(double) * DIM_STARS) != sizexyz) {
		fprintf(stderr, "Number of stars promised does jive with the xyz table size: %u vs %u.\n",
			fits_blocks_needed(cat->numstars * sizeof(double) * DIM_STARS), sizexyz);
		goto bail;
	}

	cat->mmap_cat_size = offxyz + sizexyz;

	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}
	cat->mmap_cat = mmap(0, cat->mmap_cat_size, mode, flags, fileno(catfid), 0);
	if (cat->mmap_cat == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap catalogue file: %s\n", strerror(errno));
		goto bail;
	}
	fclose(catfid);

	cat->stars = (double*)(((char*)cat->mmap_cat) + offxyz);

	return cat;
bail:
	free(cat);
	return NULL;
}

catalog* catalog_open_for_writing(char* fn) 
{
	catalog* qf;
	qf = calloc(1, sizeof(catalog));
	if (!qf) {
		fprintf(stderr, "catalog_open_for_writing: malloc failed.\n");
		goto bailout;
	}

	qf->fid = fopen(fn, "wb");
	if (!qf->fid) {
		fprintf(stderr, "Couldn't open file %s for FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

	// the header
	qf->header = qfits_table_prim_header_default();
	fits_add_endian(qf->header);
	fits_add_double_size(qf->header);
	qfits_header_add(qf->header, "NSTARS", "0", "Number of stars used.", NULL);
	qfits_header_add(qf->header, "AN_FILE", "OBJS", "This file has a list of object positions.", NULL);
	qfits_header_add(qf->header, "", NULL, "This is a flat array of XYZ for each catalog star.", NULL);

	return qf;

bailout:
	if (qf) {
		if (qf->fid)
			fclose(qf->fid);
		free(qf);
	}
	return NULL;

}

double* catalog_get_star(catalog* cat, uint sid)
{
	if (sid >= cat->numstars) {
		fprintf(stderr, "catalog: asked for star %u, but catalog size is only %u.\n",
		        sid, cat->numstars);
		return NULL;
	}
	return cat->stars + sid * 3;
}

int catalog_write_star(catalog* cat, double* star)
{
	if (!cat->fid) {
		fprintf(stderr, "Couldn't write a star\n");
		assert(0);
		return *(int*)0x0;
	}

	if (fwrite(star, sizeof(double), DIM_STARS, cat->fid) != DIM_STARS) {
		fprintf(stderr, "Failed to write catalog data. No, I don't know what file it is you insensitive clod!: %s\n",
		        strerror(errno));
		return -1;
	}
	cat->numstars++;
	return 0;
}

void catalog_close(catalog* cat)
{
	if (cat->fid) {
		fits_pad_file(cat->fid);
		fclose(cat->fid); /* meow */
	}
	munmap(cat->mmap_cat, cat->mmap_cat_size);
	free(cat);
}

