#ifndef MATCHFILE_H
#define MATCHFILE_H

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "matchobj.h"
#include "starutil.h"
#include "qfits.h"

#define MATCHFILE_AN_FILETYPE "MATCH"

#define MATCHFILE_FITS_COLUMNS 6

// a matchfile is structured as a set of tables.
// each table corresponds to a matchfile_entry
struct matchfile_entry {
	int fieldnum;
	bool parity;
	char* indexpath;
	char* fieldpath;
	double codetol;
};
typedef struct matchfile_entry matchfile_entry;


struct matchfile {
	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;

	// the current matchfile_entry
	qfits_table* table;
	qfits_header* tableheader;
	off_t tableheader_start;
	off_t tableheader_end;
	int nrows;

	// when reading:
	// the FITS extension being read.
	int extension;
	char* fn;
	int columns[MATCHFILE_FITS_COLUMNS];

	// for buffered reading:
	MatchObj* buffer;
	// number of elements in the buffer
	int nbuff;
	// offset of the start of the buffer
	int off;
	// index within the buffer
	int buffind;
};
typedef struct matchfile matchfile;

matchfile* matchfile_open_for_writing(char* fn);

int matchfile_write_header(matchfile* m);


int matchfile_start_table(matchfile* m, matchfile_entry* me);

int matchfile_write_table(matchfile* m);

int matchfile_fix_table(matchfile* m);


int matchfile_write_match(matchfile* m, MatchObj* mo);


matchfile* matchfile_open(char* fn);

int matchfile_next_table(matchfile* m, matchfile_entry* me);

int matchfile_read_matches(matchfile* m, MatchObj* mo, uint offset, uint n);

MatchObj* matchfile_buffered_read_match(matchfile* m);


int matchfile_close(matchfile* m);

#endif
