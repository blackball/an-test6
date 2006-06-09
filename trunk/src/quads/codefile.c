#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "codefile.h"
#include "starutil.h"
#include "fileutil.h"
#include "ioutils.h"
#include "fitsioutils.h"

static codefile* new_codefile()
{
	codefile* cf = malloc(sizeof(codefile));
	if (!cf) {
		fprintf(stderr, "Couldn't malloc a codefile struct: %s\n", strerror(errno));
		return NULL;
	}
	memset(cf, 0, sizeof(codefile));
	return cf;
}


void codefile_get_code(codefile* cf, uint codeid,
                       double* cx, double* cy, double* dx, double* dy)
{
	*cx = cf->codearray[codeid * DIM_CODES + 0];
	*cy = cf->codearray[codeid * DIM_CODES + 1];
	*dx = cf->codearray[codeid * DIM_CODES + 2];
	*dy = cf->codearray[codeid * DIM_CODES + 3];
}

int codefile_close(codefile* cf)
{
	int rtn = 0;
	if (cf->mmap_code)
		if (munmap(cf->mmap_code, cf->mmap_code_size)) {
			fprintf(stderr, "Error munmapping codefile: %s\n", strerror(errno));
			rtn = -1;
		}
	if (cf->fid) {
		fits_pad_file(cf->fid);
		if (fclose(cf->fid)) {
			fprintf(stderr, "Error closing codefile: %s\n", strerror(errno));
			rtn = -1;
		}
	}
	if (cf->header)
		qfits_header_destroy(cf->header);

	free(cf);
	return rtn;
}

codefile* codefile_open(char* fn, int modifiable)
{
	codefile* cf = NULL;
	FILE* fid = NULL;
	qfits_header* header = NULL;
	int offcodes, sizecodes;
	int mode, flags;

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		goto bailout;
	}
	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read codes: %s\n", fn, strerror(errno));
		goto bailout;
	}
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS code header from %s.\n", fn);
		goto bailout;
	}

	if (fits_check_endian(header) ||
	        fits_check_double_size(header)) {
		fprintf(stderr, "File %s was written with wrong endianness or double size.\n", fn);
		goto bailout;
	}

	cf = new_codefile();
	if (!cf)
		goto bailout;

	cf->numcodes = qfits_header_getint(header, "NCODES", -1);
	cf->numstars = qfits_header_getint(header, "NSTARS", -1);
	cf->index_scale = qfits_header_getdouble(header, "SCALE_U", -1.0);
	cf->index_scale_lower = qfits_header_getdouble(header, "SCALE_L", -1.0);
	cf->header = header;

	if ((cf->numcodes == -1) || (cf->numstars == -1) ||
	        (cf->index_scale == -1.0) || (cf->index_scale_lower == -1.0)) {
		fprintf(stderr, "Couldn't find NCODES or NSTARS or SCALE_U or SCALE_L entries in FITS header.");
		goto bailout;
	}
	fprintf(stderr, "ncodes %u, nstars %u.\n", cf->numcodes, cf->numstars);

	if (fits_find_table_column(fn, "codes", &offcodes, &sizecodes)) {
		fprintf(stderr, "Couldn't find \"codes\" column in FITS file.");
		goto bailout;
	}

	if (fits_blocks_needed(cf->numcodes * sizeof(double) * DIM_CODES) != sizecodes) {
		fprintf(stderr, "Number of codes promised does jive with the table size: %u vs %u.\n",
		        fits_blocks_needed(cf->numcodes * sizeof(double) * DIM_CODES), sizecodes);
		goto bailout;
	}

	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}

	cf->mmap_code_size = offcodes + sizecodes;
	cf->mmap_code = mmap(0, cf->mmap_code_size, mode, flags, fileno(fid), 0);
	fclose(fid);
	fid = NULL;
	if (cf->mmap_code == MAP_FAILED) {
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
		goto bailout;
	}

	cf->codearray = (double*)(((char*)cf->mmap_code) + offcodes);
	return cf;

