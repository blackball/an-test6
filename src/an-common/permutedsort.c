/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

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

int compare_floats(const void* v1, const void* v2) {
	float f1 = *(float*)v1;
	float f2 = *(float*)v2;
	if (f1 < f2)
		return -1;
	if (f1 > f2)
		return 1;
	return 0;
}

int compare_doubles_desc(const void* v1, const void* v2) {
    return compare_doubles(v2, v1);
}

int compare_floats_desc(const void* v1, const void* v2) {
    return compare_floats(v2, v1);
}

