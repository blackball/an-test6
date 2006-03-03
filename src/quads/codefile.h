#ifndef CODEFILE_H_
#define CODEFILE_H_

#include <stdio.h>

double* codefile_read(FILE *fid, int *numcodes, int *Dim_Codes,
					  double *index_scale);

#endif
