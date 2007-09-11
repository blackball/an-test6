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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cutest.h"

#include "bl.h"

void test_il_new(CuTest* tc) {
	il* x = NULL;
	x = il_new(10);
	CuAssert(tc, "new", x != NULL);
	CuAssertIntEquals(tc, il_size(x), 0);
	CuAssertPtrEquals(tc, x->head, NULL);
	CuAssertPtrEquals(tc, x->tail, NULL);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	CuAssertIntEquals(tc, il_check_sorted_ascending(x, 0), 0);
	il_free(x);
}

void test_il_size(CuTest* tc) {
	il* x = il_new(10);
	int i, N = 100;
	for (i=0; i<N; i++) {
		il_push(x, i);
		CuAssertIntEquals(tc, i+1, il_size(x));
	}
	il_free(x);
}

void test_il_get_push(CuTest* tc) {
	il* x = il_new(10);
	il_push(x,10);
	CuAssertIntEquals(tc, 10, il_get(x, 0));
	il_push(x,20);
	CuAssertIntEquals(tc, 10, il_get(x, 0));
	CuAssertIntEquals(tc, 20, il_get(x, 1));
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	il_free(x);
}

void test_il_push_pop(CuTest* tc) {
	il* x = il_new(10);
	il_push(x,10);
	CuAssertIntEquals(tc, 10, il_get(x, 0));
	il_push(x,20);
	CuAssertIntEquals(tc, 10, il_get(x, 0));
	CuAssertIntEquals(tc, 20, il_get(x, 1));
	CuAssertIntEquals(tc, 20, il_pop(x));
	CuAssertIntEquals(tc, 10, il_pop(x));
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	il_free(x);
}

void test_il_push_pop2(CuTest* tc) {
	int i, N=100;
	il* x = il_new(10);
	for (i=0; i<N; i++)
		il_push(x,i);
	for (i=0; i<N; i++)
		CuAssertIntEquals(tc, N-i-1, il_pop(x));
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	il_free(x);
}


void test_il(CuTest* tc) {
	il* x = il_new(10);
	il_push(x,10);
	CuAssertIntEquals(tc, 10, il_get(x, 0));
	il_push(x,20);
	CuAssertIntEquals(tc, 10, il_get(x, 0));
	CuAssertIntEquals(tc, 20, il_get(x, 1));
	il_free(x);
}

void test_il_remove_value(CuTest* tc) {
	il* x = il_new(5);
	il_push(x,10);
	il_push(x,20);
	il_push(x,30);
	il_push(x,87);
	il_push(x,87);
	il_push(x,87);
	il_push(x,87);
	il_push(x,87);
	il_push(x,92);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	CuAssertIntEquals(tc, 8, il_remove_value(x, 92));
	CuAssertIntEquals(tc, -1, il_remove_value(x, 37));
	CuAssertIntEquals(tc, -1, il_remove_value(x, 0));
	CuAssertIntEquals(tc, 2, il_remove_value(x, 30));
	CuAssertIntEquals(tc, 0, il_remove_value(x, 10));
	CuAssertIntEquals(tc, 0, il_remove_value(x, 20));
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	il_free(x);
}

void test_il_contains(CuTest* tc) {
	il* x = il_new(4);
	il_push(x,10);
	il_push(x,20);
	il_push(x,30);
	il_push(x,30);
	il_push(x,30);
	il_push(x,41);
	il_push(x,30);
	il_push(x,81);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	CuAssertIntEquals(tc, 1, il_contains(x, 10));
	CuAssertIntEquals(tc, 1, il_contains(x, 20));
	CuAssertIntEquals(tc, 1, il_contains(x, 30));
	CuAssertIntEquals(tc, 1, il_contains(x, 81));
	CuAssertIntEquals(tc, 1, il_contains(x, 41));
	CuAssertIntEquals(tc, 0, il_contains(x, 42));
	il_remove_value(x, 41);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	CuAssertIntEquals(tc, 0, il_contains(x, 41));
	il_free(x);
}

