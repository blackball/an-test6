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

#include <stdio.h>

#include "bt.h"

static int compare_ints(const void* v1, const void* v2) {
	int i1 = *(int*)v1;
	int i2 = *(int*)v2;
	if (i1 < i2) return -1;
	if (i1 > i2) return 1;
	return 0;
}

static void print_int(void* v1) {
	int i = *(int*)v1;
	printf("%i ", i);
}

int main() {
	int val;
	int i;
	bt* tree;

	tree = bt_new(sizeof(int), 4);

	printf("Empty:\n");
	bt_print(tree, print_int);
	printf("\n");

	{
		int vals[] = { 10, 5, 100, 10, 50, 50, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 200,200,200,200,200,250,150 };
		for (i=0; i<sizeof(vals)/sizeof(int); i++) {
			val = vals[i];
			printf("Insert %i:\n", val);
			bt_insert(tree, &val, 0, compare_ints);
			bt_print(tree, print_int);
			printf("\n");
		}
	}

	printf("Values: ");
	for (i=0; i<tree->N; i++) {
		printf("%i ", *(int*)bt_access(tree, i));
	}
	printf("\n");

	{
		int vals[] = { 0, 1, 2, 9, 10, 11, 49, 50, 51, 99, 100, 101, 149, 150, 151, 199, 200, 201, 249, 250, 251 };
		for (i=0; i<sizeof(vals)/sizeof(int); i++) {
			val = vals[i];
			printf("Contains %i: %s\n", val,
				   (bt_contains(tree, &val, compare_ints) ? "yes" : "no"));
		}
	}

	bt_free(tree);

	printf("Inserting a million items...\n");
	tree = bt_new(sizeof(int), 32);
	for (i=0; i<1000000; i++) {
		val = rand() % 1000;
		bt_insert(tree, &val, 0, compare_ints);
		//bt_check(tree);
	}
	printf("Checking...\n");
	bt_check(tree);
	printf("Done.\n");
	
	bt_free(tree);

	return 0;
}
