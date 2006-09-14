#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include "kdtree.h"

static double* getddata(int N, int D) {
	int i;
	double* ddata;
	ddata = malloc(N*D*sizeof(double));
	for (i=0; i<(N*D); i++) {
		ddata[i] = 10.0 * rand() / (double)RAND_MAX;
	}
	return ddata;
}

static float* getfdata(int N, int D) {
	int i;
	float* fdata;
	fdata = malloc(N*D*sizeof(float));
	for (i=0; i<(N*D); i++) {
		fdata[i] = 10.0 * rand() / (float)RAND_MAX;
	}
	return fdata;
}

int main() {
	int N = 1000;
	int D = 2;

	double* ddata;
	float* fdata;
	kdtree_t* t1;
	kdtree_t* t2;
	kdtree_t* t3;
	kdtree_t* t4;
	int levels;
	void* datacopy;
	kdtree_qres_t* res;
	double pt[D];
	double maxd2;
	int d;
	//int i;

	levels = 8;

	/*
	  kdtree_t* KDFUNC(kdtree_build)
	  (void *data, int N, int D, int maxlevel, int treetype, bool bb,
	  bool copydata);
	*/

	printf("Making dd tree..\n");
	ddata = getddata(N, D);
	for (d=0; d<D; d++)
		pt[d] = 10.0 * rand() / (double)RAND_MAX;
	maxd2 = 1.0;

	t1 = kdtree_build(ddata, N, D, levels, KDTT_DOUBLE, TRUE, FALSE);

	/*
	  printf("nnodes %i\n", t1->nnodes);
	  printf("nlevels %i\n", t1->nlevels);
	  for (i=0; i<t1->nnodes; i++) {
	  printf("node %i: L %i, R %i\n", i, kdtree_left(t1, i), kdtree_right(t1, i));
	  }
	*/

	printf("Rangesearch...\n");
	res = kdtree_rangesearch_options(t1, pt, maxd2, KD_OPTIONS_SMALL_RADIUS);
	if (!res)
		printf("No results.\n");
	else {
		printf("%i results.\n", res->nres);
		kdtree_free_query(res);
	}

	free(ddata);

	printf("Making inttree..\n");
	ddata = getddata(N, D);
	t2 = kdtree_build(ddata, N, D, levels, KDTT_DOUBLE_U32, FALSE, FALSE);
	free(ddata);

	printf("Making du32 tree..\n");
	ddata = getddata(N, D);
	datacopy = malloc(N * D * sizeof(double));
	memcpy(datacopy, ddata, N*D*sizeof(double));
	t3 = kdtree_build(ddata, N, D, levels, KDTT_DOUBLE_U32, TRUE, TRUE);
	if (memcmp(ddata, datacopy, N*D*sizeof(double))) {
		printf("ERROR - data changed!\n");
		exit(-1);
	}
	free(ddata);
	free(datacopy);

	printf("Making ff tree..\n");
	fdata = getfdata(N, D);
	datacopy = malloc(N * D * sizeof(float));
	memcpy(datacopy, fdata, N*D*sizeof(float));
	t4 = kdtree_build(fdata, N, D, levels, KDTT_FLOAT, TRUE, TRUE);
	if (memcmp(fdata, datacopy, N*D*sizeof(float))) {
		printf("ERROR - data changed!\n");
		exit(-1);
	}
	free(fdata);
	free(datacopy);
	printf("Done!\n");

	kdtree_free(t1);
	kdtree_free(t2);
	kdtree_free(t3);
	kdtree_free(t4);

	return 0;
}


