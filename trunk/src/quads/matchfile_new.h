#ifndef MATCHFILE_H
#define MATCHFILE_H

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "matchobj.h"
#include "starutil.h"
#include "qfits.h"

#define MATCHFILE_AN_FILETYPE "MATCH"

// a matchfile is structured as a set of tables.
// each table corresponds to a matchfile_entry
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


struct matchfile {
	int nfields;  // number of matchfile_entries
	int nmatches; // number of MatchObjs
	FILE* fid;
	qfits_header* header;
	off_t header_end;
};
typedef struct matchfile matchfile;

matchfile* matchfile_open(char* fn);



matchfile* matchfile_open_for_writing(char* fn);

int matchfile_write_header(matchfile* m);

int matchfile_fix_header(matchfile* m);




int matchfile_write_match(FILE* fid, MatchObj* mo, matchfile_entry* me);

int matchfile_read_match(FILE* fid, MatchObj** mo, matchfile_entry* me);

#endif
