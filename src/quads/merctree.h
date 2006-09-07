#ifndef MERCTREE_H
#define MERCTREE_H

#include "kdtree.h"
#include "qfits.h"

struct merc_flux {
	float rflux;
	float bflux;
	float nflux;
};
typedef struct merc_flux merc_flux;

struct merc_cached_stats {
	merc_flux flux;
};
typedef struct merc_cached_stats merc_stats;

struct merctree {
	kdtree_t* tree;
	merc_stats* stats;
	merc_flux* flux;
	qfits_header* header;
	int* inverse_perm;
};
typedef struct merctree merctree;


merctree* merctree_open(char* fn);

//int merctree_get(merctree* s, uint mercid, double* posn);

int merctree_close(merctree* s);

//void merctree_compute_inverse_perm(merctree* s);

void merctree_compute_stats(merctree* m);

// for writing
merctree* merctree_new();

int merctree_write_to_file(merctree* s, char* fn);

#endif
