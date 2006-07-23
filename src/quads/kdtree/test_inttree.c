#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "inttree.h"
#include "CuTest/CuTest.h"

#define MAXDEPTH 30

/* Commenting out-- no longer sorting; we're median pivoting */
/*
void test_sort_1d_odd(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120,1};
	real data_sorted[] = {1,4,4,5,7,8,9,33,56,84,120};
	int i, n=11, d=1;
	intkdtree_t *kd = intkdtree_build(data, n, d, 2, 0, 150);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	printf("failure: ");
	for (i=0;i<n*d;i++) {
	    printf("%.2lf ", data[i]);
	}
	printf("\n");
	fflush(stdout);
	for (i=0;i<n*d;i++) {
	    CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
}
*/

/* 3 is the deepest level we can do with 10 elements */
/* This is no longer sorted, because the pivot doesn't run on child nodes. This
 * means at the low levels the data isn't sorted anymore */
void test_sort_1d_even_3(CuTest *tc)
{
	//real data[]        = {5,9,84,7,56,4,8,4,33,120};
	real data[]        = {33,9,84,7,56,4,8,4,5,120};
	real data_sorted[] = {4,4,5,7,8,33,9,56,84,120}; /* notice this isn't quite sorted */
	int lr[]           = {2,4,7,9};
	int i, n=10, d=1;
	intkdtree_t *kd = intkdtree_build(data, n, d, 3, 0, 150);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
		printf("%3.0f ",  data[i]);
	}
	printf("\n");
	for (i=0;i<n*d;i++) {
	    CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
	for (i=0;i<4;i++) {
	    CuAssertIntEquals(tc, kd->lr[i], lr[i]);
	}
}

/* 4 is too deep for 10 elements */
void test_sort_1d_even_too_many_levels(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120};
	int n=10, d=1;
	intkdtree_t *kd = intkdtree_build(data, n, d, 9, 0, 150);
	CuAssertPtrEquals(tc, kd, NULL);
}

void test_sort_2d_even(CuTest *tc)
{
	real data[]        = {5,6, 84,85, 56,57, 8,9};
	real data_sorted[] = {5,6, 84,85, 56,57, 8,9};
	int lr[]           = {3};
	int i, n=4, d=2;
	/* in this case there are no interior nodes, so no pivot takes place */
	intkdtree_t *kd = intkdtree_build(data, n, d, 1, 0, 150);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
		CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
	for (i=0;i<1;i++) {
	    CuAssertIntEquals(tc, kd->lr[i], lr[i]);
	}
}

void test_sort_2d_even_harder(CuTest *tc)
{
	real data[]        = {5,6, 84,85, 56,52, 8,2};
	real data_sorted[] = {8,2, 5,6, 56,52, 84,85};
	int i, n=4, d=2;
	intkdtree_t *kd = intkdtree_build(data, n, d, 2, 0, 150);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
		CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
}

/*
void test_1d_nn(CuTest *tc)
{
	real data[]        = {5,9,36,84,7,56,4,8,4,120};
	real qp[]          = {33};
	int n=10, d=1;
	kdtree_qres_t *kr;
	intkdtree_t *kd = intkdtree_build(data, n, d, 2);
	CuAssertIntEquals(tc, 1, kdtree_check(kd));
	CuAssertPtrNotNullMsg(tc, "null kd-tree", kd);
	kr = kdtree_rangesearch(kd, qp, 9.2);
	CuAssertPtrNotNullMsg(tc, "null query result", kr);
	CuAssertIntEquals(tc, 1, kr->nres);
	CuAssertIntEquals(tc, 36.0, kr->results[0]);
}

void test_1d_nn_2(CuTest *tc)
{
	real data[]        = {5,9,36,84,35,7,56,4,8,4,120};
	real qp[]          = {33};
	int n=10, d=1;
	kdtree_qres_t *kr;
	intkdtree_t *kd = intkdtree_build(data, n, d, 2);
	CuAssertPtrNotNullMsg(tc, "null kd-tree", kd);
	kr = kdtree_rangesearch(kd, qp, 9.2);
	CuAssertPtrNotNullMsg(tc, "null query result", kr);
	CuAssertIntEquals(tc, 2, kr->nres);
	CuAssertIntEquals(tc, 35.0, kr->results[0]);
	CuAssertIntEquals(tc, 36.0, kr->results[1]);
	CuAssertIntEquals(tc, 4, kr->inds[0]);
	CuAssertIntEquals(tc, 2, kr->inds[1]);
}

void test_2d_lots(CuTest *tc)
{
   real data[]        = {5,9,    36,84,  7,56,  4,8,   4,33, 42,1,
   22,20,  14,92,  32,22, 23,1,  89,2, 32,9,
   93,33,  85,24,  46,21, 21,76, 99,39};
   int n=17, d=2;
}
*/

