#ifndef FIELD_CHECK_H
#define FIELD_CHECK_H

#include "qfits.h"

#define FIELDCHECK_AN_FILETYPE "FC"

#define FIELDCHECK_FITS_COLUMNS 5

struct fieldcheck_file {
	// when writing:
	FILE* fid;
	off_t header_end;

	// when reading:
	char* fn;
	int columns[FIELDCHECK_FITS_COLUMNS];

	qfits_table* table;
	qfits_header* header;
	int nrows;
};
typedef struct fieldcheck_file fieldcheck_file;

struct fieldcheck {
	int filenum;
	int fieldnum;
	// in degrees
	double ra;
	double dec;
	// in arcsec
	float radius;
};
typedef struct fieldcheck fieldcheck;

fieldcheck_file* fieldcheck_file_open_for_writing(char* fn);

int fieldcheck_file_write_header(fieldcheck_file* f);

int fieldcheck_file_fix_header(fieldcheck_file* f);

int fieldcheck_file_write_entry(fieldcheck_file* f, fieldcheck* fc);

fieldcheck_file* fieldcheck_file_open(char* fn);

int fieldcheck_file_read_entries(fieldcheck_file* f, fieldcheck* fc,
								 uint offset, uint n);

int fieldcheck_file_close(fieldcheck_file* f);

#endif
