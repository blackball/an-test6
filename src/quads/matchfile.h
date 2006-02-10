#ifndef MATCHFILE_H_
#define MATCHFILE_H_

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "matchobj.h"
#include "starutil.h"

struct matchfile_entry {
	int fieldnum;
	bool parity;
	char* indexpath;
	char* fieldpath;
	double codetol;
};
typedef struct matchfile_entry matchfile_entry;

int matchfile_write_match(FILE* fid, MatchObj* mo, matchfile_entry* me);

#endif
