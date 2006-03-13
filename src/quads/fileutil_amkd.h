#ifndef FILEUTIL_AMKD_H
#define FILEUTIL_AMKD_H

#include "fileutil_am.h"

#include "KD/kdtree.h"

kdtree *read_starkd(FILE *treefid, double *ramin, double *ramax,
                    double *decmin, double *decmax);
kdtree *read_codekd(FILE *treefid, double *index_scale);
void write_starkd(FILE *treefid, kdtree *starkd,
                  double ramin, double ramax, double decmin, double decmax);
void write_codekd(FILE *treefid, kdtree *codekd, double index_scale);

#endif