void test_kd_size(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120};
	int n=10, d=1;
	intkdtree_t *kd = intkdtree_build(data, n, d, 2, 0, 150);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	CuAssertIntEquals(tc, kd->ndim, d);
	CuAssertIntEquals(tc, kd->ndata, n);
}

void test_kd_invalid_args(CuTest *tc)
{
	real data[] = {5,9,84,7,56,4,8,4,33,120};
	int n=10, d=1;
	CuAssertPtrEquals(tc, intkdtree_build(data,n,0,2, 0, 150), NULL);
	CuAssertPtrEquals(tc, intkdtree_build(data,0,d,2, 0, 150), NULL);
	CuAssertPtrEquals(tc, intkdtree_build(NULL,n,d,2, 0, 150), NULL);
	CuAssertPtrEquals(tc, intkdtree_build(NULL,n,d,2, 0, 150), NULL);
}

void test_kd_massive_build(CuTest *tc)
{
	intkdtree_t *kd;
	int n=10000, d=4, i;
	real *data = malloc(sizeof(real)*n*d);
	srandom(0);
	for (i=0; i < n*d; i++) {
		data[i] = random() / (real)RAND_MAX;
		assert(data[i] >= 0.0);
		assert(data[i] <= 1.0);
	}
	kd = intkdtree_build(data, n, d, 10, 0, 1);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
}

