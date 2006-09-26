#include <stdlib.h>

#include "permutedsort.h"

static void* qsort_array;
static int qsort_array_stride;
static int (*qsort_compare)(const void*, const void*);

static int compare_permuted(const void* v1, const void* v2) {
	int i1 = *(int*)v1;
	int i2 = *(int*)v2;
	void* val1, *val2;
	val1 = ((char*)qsort_array) + i1 * qsort_array_stride;
	val2 = ((char*)qsort_array) + i2 * qsort_array_stride;
	return qsort_compare(val1, val2);
}

void permuted_sort_set_params(void* realarray, int array_stride,
							  int (*compare)(const void*, const void*)) {
	qsort_array = realarray;
	qsort_array_stride = array_stride;
	qsort_compare = compare;
}

void permuted_sort(int* perm, int Nperm) {
	qsort(perm, Nperm, sizeof(int), compare_permuted);
}


int compare_doubles(const void* v1, const void* v2) {
	const double d1 = *(double*)v1;
	const double d2 = *(double*)v2;
	if (d1 < d2)
		return -1;
	if (d1 > d2)
		return 1;
	return 0;
}


