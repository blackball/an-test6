/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Michael Blanton, Keir Mierle, David Hogg, Sam Roweis
  and Dustin Lang.

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
#include <stdlib.h>
int dfind(int *image, int nx, int ny, int *object);
int dfind2(int *image, int nx, int ny, int *object);
int compare_inputs(int *test_data, int nx, int ny) {
	int *test_outs_keir = calloc(nx*ny, sizeof(int));
	int *test_outs_blanton = calloc(nx*ny, sizeof(int));
	int fail = 0;
	int ix, iy;
	dfind2(test_data, nx,ny,test_outs_keir);
	dfind(test_data, nx,ny,test_outs_blanton);

	for(iy=0; iy<9; iy++) {
		for (ix=0; ix<11; ix++) {
			if (!(test_outs_keir[nx*iy+ix] == test_outs_blanton[nx*iy+ix])) {
				printf("failure -- k%d != b%d\n",
						test_outs_keir[nx*iy+ix], test_outs_blanton[nx*iy+ix]);
				fail++;
			}
		}
	}
	return fail;
}


void test1() {
	int test_data[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	compare_inputs(test_data, 11, 9);
}

void test2() {
	int test_data[] = {1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1,
	                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0,
	                   0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0,
	                   0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0,
	                   0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1,
	                   0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0,
	                   0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0,
	                   0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0};
	compare_inputs(test_data, 11, 9);
}

void test3() {
	int test_data[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	                   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
	                   1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
	                   1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1,
	                   0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1,
	                   1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0,
	                   1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
	                   0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0};
	compare_inputs(test_data, 11, 9);
}

void test4() {
	int test_data[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	                   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
	                   1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
	                   1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1,
	                   0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1,
	                   1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0,
	                   1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
	                   0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0};
	compare_inputs(test_data, 11, 9);
}

void test5() {
	int test_data[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	                   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                   1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
	                   1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
	                   1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1,
	                   0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1,
	                   1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0,
	                   0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0,
	                   0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0};
	compare_inputs(test_data, 11, 9);
}

int main() {
	test1();
	printf("--------------------------------------------\n");
	test2();
	printf("--------------------------------------------\n");
	test3();
	printf("--------------------------------------------\n");
	test4();
	printf("--------------------------------------------\n");
	test5();
	return 0;
}