void test_il_insert_ascending(CuTest* tc) {
	int i;
	il* x = il_new(4);
	il_insert_ascending(x,2);
	il_insert_ascending(x,4);
	il_insert_ascending(x,8);
	il_insert_ascending(x,5);
	il_insert_ascending(x,6);
	il_insert_ascending(x,7);
	il_insert_ascending(x,1);
	il_insert_ascending(x,3);
	il_insert_ascending(x,0);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	CuAssertIntEquals(tc, il_check_sorted_ascending(x, 0), 0);
	for (i=0;i<il_size(x);i++)
		CuAssertIntEquals(tc, i, il_get(x, i));
	for (i=0;i<il_size(x);i++)
		CuAssertIntEquals(tc, i, il_find_index_ascending(x, i));
	il_free(x);
}

void test_il_insert_descending(CuTest* tc) {
	int i;
	il* x = il_new(4);
	il_insert_descending(x,2);
	il_insert_descending(x,4);
	il_insert_descending(x,8);
	il_insert_descending(x,5);
	il_insert_descending(x,6);
	il_insert_descending(x,7);
	il_insert_descending(x,1);
	il_insert_descending(x,3);
	il_insert_descending(x,0);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	CuAssertIntEquals(tc, il_check_sorted_descending(x, 0), 0);
	for (i=0;i<il_size(x);i++)
		CuAssertIntEquals(tc, il_size(x)-i-1, il_get(x, i));
	il_free(x);
}


void test_il_insert_unique_ascending(CuTest* tc) {
	int i;
	il* x = il_new(4);
	il_insert_unique_ascending(x,2);
	il_insert_unique_ascending(x,4);
	il_insert_unique_ascending(x,4);
	il_insert_unique_ascending(x,7);
	il_insert_unique_ascending(x,4);
	il_insert_unique_ascending(x,4);
	il_insert_unique_ascending(x,8);
	il_insert_unique_ascending(x,5);
	il_insert_unique_ascending(x,0);
	il_insert_unique_ascending(x,5);
	il_insert_unique_ascending(x,5);
	il_insert_unique_ascending(x,5);
	il_insert_unique_ascending(x,4);
	il_insert_unique_ascending(x,5);
	il_insert_unique_ascending(x,6);
	il_insert_unique_ascending(x,7);
	il_insert_unique_ascending(x,7);
	il_insert_unique_ascending(x,7);
	il_insert_unique_ascending(x,7);
	il_insert_unique_ascending(x,1);
	il_insert_unique_ascending(x,1);
	il_insert_unique_ascending(x,3);
	il_insert_unique_ascending(x,1);
	il_insert_unique_ascending(x,1);
	il_insert_unique_ascending(x,0);
	//il_print(x);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	CuAssertIntEquals(tc, il_check_sorted_ascending(x, 1), 0);
	for (i=0;i<il_size(x);i++)
		CuAssertIntEquals(tc, i, il_get(x, i));
	il_free(x);
}

void test_il_copy(CuTest* tc) {
	int i, N=60, start=10, length=10;
	int buf[N];
	il* x = il_new(4);
	memset(buf, 0, N);
	for (i=0;i<N;i++) 
		il_push(x,i);
	il_copy(x, start, length, buf);
	for (i=0;i<length;i++) 
		CuAssertIntEquals(tc, start+i, buf[i]);
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	il_free(x);
}

void test_il_dupe(CuTest* tc) {
	int i, N=63;
	il* x = il_new(4), *y;
	for (i=0;i<N;i++) 
		il_push(x,i);
	y = il_dupe(x);
	for (i=0;i<N;i++) 
		CuAssertIntEquals(tc, i, il_get(y, i));
	for (i=0;i<N;i++) 
		il_pop(x);
	CuAssertIntEquals(tc, N, il_size(y));
	CuAssertIntEquals(tc, il_check_consistency(x), 0);
	il_free(x);
	il_free(y);
}

