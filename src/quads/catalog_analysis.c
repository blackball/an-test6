#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "healpix.h"
#include "mathutil.h"

#define BUFSIZE 1000

#define PI 3.1415926535897931
#define EPSILON 0.01

typedef struct {
	double *corners;
} rect_field;


int is_inside_rect_field(rect_field *f, double *p)
{
	int retval;
	double *side;
	double *v1, *v2, *norm;
	double dot_mypoint_norm, dot_c1_norm, u;
	double *pprime;
	double side2[3];
	double tot_angle;
	int i;	
	/* allocate space for two vectors that form the plane + the norm */
	v1 = malloc(sizeof(double) * 3);
	v2 = malloc(sizeof(double) * 3);
	norm = malloc(sizeof(double) * 3);
	
	/* fill the vectors */
	for (i = 0; i < 3; i++)
	{
		v1[i] = f->corners[i+3] - f->corners[i];
		v2[i] = f->corners[i+6] - f->corners[i+3];
	}
	/* Take the cross product to get the norm */
	cross_product(v1, v2, norm);
	
	/* Return false if vector to p is perpendicular to the norm */ 
	dot_mypoint_norm = dot_product_3(norm, p);
	
	if (dot_mypoint_norm == 0)
	{
		free(v1);
		free(v2);
		free(norm);
		return 0;
	}
	/* Otherwise, solve for the constant that gives us the intersect */
	dot_c1_norm = dot_product_3(norm, f->corners);
	u = dot_c1_norm / dot_mypoint_norm;

	/* Fill pprime with our point on the plane */
	pprime = malloc(sizeof(double) * 3);
	assert (u < 1);
	for (i = 0; i < 3; i++) pprime[i] = u*p[i];

	free(v1); free(v2);

	/* Reuse the space we allocated for norm */
	side = norm;
    tot_angle = 0;
	for (i = 0; i < 4; i++)
	{
		int j;
		double costheta;
		for (j = 0; j < 3; j++)
		{
			side[j] = f->corners[3 * i + j] - pprime[j];
			side2[j] = f->corners[3 * ((i + 1) % 4)] - pprime[j];
		}
		costheta = dot_product_3(side, side2);
		tot_angle += acos(costheta);
	}
	if (fabs(tot_angle - 2*PI) < EPSILON)
	{
		retval = 1;
	}
	else {
		retval = 0;
	}
	free(side);
	free(pprime);
	return retval;
}

int main(int argc, char **argv)
{
	rect_field curfield;
	int filled;
	int *hpmap;
	char *buf = malloc(BUFSIZE * sizeof(char));
	int ich, i;
	int Nside = -1;
	uint fields;
	while ((ich = getopt(argc, argv, "N:")) != EOF)
	{
		switch (ich)
		{
			case 'N':
				Nside = atoi(optarg);
				break;
		}
	}
	if (Nside < 1)
	{
		fprintf(stderr, "specify an Nside value with -N, > 1\n");
		exit(1);
	}
	hpmap = malloc(12 * Nside * Nside * sizeof(int));
	for (i = 0; i < 12 * Nside * Nside; i++) hpmap[i] = 0;
	curfield.corners = malloc(3 * 4 * sizeof(double));
	fields = 0;
	while (fgets(buf, BUFSIZE, stdin) != NULL)
	{
		int j;
		fields++;
		if (fields % 10 == 0) printf("Processing field %d\n", fields);
		curfield.corners[0] = atof(strtok(buf, "\t"));
		for (j = 1; j < 12; j++)
		{
			char *tok = strtok(NULL, "\t");
			if (tok == NULL)
			{
				fprintf(stderr, "Premature end of line!\n");
				exit(1);
			}
			curfield.corners[j] = atof(tok);
		}
		for (j = 0; j < 12 * Nside * Nside; j++)
		{
			double hp_coords[3];
			healpix_to_xyz(0.5, 0.5, (uint)j, (uint)Nside, &(hp_coords[0]), 
					&(hp_coords[1]), &(hp_coords[2]));
			if (is_inside_rect_field(&curfield, hp_coords))
			{
				hpmap[j] = 1;
			}
		}
	}
	filled = 0;
	for (i = 0; i < 12 * Nside * Nside; i++)
	{
		if (hpmap[i]) filled += 1;
	}
	printf("%d / %d\n", filled, 12 * Nside * Nside);
	return 0;
}
