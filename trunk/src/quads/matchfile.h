#ifndef MATCHFILE_H
#define MATCHFILE_H

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
	// currently we don't use these!
	double fieldunits_lower;
	double fieldunits_upper;
};
typedef struct matchfile_entry matchfile_entry;

int matchfile_write_match(FILE* fid, MatchObj* mo, matchfile_entry* me);

int matchfile_read_match(FILE* fid, MatchObj** mo, matchfile_entry* me);

#endif
