#ifndef SOLVEDFILE_H
#define SOLVEDFILE_H

#include "bl.h"

int solvedfile_get(char* fn, int fieldnum);

/**
   Get a list of unsolved fields between "firstfield" and
   "lastfield", up to a maximum of "maxfields" (no limit if
   "maxfields" is zero).
 */
il* solvedfile_getall(char* fn, int firstfield, int lastfield, int maxfields);

/**
   Same as "getall" except return solved fields.
 */
il* solvedfile_getall_solved(char* fn, int firstfield, int lastfield, int maxfields);

int solvedfile_set(char* fn, int fieldnum);

#endif