bailout:
	if (cf) {
		if (cf->header)
			qfits_header_destroy(cf->header);
		free(cf);
	}
	if (fid)
		fclose(fid);
	return NULL;
}

codefile* codefile_open_for_writing(char* fn)
{
	codefile* cf;

	cf = new_codefile();
	if (!cf)
		goto bailout;
	cf->fid = fopen(fn, "wb");
	if (!cf->fid) {
		fprintf(stderr, "Couldn't open file %s for code FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

	// the header
	cf->header = qfits_table_prim_header_default();
	fits_add_endian(cf->header);
	fits_add_double_size(cf->header);
	// placeholder values.
	qfits_header_add(cf->header, "AN_FILE", "CODE", "This file lists the code for each quad.", NULL);
	qfits_header_add(cf->header, "NCODES", "0", "", NULL);
	qfits_header_add(cf->header, "NSTARS", "0", "", NULL);
	qfits_header_add(cf->header, "SCALE_U", "0.0", "", NULL);
	qfits_header_add(cf->header, "SCALE_L", "0.0", "", NULL);
	qfits_header_add(cf->header, "COMMENT", "The first extension contains the codes ", NULL, NULL);
	qfits_header_add(cf->header, "COMMENT", " stored as 4 native-{endian,size} doubles.", NULL, NULL);
	qfits_header_add(cf->header, "COMMENT", " (ie, the quad location in 4-D code space.", NULL, NULL);

	return cf;

bailout:
	if (cf) {
		if (cf->fid)
			fclose(cf->fid);
		free(cf);
	}
	return NULL;
}

int codefile_write_header(codefile* cf) {
	qfits_table* table;
	qfits_header* tablehdr;
	char val[256];
	uint datasize;
	uint ncols, nrows, tablesize;
	char* fn;

	if (!cf->fid)
		return -1;

	// fill in the real values...
	sprintf(val, "%u", cf->numcodes);
	qfits_header_mod(cf->header, "NCODES", val, "Number of codes.");
	sprintf(val, "%u", cf->numstars);
	qfits_header_mod(cf->header, "NSTARS", val, "Number of stars.");
	sprintf(val, "%.10f", cf->index_scale);
	qfits_header_mod(cf->header, "SCALE_U", val, "Upper-bound index scale.");
	sprintf(val, "%.10f", cf->index_scale_lower);
	qfits_header_mod(cf->header, "SCALE_L", val, "Lower-bound index scale.");

	datasize = DIM_CODES * sizeof(double);
	ncols = 1;
	nrows = cf->numcodes;
	tablesize = datasize * nrows * ncols;
	fn = "";
	table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "codes",
	               "", "", "", 0, 0, 0, 0, 0);
	qfits_header_dump(cf->header, cf->fid);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, cf->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);

	cf->header_end = ftello(cf->fid);
	return 0;
}

int codefile_fix_header(codefile* cf)
{
 	off_t offset;
	off_t old_header_end;

	if (!cf->fid) {
		fprintf(stderr, "codefile_fits_fix_header: fid is null.\n");
		return -1;
	}
	offset = ftello(cf->fid);
	fseeko(cf->fid, 0, SEEK_SET);
	old_header_end = cf->header_end;

	codefile_write_header(cf);

	if (old_header_end != cf->header_end) {
		fprintf(stderr, "Warning: codefile header used to end at %lu, "
		        "now it ends at %lu.\n", (unsigned long)old_header_end,
				(unsigned long)cf->header_end);
		return -1;
	}
	fseek(cf->fid, offset, SEEK_SET);
	return 0;
}

int codefile_write_code(codefile* cf, double Cx, double Cy, double Dx, double Dy)
{
	FILE *fid = cf->fid;
	if (write_double(fid, Cx) ||
		write_double(fid, Cy) ||
		write_double(fid, Dx) ||
		write_double(fid, Dy)) {
		fprintf(stderr, "Error writing a code.\n");
		return -1;
	}
	cf->numcodes++;
	return 0;
}
