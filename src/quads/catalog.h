#ifndef CATUTILS_H
#define CATUTILS_H

typedef unsigned int uint;

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
};
typedef struct catalog catalog;

catalog* catalog_open(char* basefn);

double* catalog_get_star(catalog* cat, uint sid);

double* catalog_get_base(catalog* cat);

void catalog_close(catalog* cat);

#endif
