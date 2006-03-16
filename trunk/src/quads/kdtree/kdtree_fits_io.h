#ifndef KDTREE_FITS_IO_H
#define KDTREE_FITS_IO_H

#include "kdtree.h"

//kdtree_t* kdtree_fits_read(FILE* fin);

kdtree_t* kdtree_fits_read_file(char* fn);

//int kdtree_fits_write(FILE* fout, kdtree_t* kdtree);

int kdtree_fits_write_file(kdtree_t* kdtree, char* fn);

void kdtree_fits_close(kdtree_t* kd);


#endif
