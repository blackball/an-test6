#ifndef CODEFILE_H_
#define CODEFILE_H_

#include <stdio.h>
#include "starutil.h"

int readonecode(FILE *fid, double *Cx, double *Cy, double *Dx, double *Dy);

int writeonecode(FILE *fid, double Cx, double Cy, double Dx, double Dy);

double* codefile_read(FILE *fid, int *numcodes, int *Dim_Codes,
					  double *index_scale);

int write_code_header(FILE *fid, uint numCodes,
                      uint numstars, uint DimCodes, double index_scale);

int fix_code_header(FILE* fid, uint numcodes);

#endif
