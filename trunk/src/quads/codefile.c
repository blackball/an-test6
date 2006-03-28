#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "codefile.h"
#include "starutil.h"
#include "fileutil.h"
#include "ioutils.h"
#include "fitsioutils.h"

int codefile_read_header(FILE *fid, uint *numcodes, uint *numstars,
                     uint *DimCodes, double *index_scale);
int codefile_write_header(FILE *fid, uint numCodes,
                      uint numstars, uint DimCodes, double index_scale);
codefile* codefile_fits_open(char* fn, int modifiable);
codefile* codefile_fits_open_for_writing(char* fn);


static codefile* new_codefile() {
	codefile* cf = malloc(sizeof(codefile));
	if (!cf) {
		fprintf(stderr, "Couldn't malloc a codefile struct: %s\n", strerror(errno));
		return NULL;
	}
	memset(cf, 0, sizeof(codefile));
	return cf;
}


void codefile_get_code(codefile* cf, uint codeid,
                       double* cx, double* cy, double* dx, double* dy) {
    *cx = cf->codearray[codeid * DIM_CODES + 0];
    *cy = cf->codearray[codeid * DIM_CODES + 1];
    *dx = cf->codearray[codeid * DIM_CODES + 2];
    *dy = cf->codearray[codeid * DIM_CODES + 3];
}

int codefile_close(codefile* cf) {
    int rtn = 0;
	if (cf->mmap_code)
		if (munmap(cf->mmap_code, cf->mmap_code_size)) {
            fprintf(stderr, "Error munmapping codefile: %s\n", strerror(errno));
            rtn = -1;
        }
	if (cf->fid)
		if (fclose(cf->fid)) {
			fprintf(stderr, "Error closing codefile: %s\n", strerror(errno));
            rtn = -1;
        }
    if (cf->header)
        qfits_header_destroy(cf->header);

	free(cf);
    return rtn;
}

codefile* codefile_open(char* fn, int fits, int modifiable) {
    codefile* cf = NULL;
    uint dim;
	off_t headeroffset;
	off_t endoffset;
	uint maxcode;
    FILE* fid;
	int mode, flags;

    if (fits)
        return codefile_fits_open(fn, modifiable);
    cf = new_codefile();
    if (!cf)
        goto bailout;

    fid = fopen(fn, "rb");
    if (!fid) {
        fprintf(stderr, "Couldn't open codefile %s: %s\n", fn, strerror(errno));
        goto bailout;
    }

    if (codefile_read_header(fid, &cf->numcodes, &cf->numstars, &dim,
                             &cf->index_scale)) {
        fprintf(stderr, "Couldn't read codefile header.\n");
        goto bailout;
    }
    if (dim != DIM_CODES) {
        fprintf(stderr, "Something is very wrong with codefile %s: dim=%i, not %i.\n", fn, dim, DIM_CODES);
        goto bailout;
    }

	// check that the file is the right size.
	headeroffset = ftello(fid);
	fseeko(fid, 0, SEEK_END);
	endoffset = ftello(fid);
	maxcode = (endoffset - headeroffset) / (DIM_CODES * sizeof(double));
	if (maxcode != cf->numcodes) {
		fprintf(stderr, "Error: numcodes=%u (specified in .code file header) does "
				"not match maxcode=%u (computed from size of .code file).\n",
				cf->numcodes, maxcode);
        goto bailout;
	}
	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}
	cf->mmap_code_size = endoffset;
	cf->mmap_code = mmap(0, cf->mmap_code_size, mode, flags, fileno(fid), 0);
	if (cf->mmap_code == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap code file: %s\n", strerror(errno));
        goto bailout;
	}
	fclose(fid);
    fid = NULL;
	cf->codearray = (double*)(((char*)(cf->mmap_code)) + headeroffset);

	return cf;

 bailout:
    if (cf)
        free(cf);
    if (fid)
        fclose(fid);
    return NULL;
}

codefile* codefile_fits_open(char* fn, int modifiable) {
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

    cf->fits = 1;
    cf->numcodes = qfits_header_getint(header, "NCODES", -1);
    cf->numstars = qfits_header_getint(header, "NSTARS", -1);
    cf->index_scale = qfits_header_getdouble(header, "SCALE_U", -1.0);
    cf->index_scale_lower = qfits_header_getdouble(header, "SCALE_L", -1.0);

	qfits_header_destroy(header);

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
    if (cf)
        free(cf);
    if (fid)
        fclose(fid);
    return NULL;
}

