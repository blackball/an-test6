#ifndef BOILERPLATE_H
#define BOILERPLATE_H

#include <stdio.h>

#include "qfits.h"

void boilerplate_help_header(FILE* fid);

void boilerplate_add_fits_headers(qfits_header* hdr);

#endif
