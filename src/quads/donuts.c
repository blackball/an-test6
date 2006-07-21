#include <assert.h>
#include <string.h>

#include "donuts.h"
#include "dualtree_rangesearch.h"
#include "bl.h"
#include "starutil.h"

static void donut_pair_found(void* extra, int x, int y, double dist2) {
	int i;
	il* xlist = NULL;
	il* ylist = NULL;
	int xlistind=-1, ylistind=-1;
	pl* lists = extra;
	if (x >= y)
		return;
	for (i=0; i<pl_size(lists); i++) {
		il* list = pl_get(lists, i);
		if (il_find_index_ascending(list, x) != -1) {
			xlistind = i;
			xlist = list;
		}
		if (il_find_index_ascending(list, y) != -1) {
			ylistind = i;
			ylist = list;
		}
	}
	if (!xlist && !ylist) {
		// add a new list.
		il* newlist = il_new(16);
		il_insert_unique_ascending(newlist, x);
		il_insert_unique_ascending(newlist, y);
		pl_append(lists, newlist);
	} else if (!xlist) {
		// add x to y's list.
		il_insert_unique_ascending(ylist, x);
	} else if (!ylist) {
		// add y to x's list.
		il_insert_unique_ascending(xlist, y);
	} else {
		if (xlist == ylist)
			// they're already in the same list.
			return;
		// merge the lists.
		il* merged = il_merge_ascending(xlist, ylist);
		il_free(xlist);
		il_free(ylist);
		pl_set(lists, xlistind, merged);
		pl_remove(lists, ylistind);
	}
}

struct merged_obj {
	double avgx;
	double avgy;
	int brightness;
};
typedef struct merged_obj merged_obj;

static int compare_brightness(const void* v1, const void* v2) {
	const merged_obj* m1 = v1;
	const merged_obj* m2 = v2;
	if (m1->brightness < m2->brightness) return -1;
	if (m1->brightness > m2->brightness) return 1;
	return 0;
}

void detect_donuts(int fieldnum, double** pfield, int* pnfield,
				   double nearbydist, double thresh) {
	double* fieldxy;
	int i, N;
	int nearby;
	kdtree_t* tree;
	int levels;
	int* counts;
	double frac;
	pl* lists;
	double* newfield;
	bool* merged;
	int fieldind;
	int nmerged;
	merged_obj* mergedobjs;
	int Nnew;
	int Ndonuts;
	int nextmerged;

	N = *pnfield;
	fieldxy = malloc(N * 2 * sizeof(double));
	memcpy(fieldxy, *pfield, N * 2 * sizeof(double));

	// Hey, doughbrain: be careful, "fieldxy" will be scrambled after 
	// creating a kdtree out of it.

	levels = kdtree_compute_levels(N, 5);
	tree = kdtree_build(fieldxy, N, 2, levels);
	assert(tree);
	counts = calloc(N, sizeof(int));
	dualtree_rangecount(tree, tree, RANGESEARCH_NO_LIMIT, nearbydist, counts);
	nearby = 0;
	for (i=0; i<N; i++)
		nearby += counts[i];
	frac = (nearby - N) / (double)N;
	fprintf(stderr, "Field %i: Donuts: %4.1f (%i of %i) in range.\n",
			fieldnum, frac, nearby - N, N);
	free(counts);
	if (frac < thresh)
		goto done;

	lists = pl_new(32);
	dualtree_rangesearch(tree, tree, RANGESEARCH_NO_LIMIT, nearbydist,
						 donut_pair_found, lists, NULL, NULL);
	fprintf(stderr, "Found %i clusters:\n", pl_size(lists));

	// how many stars joined donuts?
	nmerged = 0;
	for (i=0; i<pl_size(lists); i++)
		nmerged += il_size(pl_get(lists, i));

	Ndonuts = pl_size(lists);
	Nnew = (N - nmerged + Ndonuts);
	mergedobjs = malloc(Nnew * sizeof(merged_obj));

	merged = calloc(N, sizeof(bool));

	for (i=0; i<pl_size(lists); i++) {
		int j;
		double avgx, avgy;
		int brightest = -1;
		il* list = pl_get(lists, i);
		fprintf(stderr, "    ");
		avgx = avgy = 0.0;
		for (j=0; j<il_size(list); j++) {
			int ind;
			//fprintf(stderr, "%i ", il_get(list, j));
			ind = il_get(list, j);
			merged[ind] = TRUE;
			avgx += fieldxy[ind*2];
			avgy += fieldxy[ind*2+1];
			if ((j == 0) || (ind < brightest))
				brightest = ind;
		}
		avgx /= (double)il_size(list);
		avgy /= (double)il_size(list);
		il_free(list);
		mergedobjs[i].avgx = avgx;
		mergedobjs[i].avgy = avgy;
		mergedobjs[i].brightness = brightest;
	}

	qsort(mergedobjs, sizeof(merged_obj), Ndonuts, compare_brightness);
	nextmerged = 0;

	newfield = malloc(Nnew * 2 * sizeof(double));

	fieldind = 0;
	for (i=0; i<N; i++) {
		if (merged[i]) {
			// check if we should insert the next-brightest donut...
			if ((nextmerged < Ndonuts) &&
				(mergedobjs[nextmerged].brightness == i)) {
				newfield[fieldind*2  ] = mergedobjs[nextmerged].avgx;
				newfield[fieldind*2+1] = mergedobjs[nextmerged].avgx;
				nextmerged++;
				fieldind++;
			}
			continue;
		}
		// this star wasn't part of a donut...
		newfield[fieldind*2  ] = fieldxy[i*2];
		newfield[fieldind*2+1] = fieldxy[i*2+1];
		fieldind++;
	}
	free(mergedobjs);

	free(merged);
	pl_free(lists);
	*pfield = newfield;
	*pnfield = fieldind;

 done:
	kdtree_free(tree);
	free(fieldxy);
}
