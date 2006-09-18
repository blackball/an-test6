#ifndef CODE_KD_H
#define CODE_KD_H

#include "kdtree.h"
#include "qfits.h"

struct codetree {
	kdtree_t* tree;
	qfits_header* header;
	int* inverse_perm;
};
typedef struct codetree codetree;


codetree* codetree_open(char* fn);

int codetree_get(codetree* s, uint codeid, double* code);

int codetree_N(codetree* s);

int codetree_nodes(codetree* s);

int codetree_D(codetree* s);

int codetree_get_permuted(codetree* s, int index);

qfits_header* codetree_header(codetree* s);

int codetree_close(codetree* s);

// for writing
codetree* codetree_new();

int codetree_write_to_file(codetree* s, char* fn);

#endif
