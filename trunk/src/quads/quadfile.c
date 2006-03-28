#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "qfits.h"
#include "fitsioutils.h"
#include "quadfile.h"
#include "starutil.h"
#include "fileutil.h"
#include "ioutils.h"

quadfile* quadfile_fits_open_for_writing(char* fn);
quadfile* quadfile_fits_open(char* fn, int modifiable);
int quadfile_fits_fix_header(quadfile* qf);
int quadfile_fits_write_quad(quadfile* qf,
							 uint iA, uint iB, uint iC, uint iD);


static quadfile* new_quadfile() {
	quadfile* qf = malloc(sizeof(quadfile));
	if (!qf) {
		fprintf(stderr, "Couldn't malloc a quadfile struct: %s\n", strerror(errno));
		return NULL;
	}
	memset(qf, 0, sizeof(quadfile));
	return qf;
}

int quadfile_write_quad(quadfile *qf, uint iA, uint iB, uint iC, uint iD) {
    FILE* fid;
    if (qf->fits) {
        return quadfile_fits_write_quad(qf, iA, iB, iC, iD);
    }
    fid = qf->fid;
    if (write_u32(fid, iA) ||
        write_u32(fid, iB) ||
        write_u32(fid, iC) ||
        write_u32(fid, iD)) {
        fprintf(stderr, "Couldn't write a quad.\n");
        return -1;
    }
    return 0;
}

int read_quad_header(FILE *fid, uint *numquads, uint *numstars,
                     uint *DimQuads, double *index_scale) {
    uint magic;
	if (read_u16(fid, &magic)) {
		fprintf(stderr, "Couldn't read quad header.\n");
		return 1;
	}
	if (magic != MAGIC_VAL) {
		fprintf(stderr, "Error reading quad header: bad magic value.\n");
		return 1;
	}
    if (read_u32(fid, numquads) ||
        read_u16(fid, DimQuads) ||
        read_double(fid, index_scale) ||
        read_u32(fid, numstars)) {
		fprintf(stderr, "Couldn't read quad header.\n");
		return 1;
	}
	return 0;
}

int quadfile_write_header(FILE *fid, uint numQuads, uint numstars,
                          uint DimQuads, double index_scale) {
    magicval magic = MAGIC_VAL;
    if (write_u16(fid, magic) ||
        write_u32(fid, numQuads) ||
        write_u16(fid, DimQuads) ||
        write_double(fid, index_scale) ||
        write_u32(fid, numstars)) {
        fprintf(stderr, "Couldn't write quad header.\n");
        return -1;
    }
    return 0;
}

int quadfile_fix_header(quadfile* qf) {
    off_t off;
    int rtn;
    if (qf->fits) {
        return quadfile_fits_fix_header(qf);
    }
    off = ftello(qf->fid);
    // go back to the beginning...
    fseek(qf->fid, 0, SEEK_SET);
    rtn = quadfile_write_header(qf->fid, qf->numquads, qf->numstars,
                                DIM_QUADS, qf->index_scale);
    // go back from whence we came...
    fseek(qf->fid, off, SEEK_SET);
    return rtn;
}

