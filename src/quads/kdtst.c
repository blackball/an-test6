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

static float* getfdata(int N, int D) {
	int i;
	float* fdata;
	fdata = malloc(N*D*sizeof(float));
	for (i=0; i<(N*D); i++) {
		fdata[i] = 10.0 * rand() / (float)RAND_MAX;
	}
	return fdata;
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
	float* fdata;
	kdtree_t* t1;
	kdtree_t* t2;
	kdtree_t* t3;
	kdtree_t* t4;
	void* datacopy;
	kdtree_qres_t* res;
	double pt[D];
	float fpt[D];
	double maxd2;
	int d;
	int i;
	double* data2;
	float* fdata2;
	int Nleaf = 5;

	double lowvals[] = { 0.0, 0.0 };
	double highvals[] = { 10.0, 10.0 };

	for (d=0; d<D; d++)
		pt[d] = 10.0 * rand() / (double)RAND_MAX;

	for (d=0; d<D; d++)
		fpt[d] = 10.0 * rand() / (double)RAND_MAX;

	maxd2 = 1.0;

	for (i=1; i<argc; i++) {
		char* fn = args[i];
		kdtree_t* kd;
		printf("Reading kdtree from file %s...\n", fn);
		kd = kdtree_read_fits(fn, NULL);
		if (!kd) {
			printf("Failed to read kdtree.\n");
			exit(-1);
		}
		kdtree_fits_close(kd);
	}

	printf("Making dd tree..\n");
	ddata = getddata(N, D);

	datacopy = malloc(N * D * sizeof(double));
	memcpy(datacopy, ddata, N*D*sizeof(double));

	t1 = kdtree_build(NULL, ddata, N, D, Nleaf, KDTT_DOUBLE, TRUE);

	printf("Writing dd tree...\n");
	if (kdtree_write_fits(t1, "t1.fits", NULL)) {
		fprintf(stderr, "Failed to write kdtree.\n");
		exit(-1);
	}

	kdtree_free(t1);
	free(ddata);

	printf("Reading dd tree...\n");
	t1 = kdtree_read_fits("t1.fits", NULL);

	printf("kdtree types: external %s, internal %s, data %s.\n",
		   kdtree_kdtype_to_string(t1->external_type),
		   kdtree_kdtype_to_string(t1->internal_type),
		   kdtree_kdtype_to_string(t1->data_type));

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

	printf("Making inttree..\n");
	ddata = getddata(N, D);

	datacopy = malloc(N * D * sizeof(double));
	memcpy(datacopy, ddata, N*D*sizeof(double));

	t2 = kdtree_new(N, D, Nleaf);
	kdtree_set_limits(t2, lowvals, highvals);
	kdtree_build(t2, ddata, N, D, Nleaf, KDTT_DOUBLE_U32, FALSE);

	printf("Writing inttree...\n");
	if (kdtree_write_fits(t2, "t2.fits", NULL)) {
		fprintf(stderr, "Failed to write kdtree.\n");
		exit(-1);
	}

	kdtree_free(t2);
	free(ddata);

	printf("Reading inttree...\n");
	t2 = kdtree_read_fits("t2.fits", NULL);

	printf("Rangesearch...\n");
	res = kdtree_rangesearch_options(t2, pt, maxd2, KD_OPTIONS_SMALL_RADIUS);
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
	kdtree_fits_close(t2);


	printf("Making du32 convtree (bb + convert_data)..\n");
	ddata = getddata(N, D);
	datacopy = malloc(N * D * sizeof(double));
	memcpy(datacopy, ddata, N*D*sizeof(double));

	t3 = kdtree_new(N, D, Nleaf);
	kdtree_set_limits(t3, lowvals, highvals);
	kdtree_convert_data(t3, ddata, N, D, Nleaf, KDTT_DOUBLE_U32);
	kdtree_build(t3, t3->data.any, N, D, Nleaf, t3->treetype, TRUE);

	if (memcmp(ddata, datacopy, N*D*sizeof(double))) {
		printf("ERROR - data changed!\n");
		exit(-1);
	}
	free(datacopy);

	printf("Writing convtree...\n");
	if (kdtree_write_fits(t3, "t3.fits", NULL)) {
		fprintf(stderr, "Failed to write kdtree.\n");
		exit(-1);
	}

	kdtree_free(t3);

	printf("Reading convtree...\n");
	t3 = kdtree_read_fits("t3.fits", NULL);

	printf("Rangesearch...\n");
	res = kdtree_rangesearch_options(t3, pt, maxd2, KD_OPTIONS_SMALL_RADIUS);
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
	data2 = ddata;
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

	kdtree_fits_close(t3);
	free(ddata);

	printf("Making ff tree... (split/dim)\n");
	fdata = getfdata(N, D);
	datacopy = malloc(N * D * sizeof(float));
	memcpy(datacopy, fdata, N*D*sizeof(float));
	t4 = kdtree_build(NULL, fdata, N, D, Nleaf, KDTT_FLOAT, FALSE);

	printf("Writing fftree...\n");
	if (kdtree_write_fits(t4, "t4.fits", NULL)) {
		fprintf(stderr, "Failed to write kdtree.\n");
		exit(-1);
	}

	kdtree_free(t4);
	free(fdata);

	printf("Reading fftree...\n");
	t4= kdtree_read_fits("t4.fits", NULL);

	printf("Rangesearch...\n");
	res = kdtree_rangesearch_options(t4, fpt, maxd2, KD_OPTIONS_SMALL_RADIUS);
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
	fdata2 = datacopy;
	for (i=0; i<N; i++) {
		double d2 = 0.0;
		for (d=0; d<D; d++) {
			double delta = (fdata2[i*D + d] - fpt[d]);
			d2 += (delta * delta);
		}
		if (d2 <= maxd2)
			printf("%i ", i);
	}
	printf("]\n");

	kdtree_fits_close(t4);
	free(datacopy);

	printf("Done!\n");
	return 0;
}


