#ifndef STAR_KD_H
#define STAR_KD_H

#include "kdtree.h"
#include "qfits.h"

struct startree {
	kdtree_t* tree;
	/*
	  uint N; // == tree->ndata
	  uint D; // == tree->ndim
	*/
	qfits_header* header;
	int* inverse_perm;
};
typedef struct startree startree;


startree* startree_open(char* fn);

int startree_get(startree* s, uint starid, double* posn);

int startree_close(startree* s);

// for writing
startree* startree_new();

int startree_write_to_file(startree* s, char* fn);

#endif
