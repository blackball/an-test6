#ifndef CATUTILS_H
#define CATUTILS_H

#include <sys/types.h>

struct catalog {
	void* mmap_cat;
	size_t mmap_cat_size;
	char* catfname;
	uint nstars;
	double ramin;
	double ramax;
	double decmin;
	double decmax;
	double* stars;
	int64_t* ids;
};
typedef struct catalog catalog;

// if "modifiable" is non-zero, a private copy-on-write is made.
// (changes don't automatically get written to the file.)

//catalog* catalog_open(char* basefn, int fits, int modifiable);

catalog* catalog_open(char* catfn, int fits, int modifiable);

double* catalog_get_star(catalog* cat, uint sid);

double* catalog_get_base(catalog* cat);

void catalog_close(catalog* cat);

void catalog_compute_radecminmax(catalog* cat);

int catalog_rewrite_header(catalog* cat);

int catalog_write_to_file(catalog* cat, char* fn, int fits);

#endif
