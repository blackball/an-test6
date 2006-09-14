#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

#include "kdtree.h"

int main() {
	double* ddata;
	float* fdata;
	kdtree_t* t1;
	kdtree_t* t2;
	kdtree_t* t3;
	kdtree_t* ti;
	int N, D;
	int i;
	int levels;

	N = 1000;
	D = 2;
	levels = 8;

	ddata = malloc(N*D*sizeof(double));
	fdata = malloc(N*D*sizeof(float));

	for (i=0; i<(N*D); i++) {
		ddata[i] = rand() / (double)RAND_MAX;
		fdata[i] = rand() / (float)RAND_MAX;
	}

	printf("Making dd tree..\n");
	t1 = kdtree_build(ddata, N, D, levels, KDTT_DOUBLE, TRUE, FALSE);
	printf("Making inttree..\n");
	ti = kdtree_build(ddata, N, D, levels, KDTT_DOUBLE_U32, FALSE, FALSE);
	printf("Making du32 tree..\n");
	t2 = kdtree_build(ddata, N, D, levels, KDTT_DOUBLE_U32, TRUE, TRUE);
	printf("Making ff tree..\n");
	t3 = kdtree_build(fdata, N, D, levels, KDTT_FLOAT, TRUE, TRUE);
	printf("Done!\n");

	return 0;
}


