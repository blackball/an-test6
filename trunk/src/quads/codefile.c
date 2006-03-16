#include <errno.h>
#include <string.h>

#include "codefile.h"
#include "starutil.h"
#include "fileutil.h"
#include "ioutils.h"

int read_code_header(FILE *fid, uint *numcodes, uint *numstars,
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

int write_code_header(FILE *fid, uint numCodes,
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

int fix_code_header(FILE* fid, uint numcodes) {
    off_t off;
    magicval magic = MAGIC_VAL;
    int rtn;
    off = ftello(fid);
    // go back to the beginning...
    fseek(fid, 0, SEEK_SET);
    rtn = 0;
    if (write_u16(fid, magic) ||
        write_u32(fid, numcodes)) {
        fprintf(stderr, "Couldn't fix code header.\n");
        rtn = -1;
    }
    // go back from whence we came...
    fseek(fid, off, SEEK_SET);
    return rtn;
}

int readonecode(FILE *fid, double *Cx, double *Cy, double *Dx, double *Dy) {
    if (read_double(fid, Cx) ||
        read_double(fid, Cy) ||
        read_double(fid, Dx) ||
        read_double(fid, Dy)) {
        fprintf(stderr, "Error reading a code.\n");
        return -1;
    }
	return 0;
}

int writeonecode(FILE *fid, double Cx, double Cy, double Dx, double Dy) {
    if (write_double(fid, Cx) ||
        write_double(fid, Cy) ||
        write_double(fid, Dx) ||
        write_double(fid, Dy)) {
        fprintf(stderr, "Error writing a code.\n");
        return -1;
    }
    return 0;
}

double* codefile_read(FILE *fid, int *numcodes, int *Dim_Codes,
					  double *index_scale) {
    uint ncodes;
    uint dimcodes;
    uint numstars;
    double* codearray = NULL;
    int nitems, nread;

    if (read_code_header(fid, &ncodes, &numstars, 
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

