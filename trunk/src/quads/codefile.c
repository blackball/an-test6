#include <errno.h>
#include <string.h>

#include "codefile.h"
#include "starutil.h"
#include "fileutil.h"

double* codefile_read(FILE *fid, int *numcodes, int *Dim_Codes,
					  double *index_scale) {
    qidx ncodes;
    dimension dimcodes;
    sidx numstars;
    double* codearray = NULL;
    char readStatus;
    int nitems, nread;

    readStatus = read_code_header(fid, &ncodes, &numstars, 
				  &dimcodes, index_scale);
    if (readStatus == READ_FAIL)
	return NULL;

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

