#ifndef MATCHFILE_H
#define MATCHFILE_H

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "matchobj.h"
#include "starutil.h"
#include "qfits.h"

#define MATCHFILE_AN_FILETYPE "MATCH"

#define MATCHFILE_FITS_COLUMNS 8

// a matchfile is structured as a set of tables.
// each table corresponds to a matchfile_entry
struct matchfile_entry {
	int fieldnum;
	bool parity;
	char* indexpath;
	char* fieldpath;
	/*
	  double codetol;
	  // currently we don't use these!
	  double fieldunits_lower;
	  double fieldunits_upper;
	*/
};
typedef struct matchfile_entry matchfile_entry;


struct matchfile {
	/*
	  int nfields;  // number of matchfile_entries
	  int nmatches; // number of MatchObjs
	*/
	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;

	// the current matchfile_entry
	qfits_header* meheader;
};
typedef struct matchfile matchfile;

matchfile* matchfile_open_for_writing(char* fn);

int matchfile_write_header(matchfile* m);

int matchfile_fix_header(matchfile* m);


int matchfile_write_table(matchfile* m, matchfile_entry* me);

int matchfile_fix_table(matchfile* m);


int matchfile_write_match(matchfile* m, MatchObj* mo);

int matchfile_close(matchfile* m);


matchfile* matchfile_open(char* fn);

int matchfile_read_table(matchfile* m, matchfile_entry* me);

int matchfile_read_match(matchfile* m, MatchObj* mo);

int matchfile_read_matches(matchfile* m, MatchObj* mo, int n);

#endif
