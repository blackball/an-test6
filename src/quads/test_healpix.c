#include <math.h>
#include <stdio.h>

#include "CUnit/CUnit.h"
#include "CUnit/Basic.h"

#include "healpix.h"

void print_neighbours(int* n, int nn) {
	int i;
	printf("[ ");
	for (i=0; i<nn; i++) {
		printf("%i ", n[i]);
	}
	printf("]");
}

void test_neighbours(int pix, int* true_neigh, int true_nn, int Nside) {
	int neigh[8];
	int nn;
	int i;
	for (i=0; i<8; i++) {
		neigh[i] = -1;
	}
	nn = healpix_get_neighbours_nside(pix, neigh, Nside);
	printf("true(%i) : ", pix);
	print_neighbours(true_neigh, true_nn);
	printf("\n");
	printf("got (%i) : ", pix);
	print_neighbours(neigh, nn);
	printf("\n");

	CU_ASSERT_EQUAL_FATAL(nn, true_nn);

	for (i=0; i<true_nn; i++) {
		CU_ASSERT_EQUAL_FATAL(neigh[i], true_neigh[i]);
	}
}

void test_healpix_neighbours() {
	int n0[] = { 1,3,2,71,69,143,90,91 };
	int n5[] = { 26,27,7,6,4,94,95 };
	int n13[] = { 30,31,15,14,12,6,7,27 };
	int n15[] = { 31,47,63,61,14,12,13,30 };
	int n30[] = { 31,15,13,7,27,25,28,29 };
	int n101[] = { 32,34,103,102,100,174,175,122 };
	int n127[] = { 58,37,36,126,124,125,56 };
	int n64[] = { 65,67,66,183,181,138,139 };
	int n133[] = { 80,82,135,134,132,152,154 };
	int n148[] = { 149,151,150,147,145,162,168,170 };
	int n160[] = { 161,163,162,145,144,128,176,178 };
	int n24[] = { 25,27,26,95,93,87,18,19 };
	int n42[] = { 43,23,21,111,109,40,41 };
	int n59[] = { 62,45,39,37,58,56,57,60 };
	int n191[] = { 74,48,117,116,190,188,189,72 };
	int n190[] = { 191,117,116,113,187,185,188,189 };
	int n186[] = { 187,113,112,165,164,184,185 };
	int n184[] = { 185,187,186,165,164,161,178,179 };

	test_neighbours(0,   n0,   sizeof(n0)  /sizeof(int), 4);
	test_neighbours(5,   n5,   sizeof(n5)  /sizeof(int), 4);
	test_neighbours(13,  n13,  sizeof(n13) /sizeof(int), 4);
	test_neighbours(15,  n15,  sizeof(n15) /sizeof(int), 4);
	test_neighbours(30,  n30,  sizeof(n30) /sizeof(int), 4);
	test_neighbours(101, n101, sizeof(n101)/sizeof(int), 4);
	test_neighbours(127, n127, sizeof(n127)/sizeof(int), 4);
	test_neighbours(64,  n64,  sizeof(n64) /sizeof(int), 4);
	test_neighbours(133, n133, sizeof(n133)/sizeof(int), 4);
	test_neighbours(148, n148, sizeof(n148)/sizeof(int), 4);
	test_neighbours(160, n160, sizeof(n160)/sizeof(int), 4);
	test_neighbours(24,  n24,  sizeof(n24) /sizeof(int), 4);
	test_neighbours(42,  n42,  sizeof(n42) /sizeof(int), 4);
	test_neighbours(59,  n59,  sizeof(n59) /sizeof(int), 4);
	test_neighbours(191, n191, sizeof(n191)/sizeof(int), 4);
	test_neighbours(190, n190, sizeof(n190)/sizeof(int), 4);
	test_neighbours(186, n186, sizeof(n186)/sizeof(int), 4);
	test_neighbours(184, n184, sizeof(n184)/sizeof(int), 4);
}

int main(int argc, char** args) {

	CU_ErrorCode errcode;
	CU_pSuite suite;
	//char* suitename;

	if ((errcode = CU_initialize_registry()) != CUE_SUCCESS) {
		fprintf(stderr, "initialize_registry failed.\n");
		return -1;
	}

	if ((suite = CU_add_suite("healpix", NULL, NULL)) == NULL) {
		fprintf(stderr, "add_suite failed.\n");
		return -1;
	}

	if (!CU_add_test(suite, "neighbour", test_healpix_neighbours)) {
		fprintf(stderr, "add_test failed.\n");
		return -1;
	}

	//CU_console_run_tests();

	CU_basic_set_mode(CU_BRM_VERBOSE);

	CU_basic_run_tests();

	CU_cleanup_registry();


	/*
	  int rastep, decstep;
	  int Nra = 100;
	  int Ndec = 100;
	  double ra, dec;
	  int healpix;
	  printf("radechealpix=zeros(%i,3);\n", Nra*Ndec);
	  for (rastep=0; rastep<Nra; rastep++) {
	  ra = ((double)rastep / (double)(Nra-1)) * 2.0 * M_PI;
	  for (decstep=0; decstep<Ndec; decstep++) {
	  dec = (((double)decstep / (double)(Ndec-1)) * M_PI) - M_PI/2.0;
	  healpix = radectohealpix(ra, dec);
	  printf("radechealpix(%i,:)=[%g,%g,%i];\n", 
	  (rastep*Ndec) + decstep + 1, ra, dec, healpix);
	  }
	  }
	  printf("ra=radechealpix(:,1);\n");
	  printf("dec=radechealpix(:,2);\n");
	  printf("healpix=radechealpix(:,3);\n");
	  return 0;
	*/
	return 0;
}