quadfile* quadfile_open(char* quadfname, int fits, int modifiable) {
	quadfile* qf;
    uint Dim_Quads;
	FILE* quadfid;
	uint numquads, numstars;
	off_t headeroffset;
	off_t endoffset;
	uint maxquad;
	uint mmap_quad_size;
	void* mmap_quad;
	double index_scale;
	uint* quadarray;
	int mode, flags;

    if (fits) {
        return quadfile_fits_open(quadfname, modifiable);
    }

	// Read .quad file...
	quadfid = fopen(quadfname, "rb");
	if (!quadfid) {
		fprintf(stderr, "Couldn't open quad file %s: %s\n", quadfname, strerror(errno));
		return NULL;
	}
	if (read_quad_header(quadfid, &numquads, &numstars, &Dim_Quads, &index_scale)) {
		fprintf(stderr, "Couldn't read quad header.\n");
		return NULL;
	}
	if (Dim_Quads != DIM_QUADS) {
		fprintf(stderr, "Quad file %s is wrong: Dim_Quads=%i, not %i.\n", quadfname, Dim_Quads, DIM_QUADS);
		return NULL;
	}
	headeroffset = ftello(quadfid);
	// check that the quads file is the right size.
	fseeko(quadfid, 0, SEEK_END);
	endoffset = ftello(quadfid);
	maxquad = (endoffset - headeroffset) / (DIM_QUADS * sizeof(uint));
	if (maxquad != numquads) {
		fprintf(stderr, "Error: numquads=%u (specified in .quad file header) does "
				"not match maxquad=%u (computed from size of .quad file).\n",
				numquads, maxquad);
		return NULL;
	}

	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}
	mmap_quad_size = endoffset;
	mmap_quad = mmap(0, mmap_quad_size, mode, flags, fileno(quadfid), 0);
	if (mmap_quad == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap quad file: %s\n", strerror(errno));
		return NULL;
	}
	fclose(quadfid);
	quadarray = (uint*)(((char*)(mmap_quad)) + headeroffset);

	qf = new_quadfile();
	if (!qf)
		return NULL;
	qf->numquads = numquads;
	qf->numstars = numstars;
	qf->index_scale = index_scale;
	qf->mmap_quad = mmap_quad;
	qf->mmap_quad_size = mmap_quad_size;
	qf->quadarray = quadarray;
	return qf;
}

quadfile* quadfile_fits_open(char* fn, int modifiable) {
	FILE* fid = NULL;
	qfits_header* header = NULL;
    quadfile* qf = NULL;
    int offquads, sizequads;
	int size;
	void* map;
	int mode, flags;

	if (!is_fits_file(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
        goto bailout;
	}
	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read quads: %s\n", fn, strerror(errno));
        goto bailout;
	}
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS quad header from %s.\n", fn);
        goto bailout;
	}

    if (fits_check_endian(header) ||
        fits_check_uint_size(header)) {
		fprintf(stderr, "File %s was written with wrong endianness or uint size.\n", fn);
        goto bailout;
    }

    qf = new_quadfile();
    if (!qf)
        goto bailout;

    qf->fits = 1;
    qf->numquads = qfits_header_getint(header, "NQUADS", -1);
    qf->numstars = qfits_header_getint(header, "NSTARS", -1);
    qf->index_scale = qfits_header_getdouble(header, "SCALE_U", -1.0);
    qf->index_scale_lower = qfits_header_getdouble(header, "SCALE_L", -1.0);

	qfits_header_destroy(header);

	if ((qf->numquads == -1) || (qf->numstars == -1) ||
		(qf->index_scale == -1.0) || (qf->index_scale_lower == -1.0)) {
		fprintf(stderr, "Couldn't find NQUADS or NSTARS or SCALE_U or SCALE_L entries in FITS header.");
        goto bailout;
	}
	fprintf(stderr, "nquads %u, nstars %u.\n", qf->numquads, qf->numstars);

    if (fits_find_table_column(fn, "quads", &offquads, &sizequads)) {
        fprintf(stderr, "Couldn't find \"quads\" column in FITS file.");
        goto bailout;
    }

	if (fits_blocks_needed(qf->numquads * sizeof(uint) * DIM_QUADS) != sizequads) {
        fprintf(stderr, "Number of quads promised does jive with the table size: %u vs %u.\n",
                fits_blocks_needed(qf->numquads * sizeof(uint) * DIM_QUADS), sizequads);
        goto bailout;
    }

	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}
    size = offquads + sizequads;
	map = mmap(0, size, mode, flags, fileno(fid), 0);
	fclose(fid);
    fid = NULL;
	if (map == MAP_FAILED) {
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
        goto bailout;
	}

    qf->quadarray = (uint*)(map + offquads);
    qf->mmap_quad = map;
    qf->mmap_quad_size = size;
    return qf;

 bailout:
    if (qf)
        free(qf);
    if (fid)
        fclose(fid);
    return NULL;
}

