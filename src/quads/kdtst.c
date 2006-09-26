#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "kdtree.h"
#include "kdtree_fits_io.h"

static double* getddata(int N, int D) {
	int i;
	double* ddata;
	ddata = malloc(N*D*sizeof(double));
	for (i=0; i<(N*D); i++) {
		ddata[i] = 10.0 * rand() / (double)RAND_MAX;
	}
	return ddata;
}

static int compare_ints(const void* v1, const void* v2) {
    int i1 = *(int*)v1;
    int i2 = *(int*)v2;
    if (i1 > i2) return 1;
    else if (i1 < i2) return -1;
    else return 0;
}

int main(int argc, char** args) {
	int N = 1000;
	int D = 2;

	double* ddata;
	kdtree_t* t1;
	void* datacopy;
	kdtree_qres_t* res;
	double pt[D];
	double maxd2;
	int d;
	int i;
	double* data2;
	int Nleaf = 5;
	int levels = 8;

	double lowvals[] = { 0.0, 0.0 };
	double highvals[] = { 10.0, 10.0 };

	for (d=0; d<D; d++)
		pt[d] = 10.0 * rand() / (double)RAND_MAX;

	maxd2 = 1.0;

	printf("Making dd tree..\n");
	ddata = getddata(N, D);

	datacopy = malloc(N * D * sizeof(double));
	memcpy(datacopy, ddata, N*D*sizeof(double));

	t1 = kdtree_build(ddata, N, D, levels);

	kdtree_check(t1);

	printf("Writing dd tree...\n");
	if (kdtree_fits_write_file(t1, "t1.fits", NULL)) {
		fprintf(stderr, "Failed to write kdtree.\n");
		exit(-1);
	}

	kdtree_free(t1);
	free(ddata);

	printf("Reading dd tree...\n");
	t1 = kdtree_fits_read_file("t1.fits");

	printf("Checking...\n");
	kdtree_check(t1);

	printf("Rangesearch...\n");
	res = kdtree_rangesearch_options(t1, pt, maxd2, KD_OPTIONS_SMALL_RADIUS);
	if (!res)
		printf("No results.\n");
	else {
		printf("%i results.\n", res->nres);
		// sort by index.
		qsort(res->inds, res->nres, sizeof(int), compare_ints);
		printf("Inds : [ ");
		for (i=0; i<res->nres; i++) {
			printf("%i ", res->inds[i]);
		}
		printf("]\n");
		kdtree_free_query(res);
	}
	printf("Naive: [ ");
	data2 = datacopy;
	for (i=0; i<N; i++) {
		double d2 = 0.0;
		for (d=0; d<D; d++) {
			double delta = (data2[i*D + d] - pt[d]);
			d2 += (delta * delta);
		}
		if (d2 <= maxd2)
			printf("%i ", i);
	}
	printf("]\n");

	free(datacopy);
	kdtree_fits_close(t1);

	printf("Done!\n");
	return 0;
}


