#ifndef EZFITS_H
#define EZFITS_H


#include "fitsio.h"


int ezwriteimage(char* fn, int datatype, void* data, int w, int h);


#endif
