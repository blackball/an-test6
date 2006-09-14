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
	double* ddata;
	float* fdata;
	kdtree_t* t1;
	kdtree_t* t2;
	kdtree_t* t3;
	kdtree_t* t4;
	int N, D;
	int levels;
	void* datacopy;

	N = 1000;
	D = 2;
	levels = 8;

	/*
	  kdtree_t* KDFUNC(kdtree_build)
	  (void *data, int N, int D, int maxlevel, int treetype, bool bb,
	  bool copydata);
	*/

	printf("Making dd tree..\n");
	ddata = getddata(N, D);
	t1 = kdtree_build(ddata, N, D, levels, KDTT_DOUBLE, TRUE, FALSE);
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


