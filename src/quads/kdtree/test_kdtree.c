#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "kdtree.h"
#include "CuTest/CuTest.h"

#define MAXDEPTH 30

void test_sort_1d_even(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120};
	real data_sorted[] = {4,4,5,7,8,9,33,56,84,120};
	int i, n=10, d=1;
	kdtree_t *kd = kdtree_build(data, n, d, 2);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
	    CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
}

void test_sort_1d_odd(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120,1};
	real data_sorted[] = {1,4,4,5,7,8,9,33,56,84,120};
	int i, n=11, d=1;
	kdtree_t *kd = kdtree_build(data, n, d, 2);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
	    CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
}

/* 3 is the deepest level we can do with 10 elements */
void test_sort_1d_even_3(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120};
	real data_sorted[] = {4,4,5,7,8,9,33,56,84,120};
	int i, n=10, d=1;
	kdtree_t *kd = kdtree_build(data, n, d, 3);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
	    CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
    //kdtree_output_dot(stdout, kd);
}

/* 4 is too deep for 10 elements */
void test_sort_1d_even_too_many_levels(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120};
	int n=10, d=1;
	kdtree_t *kd = kdtree_build(data, n, d, 9);
	CuAssertPtrEquals(tc, kd, NULL);
}

void test_sort_2d_even(CuTest *tc)
{
	real data[]        = {5,6, 84,85, 56,57, 8,9};
	real data_sorted[] = {5,6, 8,9, 56,57, 84,85};
	int i, n=4, d=2;
	kdtree_t *kd = kdtree_build(data, n, d, 1);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
		CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
}

void test_sort_2d_even_harder(CuTest *tc)
{
	real data[]        = {5,6, 84,85, 56,52, 8,2};
	real data_sorted[] = {8,2, 5,6, 56,52, 84,85};
	int i, n=4, d=2;
	kdtree_t *kd = kdtree_build(data, n, d, 2);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	for (i=0;i<n*d;i++) {
		CuAssertIntEquals(tc, data[i], data_sorted[i]);
	}
}

void test_1d_nn(CuTest *tc)
{
	real data[]        = {5,9,36,84,7,56,4,8,4,120};
	real qp[]          = {33};
	int n=10, d=1;
	kdtree_t *kd = kdtree_build(data, n, d, 2);
	CuAssertPtrNotNullMsg(tc, "null kd-tree", kd);
	kdtree_qres_t *kr = kdtree_rangesearch(kd, qp, 9.2);
	CuAssertPtrNotNullMsg(tc, "null query result", kr);
	CuAssertIntEquals(tc, 1, kr->nres);
	CuAssertIntEquals(tc, 36.0, kr->results[0]);
}

void test_1d_nn_2(CuTest *tc)
{
	real data[]        = {5,9,36,84,35,7,56,4,8,4,120};
	real qp[]          = {33};
	int n=10, d=1;
	kdtree_t *kd = kdtree_build(data, n, d, 2);
	CuAssertPtrNotNullMsg(tc, "null kd-tree", kd);
	kdtree_qres_t *kr = kdtree_rangesearch(kd, qp, 9.2);
	CuAssertPtrNotNullMsg(tc, "null query result", kr);
	CuAssertIntEquals(tc, 2, kr->nres);
	CuAssertIntEquals(tc, 35.0, kr->results[0]);
	CuAssertIntEquals(tc, 36.0, kr->results[1]);
	CuAssertIntEquals(tc, 4, kr->inds[0]);
	CuAssertIntEquals(tc, 2, kr->inds[1]);
}

void test_2d_lots(CuTest *tc)
{/*
	real data[]        = {5,9,    36,84,  7,56,  4,8,   4,33, 42,1,
	                      22,20,  14,92,  32,22, 23,1,  89,2, 32,9,
	                      93,33,  85,24,  46,21, 21,76, 99,39};
	 int n=17, d=2;*/
}

void test_kd_size(CuTest *tc)
{
	real data[]        = {5,9,84,7,56,4,8,4,33,120};
	int n=10, d=1;
	kdtree_t *kd = kdtree_build(data, n, d, 2);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
	CuAssertIntEquals(tc, kd->ndim, d);
	CuAssertIntEquals(tc, kd->ndata, n);
}

void test_kd_invalid_args(CuTest *tc)
{
	real data[] = {5,9,84,7,56,4,8,4,33,120};
	int n=10, d=1;
	CuAssertPtrEquals(tc, kdtree_build(data,n,0,2), NULL);
	CuAssertPtrEquals(tc, kdtree_build(data,0,d,2), NULL);
	CuAssertPtrEquals(tc, kdtree_build(NULL,n,d,2), NULL);
	CuAssertPtrEquals(tc, kdtree_build(NULL,n,d,2), NULL);
}

void test_kd_massive_build(CuTest *tc)
{
	srandom(0);
	int n=100000, d=4, i;
	real *data = malloc(sizeof(real)*n*d);
	for (i=0; i < n*d; i++) 
        data[i] = random() / (real)RAND_MAX;
	kdtree_t *kd = kdtree_build(data, n, d, 16);
	CuAssertPtrNotNullMsg(tc, "null kd-tree return", kd);
}

int main(void) {
	/* Run all tests */
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();


	/* Add new tests here */
	SUITE_ADD_TEST(suite, test_sort_1d_even);
	SUITE_ADD_TEST(suite, test_sort_1d_odd);
	SUITE_ADD_TEST(suite, test_sort_1d_even_3);
	SUITE_ADD_TEST(suite, test_sort_1d_even_too_many_levels);
	SUITE_ADD_TEST(suite, test_sort_2d_even);
	SUITE_ADD_TEST(suite, test_sort_2d_even_harder);
	SUITE_ADD_TEST(suite, test_1d_nn);
	SUITE_ADD_TEST(suite, test_1d_nn_2);
	SUITE_ADD_TEST(suite, test_kd_size);
	SUITE_ADD_TEST(suite, test_kd_invalid_args);
	//SUITE_ADD_TEST(suite, test_kd_massive_build);

	/* Run the suite, collect results and display */
	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
	return 0;
}
