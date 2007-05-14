#include <stdio.h>
#include <string.h>

#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_vector_double.h>

int main() {
	int ret;
	gsl_matrix A;
	double data[9];
	int i, j;
	gsl_vector* tau;
	gsl_matrix *Q, *R;

	memset(&A, 0, sizeof(gsl_matrix));

	A.size1 = 3;
	A.size2 = 3;
	A.tda = 3;
	A.data = data;

	gsl_matrix_set(&A, 0, 0, 34.0);
	gsl_matrix_set(&A, 0, 1, 4.0);
	gsl_matrix_set(&A, 0, 2, 14.0);
	gsl_matrix_set(&A, 1, 0, 1.0);
	gsl_matrix_set(&A, 1, 1, 8.0);
	gsl_matrix_set(&A, 1, 2, 3.0);
	gsl_matrix_set(&A, 2, 0, 7.0);
	gsl_matrix_set(&A, 2, 1, 1.0);
	gsl_matrix_set(&A, 2, 2, 8.0);

	for (i=0; i<A.size1; i++) {
		printf((i==0) ? "A = (" : "    (");
		for (j=0; j<A.size2; j++) {
			printf(" %12.5g ", gsl_matrix_get(&A, i, j));
		}
		printf(")\n");
	}
	printf("\n");

	tau = gsl_vector_alloc(3);

	ret = gsl_linalg_QR_decomp(&A, tau);

	Q = gsl_matrix_alloc(3, 3);
	R = gsl_matrix_alloc(3, 3);

	ret = gsl_linalg_QR_unpack(&A, tau, Q, R);

	for (i=0; i<Q->size1; i++) {
		printf((i==0) ? "Q = (" : "    (");
		for (j=0; j<Q->size2; j++) {
			printf(" %12.5g ", gsl_matrix_get(Q, i, j));
		}
		printf(")\n");
	}
	printf("\n");

	for (i=0; i<R->size1; i++) {
		printf((i==0) ? "R = (" : "    (");
		for (j=0; j<R->size2; j++) {
			printf(" %12.5g ", gsl_matrix_get(R, i, j));
		}
		printf(")\n");
	}
	printf("\n");

	gsl_matrix_free(Q);
	gsl_matrix_free(R);
	gsl_vector_free(tau);

	return 0;
}