/*
void test_kd_nn(CuTest *tc) {
	int n=1000;
	int levels=8;
	int d=3, i, j;
	intkdtree_t* kd;
	real* data;
	real* point;
	int ntimes = 100;
	int t;

	data = malloc(sizeof(real)*n*d);
	for (i=0; i < n*d; i++) 
        data[i] = random() / (real)RAND_MAX;
	kd = intkdtree_build(data, n, d, levels);
	point = malloc(sizeof(real)*d);
	CuAssertPtrNotNull(tc, kd);

	for (t=0; t<ntimes; t++) {
		int ibest;
		real dbest2;
		int icheck;
		real dcheck2;
		for (i=0; i<d; i++)
			point[i] = random() / (real)RAND_MAX;

		ibest = kdtree_nearest_neighbour(kd, point, &dbest2);

		// check it...
		dcheck2 = 1e300;
		icheck = -1;
		for (i=0; i<n; i++) {
			real d2 = 0.0;
			for (j=0; j<d; j++) {
				real diff = (data[i*d + j] - point[j]);
				d2 += (diff*diff);
			}
			if (d2 < dcheck2) {
				dcheck2 = d2;
				icheck = i;
			}
		}
		if ((ibest != icheck) || (dbest2 != dcheck2)) {
			printf("nn returned %i / %g, check returned %i / %g.\n",
				   ibest, dbest2, icheck, dcheck2);
		}
		// make sure the reported best index is right.
		CuAssertIntEquals(tc, ibest, icheck);
		// make sure the reported distance is right.
		CuAssertDblEquals(tc, dcheck2, dbest2, 1e-10);
	}
	free(point);
	kdtree_free(kd);
	free(data);
}

void rangesearch_callback(intkdtree_t* kd, real* pt, real maxdist2,
						  real* computed_dist2, int indx, void* extra) {
	int* found = (int*)extra;
	//found[indx]++;
	found[kd->perm[indx]]++;
}

void test_kd_range_search_callback(CuTest *tc) {
	int n=10000;
	int d=3, i, j;
	int levels=10;
	real range;
	real range2;
	real *point;
	real *data = malloc(sizeof(real)*n*d);
	real *origdata = malloc(sizeof(real)*n*d);
	int* points_found = malloc(n * sizeof(int));
	int nfound, ntrue;
	int ntimes = 10;
	int t;
	intkdtree_t *kd;

	for (i=0; i < n*d; i++) 
        data[i] = random() / (real)RAND_MAX;
	memcpy(origdata, data, n*d*sizeof(real));
	kd = intkdtree_build(data, n, d, levels);
	point = malloc(sizeof(real)*d);
	for (t=0; t<ntimes; t++) {
		range = t * 0.02;
		range2 = range*range;
		for (i=0; i<d; i++)
			point[i] = random() / (real)RAND_MAX;
		memset(points_found, 0, n * sizeof(int));

		kdtree_rangesearch_callback(kd, point, range2, rangesearch_callback,
									points_found);

		nfound = 0;
		for (i=0; i<n; i++) {
			CuAssertTrue(tc, (points_found[i] == 0) || (points_found[i] == 1));
			nfound += points_found[i];
		}

		ntrue = 0;
		for (i=0; i<n; i++) {
			double d2 = 0.0;
			for (j=0; j<d; j++) {
				double diff = (origdata[i*d + j] - point[j]);
				d2 += (diff*diff);
			}
			if (d2 > range2) continue;
			ntrue++;
			// make sure this point was found.
			CuAssertTrue(tc, (points_found[i] == 1));
		}
		printf("range search: got %i results.\n", nfound);
		// make sure the number of hits is equal.
		CuAssertIntEquals(tc, nfound, ntrue);
	}
	free(origdata);
	free(point);
	free(points_found);
	kdtree_free(kd);
	free(data);
}

*/
void test_kd_range_search(CuTest *tc) {
	int n=10000;
	int d=3, i, j;
	int levels=10;
	real range = 0.08;
	real range2;
	real *point;
	real *data = malloc(sizeof(real)*n*d);
	real *origdata = malloc(sizeof(real)*n*d);
	kdtree_qres_t* results;
	int nfound;
	int ntimes = 10;
	int t;
	intkdtree_t *kd;

	for (i=0; i < n*d; i++) 
		data[i] = random() / (real)RAND_MAX;

	memcpy(origdata, data, n*d*sizeof(real));

	kd = intkdtree_build(data, n, d, levels,0,1);

	point = malloc(sizeof(real)*d);

	for (t=0; t<ntimes; t++) {

		range = t * 0.02;
		range2 = range*range;

		for (i=0; i<d; i++)
			point[i] = random() / (real)RAND_MAX;
		results = intkdtree_rangesearch(kd, point, range2);

		CuAssertPtrNotNullMsg(tc, "null kdtree rangesearch result.", results);

		nfound = 0;
		for (i=0; i<n; i++) {
			double d2;
			int ok;
			int hitind;
			d2 = 0.0;
			for (j=0; j<d; j++) {
				double diff = (origdata[i*d + j] - point[j]);
				d2 += (diff*diff);
			}
			if (d2 > range2) continue;
			nfound++;
			// make sure this hit is present in the results list.
			hitind = -1;
			ok = 0;
			for (j=0; j<results->nres; j++) {
				if (results->inds[j] == i) {
					ok = 1;
					hitind = j;
					break;
				}
			}
			CuAssertIntEquals(tc, ok, 1);
			if (ok) {
				// make sure the reported distance is right.
				CuAssertDblEquals(tc, results->sdists[hitind], d2, 1e-10);
				// make sure the reported results are right.
				for (j=0; j<d; j++) {
					CuAssertDblEquals(tc, results->results[hitind*d + j], origdata[i*d + j], 1e-30);
				}
			}
		}
		// make sure the number of hits is equal.
		CuAssertIntEquals(tc, nfound, results->nres);

		printf("range search: %i results.\n", results->nres);

		kdtree_free_query(results);
	}
	free(origdata);
	free(point);
	intkdtree_free(kd);
	free(data);
}


int main(void) {
	/* Run all tests */
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();


	/* Add new tests here */
	//SUITE_ADD_TEST(suite, test_sort_1d_even);
	//SUITE_ADD_TEST(suite, test_sort_1d_odd);
	SUITE_ADD_TEST(suite, test_sort_1d_even_3);
	SUITE_ADD_TEST(suite, test_sort_1d_even_too_many_levels);
	SUITE_ADD_TEST(suite, test_sort_2d_even);
	SUITE_ADD_TEST(suite, test_sort_2d_even_harder);
	//SUITE_ADD_TEST(suite, test_1d_nn);
	//SUITE_ADD_TEST(suite, test_1d_nn_2);
	SUITE_ADD_TEST(suite, test_kd_size);
	SUITE_ADD_TEST(suite, test_kd_invalid_args);
	//SUITE_ADD_TEST(suite, test_kd_nn);
	SUITE_ADD_TEST(suite, test_kd_range_search);
	//SUITE_ADD_TEST(suite, test_kd_range_search_callback);
	//SUITE_ADD_TEST(suite, test_sort_random);
	//
	SUITE_ADD_TEST(suite, test_kd_massive_build);

	/* Run the suite, collect results and display */
	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
	return 0;
}