int quadfile_close(quadfile* qf) {
    int rtn = 0;
	if (qf->mmap_quad)
		if (munmap(qf->mmap_quad, qf->mmap_quad_size)) {
            fprintf(stderr, "Error munmapping quadfile: %s\n", strerror(errno));
            rtn = -1;
        }
	if (qf->fid)
		if (fclose(qf->fid)) {
			fprintf(stderr, "Error closing quadfile: %s\n", strerror(errno));
            rtn = -1;
        }
    if (qf->header)
        qfits_header_destroy(qf->header);

	free(qf);
    return rtn;
}

quadfile* quadfile_open_for_writing(char* fn, int fits) {
	quadfile* qf;

    if (fits) {
        return quadfile_fits_open_for_writing(fn);
    }

	qf = new_quadfile();
	if (!qf)
		goto bailout;

	qf->fid = fopen(fn, "wb");
	if (!qf->fid) {
		fprintf(stderr, "Couldn't open file %s for quad FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

    quadfile_write_header(qf->fid, 0, 0, DIM_QUADS, 0.0);

    return qf;

 bailout:
	if (qf) {
		if (qf->fid)
			fclose(qf->fid);
		free(qf);
	}
	return NULL;

}

quadfile* quadfile_fits_open_for_writing(char* fn) {
	quadfile* qf;
    char val[256];
    qfits_table* table;
    //qfits_header* header;
    qfits_header* tablehdr;
	void* dataptr;
	uint datasize;
	uint ncols, nrows, tablesize;

	qf = new_quadfile();
	if (!qf)
		goto bailout;
    qf->fits = 1;
	qf->fid = fopen(fn, "wb");
	if (!qf->fid) {
		fprintf(stderr, "Couldn't open file %s for quad FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

    // the header
    qf->header = qfits_table_prim_header_default();
    fits_add_endian(qf->header);
    fits_add_uint_size(qf->header);
	// these may be placeholder values...
	sprintf(val, "%u", qf->numquads);
	qfits_header_add(qf->header, "NQUADS", val, "Number of quads.", NULL);
	sprintf(val, "%u", qf->numstars);
	qfits_header_add(qf->header, "NSTARS", val, "Number of stars used (or zero).", NULL);
	sprintf(val, "%.10f", qf->index_scale);
	qfits_header_add(qf->header, "SCALE_U", val, "Upper-bound index scale.", NULL);
	sprintf(val, "%.10f", qf->index_scale_lower);
	qfits_header_add(qf->header, "SCALE_L", val, "Lower-bound index scale.", NULL);
	qfits_header_add(qf->header, "", NULL, "The first extension contains the quads ", NULL);
	qfits_header_add(qf->header, "", NULL, " stored as 4 native-endian uints.", NULL);

    // first table: the quads.
    dataptr = NULL; //qf->quadarray;
    datasize = DIM_QUADS * sizeof(uint);
    ncols = 1;
	// may be dummy
    nrows = qf->numquads;
    tablesize = datasize * nrows * ncols;

    table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
    qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
				   "quads",
				   "", "", "", 0, 0, 0, 0, 0);
    qfits_header_dump(qf->header, qf->fid);
	tablehdr = qfits_table_ext_header_default(table);
    qfits_header_dump(tablehdr, qf->fid);
    qfits_table_close(table);
    qfits_header_destroy(tablehdr);

	qf->header_end = ftello(qf->fid);

	/*
	  table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
	  qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	  "quads",
	  "", "", "", 0, 0, 0, 0, 0);
	  //qfits_save_table_hdrdump(&dataptr, table, header);
	  qfits_header_dump(header, qf->fid);

	  qfits_table_append_xtension(qf->fid, table, &dataptr);
	  qfits_table_close(table);
	  qfits_header_destroy(header);
	*/

	return qf;

 bailout:
	if (qf) {
		if (qf->fid)
			fclose(qf->fid);
		free(qf);
	}
	return NULL;
}

int quadfile_fits_write_quad(quadfile* qf,
							 uint iA, uint iB, uint iC, uint iD) {
	uint abcd[4];
	if (!qf->fid) {
		fprintf(stderr, "quadfile_fits_write_quad: fid is null.\n");
		return -1;
	}
	abcd[0] = iA;
	abcd[1] = iB;
	abcd[2] = iC;
	abcd[3] = iD;
	if (fwrite(abcd, sizeof(uint), 4, qf->fid) == 4)
		return 0;
	fprintf(stderr, "quadfile_fits_write_quad: failed to write: %s\n", strerror(errno));
	return -1;
}

int quadfile_fits_fix_header(quadfile* qf) {
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

	if (!qf->fid) {
		fprintf(stderr, "quadfile_fits_fix_header: fid is null.\n");
		return -1;
	}

	offset = ftello(qf->fid);
	fseeko(qf->fid, 0, SEEK_SET);

	// fill in the real values...
	sprintf(val, "%u", qf->numquads);
	qfits_header_mod(qf->header, "NQUADS", val, "Number of quads.");
	sprintf(val, "%u", qf->numstars);
	qfits_header_mod(qf->header, "NSTARS", val, "Number of stars.");
	sprintf(val, "%.10f", qf->index_scale);
	qfits_header_mod(qf->header, "SCALE_U", val, "Upper-bound index scale.");
	sprintf(val, "%.10f", qf->index_scale_lower);
	qfits_header_mod(qf->header, "SCALE_L", val, "Lower-bound index scale.");

    dataptr = NULL;
    datasize = DIM_QUADS * sizeof(uint);
    ncols = 1;
    nrows = qf->numquads;
    tablesize = datasize * nrows * ncols;
	fn="";
    table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
    qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
				   "quads",
				   "", "", "", 0, 0, 0, 0, 0);
    qfits_header_dump(qf->header, qf->fid);
	tablehdr = qfits_table_ext_header_default(table);
    qfits_header_dump(tablehdr, qf->fid);
    qfits_table_close(table);
    qfits_header_destroy(tablehdr);
    //qfits_header_destroy(header);

	new_header_end = ftello(qf->fid);

	if (new_header_end != qf->header_end) {
		fprintf(stderr, "Warning: quadfile header used to end at %lu, "
				"now it ends at %lu.\n", (unsigned long)qf->header_end,
				(unsigned long)new_header_end);
		return -1;
	}

	fseek(qf->fid, offset, SEEK_SET);

	// pad with zeros up to a multiple of 2880 bytes.
	npad = (offset % FITS_BLOCK_SIZE);
	if (npad) {
		char nil = '\0';
		int i;
		npad = FITS_BLOCK_SIZE - npad;
		for (i=0; i<npad; i++)
			fwrite(&nil, 1, 1, qf->fid);
	}

	return 0;
}

double quadfile_get_index_scale_arcsec(quadfile* qf) {
	return qf->index_scale * (180.0 / M_PI) * 60 * 60;
}

double quadfile_get_index_scale_lower_arcsec(quadfile* qf) {
	return qf->index_scale_lower * (180.0 / M_PI) * 60 * 60;
}

void quadfile_get_starids(quadfile* qf, uint quadid,
						  uint* starA, uint* starB,
						  uint* starC, uint* starD) {

	if (quadid >= qf->numquads) {
		fprintf(stderr, "Requested quadid %i, but number of quads is %i.\n",
				quadid, qf->numquads);
		return;
	}

	*starA = qf->quadarray[quadid * DIM_QUADS + 0];
	*starB = qf->quadarray[quadid * DIM_QUADS + 1];
	*starC = qf->quadarray[quadid * DIM_QUADS + 2];
	*starD = qf->quadarray[quadid * DIM_QUADS + 3];
}