codefile* codefile_open_for_writing(char* fn, int fits) {
    codefile* cf;

    if (fits)
        return codefile_fits_open_for_writing(fn);

	cf = new_codefile();
	if (!cf)
		goto bailout;

	cf->fid = fopen(fn, "wb");
	if (!cf->fid) {
		fprintf(stderr, "Couldn't open file %s for code FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}
    codefile_write_header(cf->fid, 0, 0, DIM_CODES, 0.0);
    return cf;

 bailout:
	if (cf) {
		if (cf->fid)
			fclose(cf->fid);
		free(cf);
	}
	return NULL;
}

codefile* codefile_fits_open_for_writing(char* fn) {
	codefile* cf;
    char val[256];
    qfits_table* table;
    qfits_header* tablehdr;
	void* dataptr;
	uint datasize;
	uint ncols, nrows, tablesize;

	cf = new_codefile();
	if (!cf)
		goto bailout;
    cf->fits = 1;
	cf->fid = fopen(fn, "wb");
	if (!cf->fid) {
		fprintf(stderr, "Couldn't open file %s for code FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

    // the header
    cf->header = qfits_table_prim_header_default();
    fits_add_endian(cf->header);
    fits_add_double_size(cf->header);
	// these may be placeholder values...
	sprintf(val, "%u", cf->numcodes);
	qfits_header_add(cf->header, "NCODES", val, "Number of codes.", NULL);
	sprintf(val, "%u", cf->numstars);
	qfits_header_add(cf->header, "NSTARS", val, "Number of stars used (or zero).", NULL);
	sprintf(val, "%.10f", cf->index_scale);
	qfits_header_add(cf->header, "SCALE_U", val, "Upper-bound index scale.", NULL);
	sprintf(val, "%.10f", cf->index_scale_lower);
	qfits_header_add(cf->header, "SCALE_L", val, "Lower-bound index scale.", NULL);
	qfits_header_add(cf->header, "", NULL, "The first extension contains the codes ", NULL);
	qfits_header_add(cf->header, "", NULL, " stored as 4 native-endian doubles.", NULL);

    // first table: the codes.
    dataptr = NULL;
    datasize = DIM_CODES * sizeof(double);
    ncols = 1;
	// may be dummy
    nrows = cf->numcodes;
    tablesize = datasize * nrows * ncols;

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
	return cf;

 bailout:
	if (cf) {
		if (cf->fid)
			fclose(cf->fid);
		free(cf);
	}
	return NULL;
}

int codefile_fits_fix_header(codefile* cf) {
	off_t offset;
	off_t new_header_end;
    qfits_table* table;
    qfits_header* tablehdr;
	char val[256];
	void* dataptr;
	uint datasize;
	uint ncols, nrows, tablesize;
	char* fn;
	int npad;

	if (!cf->fid)
		return -1;

	offset = ftello(cf->fid);
	fseeko(cf->fid, 0, SEEK_SET);

	// fill in the real values...
	sprintf(val, "%u", cf->numcodes);
	qfits_header_mod(cf->header, "NCODES", val, "Number of codes.");
	sprintf(val, "%u", cf->numstars);
	qfits_header_mod(cf->header, "NSTARS", val, "Number of stars.");
	sprintf(val, "%.10f", cf->index_scale);
	qfits_header_mod(cf->header, "SCALE_U", val, "Upper-bound index scale.");
	sprintf(val, "%.10f", cf->index_scale_lower);
	qfits_header_mod(cf->header, "SCALE_L", val, "Lower-bound index scale.");

    dataptr = NULL;
    datasize = DIM_CODES * sizeof(double);
    ncols = 1;
    nrows = cf->numcodes;
    tablesize = datasize * nrows * ncols;
	fn="";
    table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
    qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
				   "codes",
				   "", "", "", 0, 0, 0, 0, 0);
    qfits_header_dump(cf->header, cf->fid);
	tablehdr = qfits_table_ext_header_default(table);
    qfits_header_dump(tablehdr, cf->fid);
    qfits_table_close(table);
    qfits_header_destroy(tablehdr);
    //qfits_header_destroy(header);

	new_header_end = ftello(cf->fid);

	if (new_header_end != cf->header_end) {
		fprintf(stderr, "Warning: codefile header used to end at %lu, "
				"now it ends at %lu.\n", (unsigned long)cf->header_end,
				(unsigned long)new_header_end);
		return -1;
	}

	fseek(cf->fid, offset, SEEK_SET);

	// pad with zeros up to a multiple of 2880 bytes.
	npad = (offset % FITS_BLOCK_SIZE);
	if (npad) {
		char nil = '\0';
		int i;
		npad = FITS_BLOCK_SIZE - npad;
		for (i=0; i<npad; i++)
			fwrite(&nil, 1, 1, cf->fid);
	}

	return 0;
}

int codefile_read_header(FILE *fid, uint *numcodes, uint *numstars,
                         uint *DimCodes, double *index_scale) {
    uint magic;
	if (read_u16(fid, &magic)) {
		fprintf(stderr, "Couldn't read code header.\n");
		return 1;
	}
	if (magic != MAGIC_VAL) {
		fprintf(stderr, "Error reading code header: bad magic value.\n");
		return 1;
	}
    if (read_u32(fid, numcodes) ||
        read_u16(fid, DimCodes) ||
        read_double(fid, index_scale) ||
        read_u32(fid, numstars)) {
		fprintf(stderr, "Couldn't read code header.\n");
		return 1;
	}
	return 0;
}

int codefile_write_header(FILE *fid, uint numCodes,
                          uint numstars, uint DimCodes, double index_scale) {
    magicval magic = MAGIC_VAL;
    if (write_u16(fid, magic) ||
        write_u32(fid, numCodes) ||
        write_u16(fid, DimCodes) ||
        write_double(fid, index_scale) ||
        write_u32(fid, numstars)) {
        fprintf(stderr, "Couldn't write code header.\n");
        return -1;
    }
    return 0;
}

int codefile_fix_header(codefile* cf) {
    off_t off;
    int rtn;

    if (cf->fits)
        return codefile_fits_fix_header(cf);

    off = ftello(cf->fid);
    // go back to the beginning...
    fseek(cf->fid, 0, SEEK_SET);
    rtn = 0;
    if (codefile_write_header(cf->fid, cf->numcodes, cf->numstars, DIM_CODES, cf->index_scale)) {
        fprintf(stderr, "Couldn't fix code header.\n");
        rtn = -1;
    }
    // go back from whence we came...
    fseek(cf->fid, off, SEEK_SET);
    return rtn;
}

/*
  int read_one_code(FILE *fid, double *Cx, double *Cy, double *Dx, double *Dy) {
  if (read_double(fid, Cx) ||
  read_double(fid, Cy) ||
  read_double(fid, Dx) ||
  read_double(fid, Dy)) {
  fprintf(stderr, "Error reading a code.\n");
  return -1;
  }
  return 0;
  }
*/

int codefile_write_code(codefile* cf, double Cx, double Cy, double Dx, double Dy) {
    FILE *fid = cf->fid;
    if (write_double(fid, Cx) ||
        write_double(fid, Cy) ||
        write_double(fid, Dx) ||
        write_double(fid, Dy)) {
        fprintf(stderr, "Error writing a code.\n");
        return -1;
    }
    return 0;
}

/*
  double* codefile_read(FILE *fid, int *numcodes, int *Dim_Codes,
  double *index_scale) {
  uint ncodes;
  uint dimcodes;
  uint numstars;
  double* codearray = NULL;
  int nitems, nread;
  if (codefile_read_header(fid, &ncodes, &numstars, 
  &dimcodes, index_scale)) {
  fprintf(stderr, "Couldn't read code file header.\n");
  return NULL;
  }
  *numcodes = ncodes;
  *Dim_Codes = dimcodes;
  fprintf(stderr, " %i x %i codes...", *numcodes, *Dim_Codes);
  fflush(stderr);
  nitems = (*numcodes) * (*Dim_Codes);
  codearray = malloc(nitems * sizeof(double));
  if (!codearray) {
  fprintf(stderr, "Couldn't allocate code array: %i x %i\n", *numcodes, *Dim_Codes);
  return NULL;
  }
  nread = fread(codearray, sizeof(double), nitems, fid);
  if (nread != nitems) {
  fprintf(stderr, "Couldn't read %i codes: %s\n", nitems, strerror(errno));
  free(codearray);
  return NULL;
  }
  return codearray;
  }
*/
