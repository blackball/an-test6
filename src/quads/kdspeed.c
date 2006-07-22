#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

#include "kdtree.h"
#include "tic.h"

int main() {
	int N = 3000000;
	int D = 4;
	double* data;
	int i, d, r;
	kdtree_t* kd;
	kdtree_qres_t* res1;
	kdtree_qres_t* res2;
	int levels;
	double maxd2 = 0.003;
	int ROUNDS = 10000;
	int REPS = 5;
	double iter_total[REPS];
	double rec_total[REPS];
	int lastgrass = 0;

	printf("N=%i, D=%i.\n", N, D);
	printf("Generating random data...\n");
	fflush(stdout);
	data = malloc(N * D * sizeof(double));
	for (i=0; i<N*D; i++)
		data[i] = rand() / (double)RAND_MAX;

	for (i=0; i<REPS; i++)
		iter_total[i] = rec_total[i] = 0.0;

	levels = kdtree_compute_levels(N, 10);
	printf("Creating tree with %i levels...\n", levels);
	fflush(stdout);
	kd = kdtree_build(data, N, D, levels);

	for (i=0; i<ROUNDS; i++) {
		double pt[3];
		struct timeval tv1, tv2, tv3;
		int grass;
		grass = i * 80 / ROUNDS;
		if (grass != lastgrass) {
			printf(".");
			fflush(stdout);
			lastgrass = grass;
		}
		for (d=0; d<D; d++)
			pt[d] = rand() / (double)RAND_MAX;

		for (r=0; r<REPS; r++) {
			gettimeofday(&tv1, NULL);
			res1 = kdtree_rangesearch_nosort(kd, pt, maxd2);
			gettimeofday(&tv2, NULL);
			res2 = kdtree_rangesearch_iter(kd, pt, maxd2);
			gettimeofday(&tv3, NULL);
			if (r < REPS-1) {
				kdtree_free_query(res1);
				kdtree_free_query(res2);
			}
			/*
				printf("recursive: %g ms.\niterative: %g ms.\n",
				   millis_between(&tv1, &tv2), millis_between(&tv2, &tv3));
			*/
			rec_total[r] += millis_between(&tv1, &tv2);
			iter_total[r] += millis_between(&tv2, &tv3);
		}

		assert(res1->nres == res2->nres);
		if (res1->nres != res2->nres) {
			printf("DISCREPANCY: res1: %i, res2: %i results.\n", res1->nres, res2->nres);
			exit(-1);
		}
		//printf("%i results.\n", res1->nres);
		kdtree_free_query(res1);
		kdtree_free_query(res2);
	}
	printf("\n");

	printf("\n\nTotals:\n\n");
	for (i=0; i<REPS; i++)
		printf("recursive: %g ms.\niterative: %g ms.\n",
			rec_total[i], iter_total[i]);
	
	return 0;
}
