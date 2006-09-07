#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "merctree.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "kdtree_access.h"
#include "kdtree_macros.h"
#include "starutil.h"

static merctree* merctree_alloc() {
	merctree* s = calloc(1, sizeof(merctree));
	if (!s) {
		fprintf(stderr, "Failed to allocate a merctree struct.\n");
		return NULL;
	}
	return s;
}

static void merctree_stats_tablesize(kdtree_t* kd, extra_table* tab) {
	tab->nitems = kd->nnodes;
}

static void merctree_flux_tablesize(kdtree_t* kd, extra_table* tab) {
	tab->nitems = kd->ndata;
}

merctree* merctree_open(char* fn) {
	merctree* s;
	extra_table extras[2];
	extra_table* stats = extras;
	extra_table* fluxes = extras + 1;
	
	s = merctree_alloc();
	if (!s)
		return s;

	s->header = qfits_header_read(fn);
	if (!s->header) {
		fprintf(stderr, "Failed to read FITS header from merc kdtree file %s.\n", fn);
		goto bailout;
	}

	memset(extras, 0, sizeof(extras));

	stats->name = "merc_stats";
	stats->datasize = sizeof(merc_stats);
	stats->nitems = 0;
	stats->required = 1;
	stats->compute_tablesize = merctree_stats_tablesize;

	fluxes->name = "merc_flux";
	fluxes->datasize = sizeof(merc_flux);
	fluxes->nitems = 0;
	fluxes->required = 1;
	fluxes->compute_tablesize = merctree_flux_tablesize;

	s->tree = kdtree_fits_read_file_extras(fn, extras, 2);
	if (!s->tree) {
		fprintf(stderr, "Failed to read merc kdtree from file %s\n", fn);
		goto bailout;
	}

	s->stats = stats->ptr;
	s->flux  = fluxes->ptr;

	return s;

 bailout:
	if (s->tree)
		kdtree_close(s->tree);
 	if (s->header)
		qfits_header_destroy(s->header);
	free(s);
	return NULL;
}

int merctree_close(merctree* s) {
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

static int Ndata(merctree* s) {
	return s->tree->ndata;
}

void merctree_compute_stats(merctree* m) {
	int i;
	kdtree_t* kd = m->tree;
	if (kd->perm) {
		fprintf(stderr, "Error: merctree_compute_stats requires a permuted tree.\n");
		assert(0);
		return;
	}
	for (i=kd->nnodes; i>=0; i--) {
		kdtree_node_t* node = NODE(i);
		merc_flux* stats = &(m->stats[i].flux);
		if (ISLEAF(i)) {
			int j;
			stats->rflux = stats->bflux = stats->nflux = 0.0;
			for (j=node->l; j<=node->r; j++) {
				stats->rflux += m->flux[j].rflux;
				stats->bflux += m->flux[j].bflux;
				stats->nflux += m->flux[j].nflux;
			}
		} else {
			merc_flux *flux1, *flux2;
			flux1 = m->flux + CHILD_INDEX_NEG(i);
			flux2 = m->flux + CHILD_INDEX_POS(i);
			stats->rflux = flux1->rflux + flux2->rflux;
			stats->bflux = flux1->bflux + flux2->bflux;
			stats->nflux = flux1->nflux + flux2->nflux;
		}
	}
}

/*
  void merctree_compute_inverse_perm(merctree* s) {
  // compute inverse permutation vector.
  s->inverse_perm = malloc(Ndata(s) * sizeof(int));
  if (!s->inverse_perm) {
  fprintf(stderr, "Failed to allocate merc kdtree inverse permutation vector.\n");
  return;
  }
  kdtree_inverse_permutation(s->tree, s->inverse_perm);
  }
*/

/*
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
*/

merctree* merctree_new() {
	merctree* s = merctree_alloc();
	s->header = qfits_header_default();
	if (!s->header) {
		fprintf(stderr, "Failed to create a qfits header for merc kdtree.\n");
		free(s);
		return NULL;
	}
	qfits_header_add(s->header, "AN_FILE", "MKDT", "This file is a merc kdtree.", NULL);
	return s;
}

int merctree_write_to_file(merctree* s, char* fn) {
	extra_table extras[2];
	extra_table* stats = extras;
	extra_table* fluxes = extras + 1;
	memset(extras, 0, sizeof(extras));

	stats->name = "merc_stats";
	stats->datasize = sizeof(merc_stats);
	stats->nitems = s->tree->nnodes;
	stats->ptr = s->stats;
	stats->found = 1;

	fluxes->name = "merc_flux";
	fluxes->datasize = sizeof(merc_flux);
	fluxes->nitems = s->tree->ndata;
	fluxes->ptr = s->flux;
	fluxes->found = 1;

	return kdtree_fits_write_file_extras(s->tree, fn, s->header, extras, 2);
}

