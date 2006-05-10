#ifndef CATUTILS_H
#define CATUTILS_H

#include <sys/types.h>
#include "qfits.h"

struct catalog {
	FILE* fid;
	void* mmap_cat;
	size_t mmap_cat_size;
	uint numstars;
	double ramin;
	double ramax;
	double decmin;
	double decmax;
	double* stars;
	qfits_header* header;
	off_t header_end;
};
typedef struct catalog catalog;

// if "modifiable" is non-zero, a private copy-on-write is made.
// (changes don't automatically get written to the file.)

catalog* catalog_open(char* catfn, int modifiable);

catalog* catalog_open_for_writing(char* catfn);

double* catalog_get_star(catalog* cat, uint sid);

double* catalog_get_base(catalog* cat);

int catalog_write_star(catalog* cat, double* star);

void catalog_close(catalog* cat);

void catalog_compute_radecminmax(catalog* cat);

int catalog_write_header(catalog* cat);

int catalog_fix_header(catalog* cat);

int catalog_write_to_file(catalog* cat, char* fn);

#endif
