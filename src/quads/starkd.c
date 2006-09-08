#include <stdio.h>
#include <stdlib.h>

#include "starkd.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "kdtree_access.h"
#include "starutil.h"

static startree* startree_alloc() {
	startree* s = calloc(1, sizeof(startree));
	if (!s) {
		fprintf(stderr, "Failed to allocate a star kdtree struct.\n");
		return NULL;
	}
	return s;
}

int startree_N(startree* s) {
	return s->tree->ndata;
}

int startree_nodes(startree* s) {
	return s->tree->nnodes;
}

int startree_D(startree* s) {
	return s->tree->ndim;
}

qfits_header* startree_header(startree* s) {
	return s->header;
}

startree* startree_open(char* fn) {
	startree* s;

	s = startree_alloc();
	if (!s)
		return s;

	s->header = qfits_header_read(fn);
	if (!s->header) {
		fprintf(stderr, "Failed to read FITS header from star kdtree file %s.\n", fn);
		goto bailout;
	}

	s->tree = kdtree_fits_read_file(fn);
	if (!s->tree) {
		fprintf(stderr, "Failed to read star kdtree from file %s\n", fn);
		goto bailout;
	}

	/*
	  s->N = s->tree->ndata;
	  s->D = s->tree->ndim;
	*/

	return s;

 bailout:
 	if (s->header)
		qfits_header_destroy(s->header);
	free(s);
	return NULL;
}

int startree_close(startree* s) {
	if (!s) return 0;
	if (s->inverse_perm)
		free(s->inverse_perm);
 	if (s->header)
		qfits_header_destroy(s->header);
	if (s->tree)
		kdtree_close(s->tree);
	free(s);
	return 0;
}

static int Ndata(startree* s) {
	return s->tree->ndata;
}

void startree_compute_inverse_perm(startree* s) {
	// compute inverse permutation vector.
	s->inverse_perm = malloc(Ndata(s) * sizeof(int));
	if (!s->inverse_perm) {
		fprintf(stderr, "Failed to allocate star kdtree inverse permutation vector.\n");
		return;
	}
	kdtree_inverse_permutation(s->tree, s->inverse_perm);
}

int startree_get(startree* s, uint starid, double* posn) {
	if (s->tree->perm && !s->inverse_perm) {
		startree_compute_inverse_perm(s);
		if (!s->inverse_perm)
			return -1;
	}
	if (starid >= Ndata(s)) {
		fprintf(stderr, "Invalid star ID: %u >= %u.\n", starid, Ndata(s));
		return -1;
	}
	if (s->inverse_perm)
		memcpy(posn, s->tree->data + s->inverse_perm[starid] * DIM_STARS,
			   DIM_STARS * sizeof(double));
	else
		memcpy(posn, s->tree->data + starid * DIM_STARS,
			   DIM_STARS * sizeof(double));
	return 0;
}

startree* startree_new() {
	startree* s = startree_alloc();
	s->header = qfits_header_default();
	if (!s->header) {
		fprintf(stderr, "Failed to create a qfits header for star kdtree.\n");
		free(s);
		return NULL;
	}
	qfits_header_add(s->header, "AN_FILE", "SKDT", "This file is a star kdtree.", NULL);
	return s;
}

int startree_write_to_file(startree* s, char* fn) {
	return kdtree_fits_write_file(s->tree, fn, s->header);
}