void test_delete(CuTest* tc) {
	il* bl = il_new(4);
	il_push(bl, 42);
	il_push(bl, 43);
	il_push(bl, 47);
	il_push(bl, 49);

	il_remove(bl, 0);
	il_remove(bl, 0);
	il_remove(bl, 0);
	il_remove(bl, 0);

	CuAssertIntEquals(tc, il_size(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
	CuAssertIntEquals(tc, il_check_consistency(bl), 0);
}

void test_delete_2(CuTest* tc) {
	il* bl = il_new(2);
	il_push(bl, 42);
	il_push(bl, 43);
	il_push(bl, 47);
	il_push(bl, 49);

	il_remove(bl, 0);
	il_remove(bl, 0);
	il_remove(bl, 0);
	il_remove(bl, 0);

	CuAssertIntEquals(tc, il_size(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
	CuAssertIntEquals(tc, il_check_consistency(bl), 0);
}

void test_delete_3(CuTest* tc) {
	il* bl;
	bl = il_new(2);
	il_push(bl, 42);
	il_push(bl, 43);
	il_push(bl, 47);
	il_push(bl, 49);

	il_remove(bl, 3);
	il_remove(bl, 2);
	il_remove(bl, 1);
	il_remove(bl, 0);

	CuAssertIntEquals(tc, il_size(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
	CuAssertIntEquals(tc, il_check_consistency(bl), 0);
}

void test_set(CuTest* tc) {
	il* bl;
	bl = il_new(2);
	CuAssertIntEquals(tc, il_size(bl), 0);
	il_push(bl, 42);
	il_push(bl, 43);
	il_push(bl, 47);
	il_push(bl, 49);

	il_set(bl, 0, 0);
	il_set(bl, 1, 1);
	il_set(bl, 2, 2);

	CuAssertIntEquals(tc, il_size(bl), 4);
	CuAssertIntEquals(tc, 0, il_get(bl, 0));
	CuAssertIntEquals(tc, 1, il_get(bl, 1));
	CuAssertIntEquals(tc, 2, il_get(bl, 2));
	CuAssertIntEquals(tc, il_check_consistency(bl), 0);
}

void test_delete_4(CuTest* tc) {
	int i, j, N;
	il* bl = il_new(20);
	N = 100;
	for (i=0; i<N; i++)
		il_push(bl, i);

	for (i=0; i<N; i++) {
		int ind = rand() % il_size(bl);
		il_remove(bl, ind);

		for (j=1; j<il_size(bl); j++) {
			CuAssert(tc, "mono", (il_get(bl, j) - il_get(bl, j-1)) > 0);
		}
	}

	CuAssertIntEquals(tc, il_size(bl), 0);
	CuAssertPtrEquals(tc, bl->head, NULL);
	CuAssertPtrEquals(tc, bl->tail, NULL);
	CuAssertIntEquals(tc, il_check_consistency(bl), 0);
}

/******************************************************************************/
/****************************** double lists **********************************/
/******************************************************************************/


void test_dl_push(CuTest* tc) {
	dl* bl;
	bl = dl_new(2);
	CuAssertIntEquals(tc, il_size(bl), 0);
	dl_push(bl, 42.0);
	dl_push(bl, 43.0);
	dl_push(bl, 47.0);
	dl_push(bl, 49.0);

	dl_set(bl, 0, 0.0);
	dl_set(bl, 1, 1.0);
	dl_set(bl, 2, 2.0);

	CuAssertIntEquals(tc, il_size(bl), 4);
	CuAssert(tc, "dl", 0.0 == dl_get(bl, 0));
	CuAssert(tc, "dl", 1.0 == dl_get(bl, 1));
	CuAssert(tc, "dl", 2.0 == dl_get(bl, 2));
	CuAssertIntEquals(tc, dl_check_consistency(bl), 0);
}
