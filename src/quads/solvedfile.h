#ifndef SOLVEDFILE_H
#define SOLVEDFILE_H

#include "bl.h"

int solvedfile_get(char* fn, int fieldnum);

il* solvedfile_getall(char* fn, int firstfield, int lastfield, int maxfields);

int solvedfile_set(char* fn, int fieldnum);

#endif
