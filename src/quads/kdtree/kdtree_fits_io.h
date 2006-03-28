#ifndef KDTREE_FITS_IO_H
#define KDTREE_FITS_IO_H

#include "kdtree.h"

kdtree_t* kdtree_fits_read_file(char* fn);

int kdtree_fits_write_file(kdtree_t* kdtree, char* fn);

#endif
