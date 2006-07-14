#ifndef MATCHFILE_H
#define MATCHFILE_H

#include <stdio.h>

#include "matchobj.h"
#include "starutil.h"
#include "qfits.h"
#include "bl.h"
#include "ioutils.h"

#define MATCHFILE_AN_FILETYPE "MATCH"

#define MATCHFILE_FITS_COLUMNS 20

struct matchfile {
	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;

	// the current matchfile_entry
	qfits_table* table;
	int nrows;

	// when reading:
	char* fn;
	int columns[MATCHFILE_FITS_COLUMNS];

	// for buffered reading:
	bread br;
};
typedef struct matchfile matchfile;

void matchobj_compute_overlap(MatchObj* mo);


pl* matchfile_get_matches_for_field(matchfile* m, uint field);

matchfile* matchfile_open_for_writing(char* fn);

int matchfile_write_header(matchfile* m);

int matchfile_fix_header(matchfile* m);

int matchfile_write_match(matchfile* m, MatchObj* mo);


matchfile* matchfile_open(char* fn);

int matchfile_read_matches(matchfile* m, MatchObj* mo, uint offset, uint n);

MatchObj* matchfile_buffered_read_match(matchfile* m);

int matchfile_buffered_read_pushback(matchfile* m);

int matchfile_close(matchfile* m);

#endif
