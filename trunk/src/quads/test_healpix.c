#include <math.h>
#include <stdio.h>

#include "CuTest.h"
#include "starutil.h"
#include "healpix.h"

void print_neighbours(int* n, int nn) {
	int i;
	printf("[ ");
	for (i=0; i<nn; i++) {
		printf("%i ", n[i]);
	}
	printf("]");
}

void test_neighbours(CuTest* ct, int pix, int* true_neigh, int true_nn, int Nside) {
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

	CuAssertIntEquals(ct, nn, true_nn);

	for (i=0; i<true_nn; i++) {
		CuAssertIntEquals(ct, neigh[i], true_neigh[i]);
	}
}

void print_node(double z, double phi, int Nside) {
	double ra, dec;
	int hp;
	int nn;
	int neigh[8];
	int k;

	double scale = 10.0;

	ra = phi;
	dec = asin(z);
	while (ra < 0.0)
		ra += 2.0 * M_PI;
	while (ra > 2.0 * M_PI)
		ra -= 2.0 * M_PI;

	// find its healpix.
	hp = radectohealpix_nside(ra, dec, Nside);
	// find its neighbourhood.
	nn = healpix_get_neighbours_nside(hp, neigh, Nside);
	fprintf(stderr, "  N%i [ label=\"%i\", pos=\"%g,%g!\" ];\n", hp, hp,
			scale * ra/M_PI, scale * z);
	for (k=0; k<nn; k++) {
		fprintf(stderr, "  N%i -- N%i\n", hp, neigh[k]);
	}
}

void test_healpix_neighbours(CuTest *ct) {
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

	test_neighbours(ct, 0,   n0,   sizeof(n0)  /sizeof(int), 4);
	test_neighbours(ct, 5,   n5,   sizeof(n5)  /sizeof(int), 4);
	test_neighbours(ct, 13,  n13,  sizeof(n13) /sizeof(int), 4);
	test_neighbours(ct, 15,  n15,  sizeof(n15) /sizeof(int), 4);
	test_neighbours(ct, 30,  n30,  sizeof(n30) /sizeof(int), 4);
	test_neighbours(ct, 101, n101, sizeof(n101)/sizeof(int), 4);
	test_neighbours(ct, 127, n127, sizeof(n127)/sizeof(int), 4);
	test_neighbours(ct, 64,  n64,  sizeof(n64) /sizeof(int), 4);
	test_neighbours(ct, 133, n133, sizeof(n133)/sizeof(int), 4);
	test_neighbours(ct, 148, n148, sizeof(n148)/sizeof(int), 4);
	test_neighbours(ct, 160, n160, sizeof(n160)/sizeof(int), 4);
	test_neighbours(ct, 24,  n24,  sizeof(n24) /sizeof(int), 4);
	test_neighbours(ct, 42,  n42,  sizeof(n42) /sizeof(int), 4);
	test_neighbours(ct, 59,  n59,  sizeof(n59) /sizeof(int), 4);
	test_neighbours(ct, 191, n191, sizeof(n191)/sizeof(int), 4);
	test_neighbours(ct, 190, n190, sizeof(n190)/sizeof(int), 4);
	test_neighbours(ct, 186, n186, sizeof(n186)/sizeof(int), 4);
	test_neighbours(ct, 184, n184, sizeof(n184)/sizeof(int), 4);
}

int main(int argc, char** args) {

	/* Run all tests */
	CuString *output = CuStringNew();
	CuSuite* suite = CuSuiteNew();

	/* Add new tests here */
	SUITE_ADD_TEST(suite, test_healpix_neighbours);

	/* Run the suite, collect results and display */
	CuSuiteRun(suite);
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);

	fprintf(stderr, "graph Nside4 {\n");
	{
		int Nside = 8;
		int i, j;
		double z;
		double phi;

		// north polar
		for (i=1; i<=Nside; i++) {
			for (j=1; j<=(4*i); j++) {
				// find the center of the pixel in ring i
				// and longitude j.
				z = 1.0 - square((double)i / (double)Nside)/3.0;
				phi = M_PI / (2.0 * i) * ((double)j - 0.5);
				fprintf(stderr, "  // North polar, i=%i, j=%i.  z=%g, phi=%g\n", i, j, z, phi);
				print_node(z, phi, Nside);
			}
		}
		// south polar
		for (i=1; i<=Nside; i++) {
			for (j=1; j<=(4*i); j++) {
				z = 1.0 - square((double)i / (double)Nside)/3.0;
				z *= -1.0;
				phi = M_PI / (2.0 * i) * ((double)j - 0.5);
				fprintf(stderr, "  // South polar, i=%i, j=%i.  z=%g, phi=%g\n", i, j, z, phi);
				print_node(z, phi, Nside);
			}
		}
		// north equatorial
		for (i=Nside+1; i<=2*Nside; i++) {
			for (j=1; j<=(4*Nside); j++) {
				int s;
				z = 4.0/3.0 - 2.0 * i / (3.0 * Nside);
				s = (i - Nside + 1) % 2;
				s = (s + 2) % 2;
				phi = M_PI / (2.0 * Nside) * ((double)j - (double)s / 2.0);
				fprintf(stderr, "  // North equatorial, i=%i, j=%i.  z=%g, phi=%g, s=%i\n", i, j, z, phi, s);
				print_node(z, phi, Nside);
			}
		}
		// south equatorial
		for (i=Nside+1; i<2*Nside; i++) {
			for (j=1; j<=(4*Nside); j++) {
				int s;
				z = 4.0/3.0 - 2.0 * i / (3.0 * Nside);
				z *= -1.0;
				s = (i - Nside + 1) % 2;
				s = (s + 2) % 2;
				phi = M_PI / (2.0 * Nside) * ((double)j - s / 2.0);
				fprintf(stderr, "  // South equatorial, i=%i, j=%i.  z=%g, phi=%g, s=%i\n", i, j, z, phi, s);
				print_node(z, phi, Nside);
			}
		}




	}
	fprintf(stderr, "  node [ shape=point ]\n");
	fprintf(stderr, "  C0 [ pos=\"0,-10!\" ];\n");
	fprintf(stderr, "  C1 [ pos=\"20,-10!\" ];\n");
	fprintf(stderr, "  C2 [ pos=\"20,10!\" ];\n");
	fprintf(stderr, "  C3 [ pos=\"0,10!\" ];\n");
	fprintf(stderr, "  C0 -- C1 -- C2 -- C3 -- C0\n");
	fprintf(stderr, "}\n");


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
