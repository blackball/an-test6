#include <stdio.h>
#include <stdlib.h>
int dfind(int *image, int nx, int ny, int *object);
int dfind2(int *image, int nx, int ny, int *object);
int compare_inputs(int *test_data, int nx, int ny) {
	int *test_outs_keir = calloc(nx*ny, sizeof(int));
	int *test_outs_blanton = calloc(nx*ny, sizeof(int));
	int fail = 0;
	dfind2(test_data, nx,ny,test_outs_keir);
	dfind(test_data, nx,ny,test_outs_blanton);

	int ix, iy;
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

