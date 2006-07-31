#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

#include "kdtree.h"
#define KD_DIM 4
#include "kdtree.h"

#include "tic.h"

int main() {
	int N = 1000000;
	int D = 4;
	double* data;
	int i, d, r;
	kdtree_t* kd;
	kdtree_qres_t* res1;
	kdtree_qres_t* res2;
	int levels;
	double maxd2 = 0.001; // gives about 5 results per query.
	//double maxd2 = 0.002;
	//double maxd2 = 0.003;
	//double maxd2 = 0.01;
	int ROUNDS = 10000;
	int REPS = 5;
	double v1_total[REPS];
	double v2_total[REPS];
	int lastgrass = 0;
	double nresavg;

	printf("N=%i, D=%i.\n", N, D);
	printf("Generating random data...\n");
	fflush(stdout);
	data = malloc(N * D * sizeof(double));
	for (i=0; i<N*D; i++)
		data[i] = rand() / (double)RAND_MAX;

	for (i=0; i<REPS; i++)
		v1_total[i] = v2_total[i] = 0.0;

	levels = kdtree_compute_levels(N, 10);
	printf("Creating tree with %i levels...\n", levels);
	fflush(stdout);

#if 0
	for (r=0; r<0; r++) {
		kdtree_t *kd1, *kd2;
		struct timeval tv1, tv2, tv3;
		gettimeofday(&tv1, NULL);
		kd1 = kdtree_build(data, N, D, levels);
		gettimeofday(&tv2, NULL);
		kd2 = kdtree_build_4(data, N, D, levels);
		gettimeofday(&tv3, NULL);

		printf("build: %g ms.\nbuild_4: %g ms.\n",
			   millis_between(&tv1, &tv2), millis_between(&tv2, &tv3));
		kdtree_free(kd1);
		kdtree_free(kd2);
	}
#endif

	kd = kdtree_build(data, N, D, levels);

	for (i=0; i<REPS; i++)
		v1_total[i] = v2_total[i] = 0.0;

	REPS=1;
	r=0;

	nresavg = 0.0;

	for (i=0; i<ROUNDS; i++) {
		double pt[4];
		struct timeval tv1, tv2, tv3, tv4;
		int grass;
		grass = i * 80 / ROUNDS;
		if (grass != lastgrass) {
			printf(".");
			fflush(stdout);
			lastgrass = grass;
		}
		for (d=0; d<D; d++)
			pt[d] = rand() / (double)RAND_MAX;

		//#define OPTION1 			res1 = kdtree_rangesearch_options_4(kd, pt, maxd2, 0);
#define OPTION1 			res1 = kdtree_rangesearch(kd, pt, maxd2);
#define OPTION2 			res2 = kdtree_rangesearch_options_4(kd, pt, maxd2, KD_OPTIONS_SMALL_RADIUS);

		//for (r=0; r<REPS; r++) {
		if (i % 2) {
			gettimeofday(&tv1, NULL);
			OPTION1
			gettimeofday(&tv2, NULL);
			gettimeofday(&tv3, NULL);
			OPTION2
			gettimeofday(&tv4, NULL);
		} else {
			gettimeofday(&tv3, NULL);
			OPTION2
			gettimeofday(&tv4, NULL);
			gettimeofday(&tv1, NULL);
			OPTION1
			gettimeofday(&tv2, NULL);
		}

		nresavg += res1->nres;

		/*
		  if (r < REPS-1) {
		  kdtree_free_query(res1);
		  kdtree_free_query(res2);
		  }
		  //printf("rangesearch: %g ms.\nrangesearch_4: %g ms.\n", millis_between(&tv1, &tv2), millis_between(&tv2, &tv3));
		  */

		v1_total[r] += millis_between(&tv1, &tv2);
		v2_total[r] += millis_between(&tv3, &tv4);
		//}

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
		printf("rangesearch:   %g ms.\nrangesearch_4: %g ms.\n",
			v1_total[i], v2_total[i]);
	
	printf("Average number of results: %g\n", nresavg / (double)ROUNDS);

	return 0;
}
