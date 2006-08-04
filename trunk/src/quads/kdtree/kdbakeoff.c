#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

#include "inttree.h"
#include "../tic.h"

#define REPS 5
double iter_total[REPS];
double rec_total[REPS];

void grnf (double *x, double *y);

int main() {
	int N = 1000000;
	//int D = 4;
	int D = 3;
	double* data, *data2;
	int i, d, r;
	kdtree_t* kd;
	intkdtree_t* ikd;
	kdtree_qres_t* res1;
	kdtree_qres_t* res2;
	int levels;
	double maxd2 = 0.005;
	//double maxd2 = 0.0001;
	//double maxd2 = 0.0015;
	int ROUNDS = 100;
	int lastgrass = 0;

	printf("N=%i, D=%i.\n", N, D);
	printf("Generating random data...\n");
	fflush(stdout);
	data = malloc(N * D * sizeof(double));
	data2 = malloc(N * D * sizeof(double));
	assert(data);
	assert(data2);



#if 0
	for (i=0; i<N*D; i++) {
		data[i] = rand() / (double)RAND_MAX;
		data2[i] = data[i];
	}
#endif
	// on the sphere
	for (i=0; i<N*D; i+=2) {
		grnf(data+i, data+i+1);
	}
	for (i=0; i<N*D; i+=3) {
		real mag = data[i]*data[i]+data[i+1]*data[i+1]+data[i+2]*data[i+2];
		data[i] /= mag;
		data[i+1] /= mag;
		data[i+2] /= mag;
		data2[i] = data[i];
		data2[i+1] = data[i+1];
		data2[i+2] = data[i+2];
	}


	for (i=0; i<REPS; i++)
		iter_total[i] = rec_total[i] = 0.0;

	levels = kdtree_compute_levels(N, 2);
	printf("Creating tree with %i levels...\n", levels);
	fflush(stdout);

	/*
	for (r=0; r<REPS; r++) {
		kdtree_t *kd1;
		intkdtree_t *kd2;
		struct timeval tv1, tv2, tv3;
		gettimeofday(&tv1, NULL);
		kd1 = kdtree_build(data, N, D, levels);
		gettimeofday(&tv2, NULL);
		kd2 = intkdtree_build(data, N, D, levels);
		gettimeofday(&tv3, NULL);

		printf("regular: %g ms.\nintkdtree: %g ms.\n",
			   millis_between(&tv1, &tv2), millis_between(&tv2, &tv3));
		kdtree_free(kd1);
		kdtree_free(kd2);
	}
	exit(0);
	*/

	tic();
	kd = kdtree_build(data, N, D, levels);
	toc();
	tic();
	ikd = intkdtree_build(data2, N, D, levels, 0, 1);
	toc();

	for (i=0; i<ROUNDS; i++) {
		double pt[4];
		struct timeval tv1, tv2, tv3;
		int grass;
		real mag;
		grass = i * 80 / ROUNDS;
		if (grass != lastgrass) {
			printf(".");
			fflush(stdout);
			lastgrass = grass;
		}

		grnf(pt, pt+1);
		grnf(pt+2, pt+3);
		mag = pt[i]*pt[i]+pt[i+1]*pt[i+1]+pt[i+2]*pt[i+2];
		pt[i] /= mag;
		pt[i+1] /= mag;
		pt[i+2] /= mag;
		//for (d=0; d<D; d++)
			//pt[d] = rand() / (double)RAND_MAX;

		for (r=0; r<REPS; r++) {
			gettimeofday(&tv1, NULL);
			res1 = kdtree_rangesearch_nosort(kd, pt, maxd2);
			gettimeofday(&tv2, NULL);
			res2 = intkdtree_rangesearch_unsorted(ikd, pt, maxd2);
			gettimeofday(&tv3, NULL);
			if (r < REPS-1) {
				kdtree_free_query(res1);
				kdtree_free_query(res2);
			}
			//printf("reg: %g ms.\nint: %g ms.\n\n",
			//	   millis_between(&tv1, &tv2), millis_between(&tv2, &tv3));

			rec_total[r] += millis_between(&tv1, &tv2);
			iter_total[r] += millis_between(&tv2, &tv3);
		}

		//assert(res1->nres == res2->nres);
		if (res1->nres != res2->nres) {
			//printf("X");
			//fflush(stdout);
			printf("\nDISCREPANCY: res1: %i, res2: %i results.\n", res1->nres, res2->nres);
			//exit(-1);
		}
		//printf("%i results.\n", res1->nres);
		kdtree_free_query(res1);
		kdtree_free_query(res2);
	}
	printf("\n");

	printf("\n\nTotals:\n\n");
	for (i=0; i<REPS; i++)
		printf("normal: %g ms.\nintegr: %g ms.\n",
			rec_total[i], iter_total[i]);
	
	return 0;
}
