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

static quadfile* new_quadfile() {
	quadfile* qf = calloc(1, sizeof(quadfile));
	if (!qf) {
		fprintf(stderr, "Couldn't malloc a quadfile struct: %s\n", strerror(errno));
		return NULL;
	}
	qf->healpix = -1;
	return qf;
}

quadfile* quadfile_open(char* fn, int modifiable) {
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

    qf->numquads = qfits_header_getint(header, "NQUADS", -1);
    qf->numstars = qfits_header_getint(header, "NSTARS", -1);
    qf->index_scale = qfits_header_getdouble(header, "SCALE_U", -1.0);
    qf->index_scale_lower = qfits_header_getdouble(header, "SCALE_L", -1.0);
	qf->indexid = qfits_header_getint(header, "INDEXID", 0);
	qf->healpix = qfits_header_getint(header, "HEALPIX", -1);
	qf->header = header;

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
    if (qf) {
		if (qf->header)
			qfits_header_destroy(qf->header);
        free(qf);
	}
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
	if (qf->fid) {
		fits_pad_file(qf->fid);
		if (fclose(qf->fid)) {
			fprintf(stderr, "Error closing quadfile: %s\n", strerror(errno));
			rtn = -1;
		}
	}
    if (qf->header)
        qfits_header_destroy(qf->header);

	free(qf);
    return rtn;
}

quadfile* quadfile_open_for_writing(char* fn) {
	quadfile* qf;

	qf = new_quadfile();
	if (!qf)
		goto bailout;
	qf->fid = fopen(fn, "wb");
	if (!qf->fid) {
		fprintf(stderr, "Couldn't open file %s for quad FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

    // the header
    qf->header = qfits_table_prim_header_default();
    fits_add_endian(qf->header);
    fits_add_uint_size(qf->header);

	qfits_header_add(qf->header, "AN_FILE", "QUAD", "This file lists, for each quad, its four stars.", NULL);
	qfits_header_add(qf->header, "NQUADS", "0", "Number of quads.", NULL);
	qfits_header_add(qf->header, "NSTARS", "0", "Number of stars used (or zero).", NULL);
	qfits_header_add(qf->header, "SCALE_U", "0.0", "Upper-bound index scale.", NULL);
	qfits_header_add(qf->header, "SCALE_L", "0.0", "Lower-bound index scale.", NULL);
	qfits_header_add(qf->header, "INDEXID", "0", "Index unique ID.", NULL);
	qfits_header_add(qf->header, "HEALPIX", "-1", "Healpix of this index.", NULL);
	qfits_header_add(qf->header, "COMMENT", "The first extension contains the quads ", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", " stored as 4 native-{endian,size} uints.", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", " (one for each star's index in the corresponding objs file)", NULL, NULL);

	return qf;

 bailout:
	if (qf) {
		if (qf->fid)
			fclose(qf->fid);
		free(qf);
	}
	return NULL;
}

int quadfile_write_quad(quadfile* qf,
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
	if (fwrite(abcd, sizeof(uint), 4, qf->fid) != 4) {
		fprintf(stderr, "quadfile_fits_write_quad: failed to write: %s\n", strerror(errno));
		return -1;
	}
	qf->numquads++;
	return 0;
}

int quadfile_fix_header(quadfile* qf) {
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
	sprintf(val, "%i", qf->indexid);
	qfits_header_mod(qf->header, "INDEXID", val, "Index unique ID.");
	sprintf(val, "%i", qf->healpix);
	qfits_header_mod(qf->header, "HEALPIX", val, "Healpix of this index.");


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

	new_header_end = ftello(qf->fid);

	if (new_header_end != qf->header_end) {
		fprintf(stderr, "Warning: quadfile header used to end at %lu, "
				"now it ends at %lu.\n", (unsigned long)qf->header_end,
				(unsigned long)new_header_end);
		return -1;
	}

	fseek(qf->fid, offset, SEEK_SET);

	fits_pad_file(qf->fid);

	return 0;
}

int quadfile_write_header(quadfile* qf) {
    // first table: the quads.
    int datasize = DIM_QUADS * sizeof(uint);
    int ncols = 1;
    int nrows = qf->numquads;
    int tablesize = datasize * nrows * ncols;
    qfits_table* table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
    qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
				   "quads", "", "", "", 0, 0, 0, 0, 0);
    qfits_header_dump(qf->header, qf->fid);
	qfits_header* tablehdr = qfits_table_ext_header_default(table);
    qfits_header_dump(tablehdr, qf->fid);
    qfits_table_close(table);
    qfits_header_destroy(tablehdr);
	qf->header_end = ftello(qf->fid);
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


