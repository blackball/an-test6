#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

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
    char readStatus;
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
	readStatus = read_quad_header(quadfid, &numquads, &numstars, &Dim_Quads, &index_scale);
	if (readStatus == (char)READ_FAIL)
		return NULL;
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


