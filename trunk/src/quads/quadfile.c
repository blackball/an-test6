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

int read_one_quad(FILE *fid, uint *iA, uint *iB, uint *iC, uint *iD) {
    if (read_u32(fid, iA) ||
        read_u32(fid, iB) ||
        read_u32(fid, iC) ||
        read_u32(fid, iD)) {
        fprintf(stderr, "Couldn't read a quad.\n");
        return -1;
    }
    return 0;
}

int write_one_quad(FILE *fid, uint iA, uint iB, uint iC, uint iD) {
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

int write_quad_header(FILE *fid, uint numQuads, uint numstars,
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

int fix_quad_header(FILE *fid, uint numQuads) {
    off_t off;
    magicval magic = MAGIC_VAL;
    int rtn;
    off = ftello(fid);
    // go back to the beginning...
    fseek(fid, 0, SEEK_SET);
    rtn = 0;
    if (write_u16(fid, magic) ||
        write_u32(fid, numQuads)) {
        fprintf(stderr, "Couldn't fix quad header.\n");
        rtn = -1;
    }
    // go back from whence we came...
    fseek(fid, off, SEEK_SET);
    return rtn;
}

quadfile* quadfile_open(char* quadfname) {
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
	mmap_quad_size = endoffset;
	mmap_quad = mmap(0, mmap_quad_size, PROT_READ, MAP_SHARED, fileno(quadfid), 0);
	if (mmap_quad == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap quad file: %s\n", strerror(errno));
		return NULL;
	}
	fclose(quadfid);
	quadarray = (uint*)(((char*)(mmap_quad)) + headeroffset);

	qf = malloc(sizeof(quadfile));
	if (!qf) {
		fprintf(stderr, "Couldn't malloc a quadfile struct: %s\n", strerror(errno));
		return NULL;
	}
	qf->numquads = numquads;
	qf->numstars = numstars;
	qf->index_scale = index_scale;
	qf->mmap_quad = mmap_quad;
	qf->mmap_quad_size = mmap_quad_size;
	qf->quadarray = quadarray;
	return qf;
}

quadfile* quadfile_fits_open(char* fn) {
	FILE* fid = NULL;
	qfits_header* header = NULL;
    quadfile* qf = NULL;
    int offquads, sizequads;
	int size;
	void* map;

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

    qf = malloc(sizeof(quadfile));
    if (!qf) {
        fprintf(stderr, "Couldn't allocate a quadfile struct.\n");
        goto bailout;
    }

    qf->numquads = qfits_header_getint(header, "NQUADS", -1);
    qf->numstars = qfits_header_getint(header, "NSTARS", -1);
    qf->index_scale = qfits_header_getdouble(header, "INDEX_SCALE", -1.0);

	qfits_header_destroy(header);

	if ((qf->numquads == -1) || (qf->numstars == -1) || (qf->index_scale == -1.0)) {
		fprintf(stderr, "Couldn't find NQUADS or NSTARS or INDEX_SCALE entries in FITS header.");
        goto bailout;
	}
	fprintf(stderr, "nquads %u, nstars %u.\n", qf->numquads, qf->numstars);

    if (fits_find_table_column(fn, "quads", &offquads, &sizequads)) {
        fprintf(stderr, "Couldn't find \"quads\" column in FITS file.");
        goto bailout;
    }

	if (fits_blocks_needed(qf->numquads * sizeof(uint) * 4) != sizequads) {
        fprintf(stderr, "Number of quads promised does jive with the table size: %u vs %u.\n",
                fits_blocks_needed(qf->numquads * sizeof(uint) * 4), sizequads);
        goto bailout;
    }

    size = offquads + sizequads;

	map = mmap(0, size, PROT_READ, MAP_SHARED, fileno(fid), 0);
	fclose(fid);
    fid = NULL;
	if (map == MAP_FAILED) {
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
        goto bailout;
	}

    qf->quadarray = (uint*)map;
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

void quadfile_close(quadfile* qf) {
	munmap(qf->mmap_quad, qf->mmap_quad_size);
	free(qf);
}

double quadfile_get_index_scale_arcsec(quadfile* qf) {
	return qf->index_scale * (180.0 / M_PI) * 60 * 60;
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


