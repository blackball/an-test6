#include <assert.h>

#include "donuts.h"
#include "dualtree_rangesearch.h"
#include "bl.h"

static void donut_pair_found(void* extra, int x, int y, double dist2) {
	int i;
	int xlistind = -1, ylistind = -1;
	pl* lists = extra;
	if (x >= y)
		return;
	for (i=0; i<pl_size(lists); i++) {
		il* list = pl_get(lists, i);
		if (il_find_index_ascending(list, x) != -1)
			xlistind = i;
		if (il_find_index_ascending(list, y) != -1)
			ylistind = i;
	}
	if ((xlistind == -1) && (ylistind == -1)) {
		// add a new list.
		il* newlist = il_new(16);
		il_insert_unique_ascending(newlist, x);
		il_insert_unique_ascending(newlist, y);
		pl_append(lists, newlist);
	} else if (xlistind == -1) {
		// add x to y's list.
		il* list = pl_get(lists, ylistind);
		il_insert_unique_ascending(list, x);
	} else if (ylistind == -1) {
		// add y to x's list.
		il* list = pl_get(lists, xlistind);
		il_insert_unique_ascending(list, y);
	} else {
		if (xlistind == ylistind)
			// they're already in the same list.
			return;
		// merge the lists.
		il* xlist = pl_get(lists, xlistind);
		il* ylist = pl_get(lists, ylistind);
		il* merged = il_merge_ascending(xlist, ylist);
		il_free(xlist);
		il_free(ylist);
		pl_set(lists, xlistind, merged);
		pl_remove(lists, ylistind);
	}
}

void detect_donuts(int fieldnum, xy** pfield,
				   double nearbydist, double thresh) {
	double* fieldxy;
	int i, N;
	int nearby;
	kdtree_t* tree;
	int levels;
	int* counts;
	double frac;
	xy* field = *pfield;
	pl* lists;
	xy* newfield;
	bool* merged;
	int fieldind;
	int nmerged;

	N = xy_size(field);
	assert(N);
	fieldxy = malloc(N * 2 * sizeof(double));
	assert(fieldxy);
	for (i=0; i<N; i++) {
		fieldxy[i*2 + 0] = xy_refx(field, i);
		fieldxy[i*2 + 1] = xy_refy(field, i);
	}

	levels = kdtree_compute_levels(N, 5);
	//printf("Building tree with %i points, %i levels.\n", N, levels);
	tree = kdtree_build(fieldxy, N, 2, levels);
	assert(tree);
	counts = calloc(N, sizeof(int));
	dualtree_rangecount(tree, tree, RANGESEARCH_NO_LIMIT, nearbydist, counts);
	nearby = 0;
	for (i=0; i<N; i++)
		nearby += counts[i];
	frac = (nearby - N) / (double)N;
	fprintf(stderr, "Field %i: Donuts: %4.1f%% (%i of %i) in range.\n",
			fieldnum, 100.0 * frac, nearby - N, N);
	free(counts);
	if (frac < thresh)
		goto done;

	lists = pl_new(32);
	dualtree_rangesearch(tree, tree, RANGESEARCH_NO_LIMIT, nearbydist,
						 donut_pair_found, lists, NULL, NULL);
	fprintf(stderr, "Found %i clusters:\n", pl_size(lists));

	nmerged = 0;
	for (i=0; i<pl_size(lists); i++)
		nmerged += il_size(pl_get(lists, i));

	newfield = mk_xy(N - nmerged + pl_size(lists));

	merged = calloc(N, sizeof(bool));

	for (i=0; i<pl_size(lists); i++) {
		int j;
		double avgx, avgy;
		il* list = pl_get(lists, i);
		fprintf(stderr, "    ");
		avgx = avgy = 0.0;
		for (j=0; j<il_size(list); j++) {
			int ind;
			fprintf(stderr, "%i ", il_get(list, j));
			ind = tree->perm[il_get(list, j)];
			merged[ind] = TRUE;
			avgx += xy_refx(field, ind);
			avgy += xy_refy(field, ind);
		}
		avgx /= (double)il_size(list);
		avgy /= (double)il_size(list);
		fprintf(stderr, "\n");
		il_free(list);
		xy_setx(newfield, i, avgx);
		xy_sety(newfield, i, avgy);
	}
	fieldind = pl_size(lists);
	for (i=0; i<N; i++) {
		if (merged[i])
			continue;
		// this star wasn't part of a donut...
		xy_setx(newfield, fieldind, xy_refx(field, i));
		xy_sety(newfield, fieldind, xy_refy(field, i));
		fieldind++;
	}

	free(merged);
	pl_free(lists);
	free_xy(field);
	*pfield = newfield;

 done:
	kdtree_free(tree);
	free(fieldxy);
}
