#ifndef STAR_KD_H
#define STAR_KD_H

#include "kdtree.h"
#include "qfits.h"

struct startree {
	kdtree_t* tree;
	qfits_header* header;
	int* inverse_perm;
};
typedef struct startree startree;


startree* startree_open(char* fn);

int startree_N(startree* s);

int startree_nodes(startree* s);

int startree_D(startree* s);

qfits_header* startree_header(startree* s);

int startree_get(startree* s, uint starid, double* posn);

int startree_close(startree* s);

void startree_compute_inverse_perm(startree* s);

// for writing
startree* startree_new();

int startree_write_to_file(startree* s, char* fn);

#endif
