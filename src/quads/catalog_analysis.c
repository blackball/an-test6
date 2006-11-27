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

struct ll_node {
	uint data;
	struct ll_node *next;
};

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
		uint j;
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

void fill_maps(char *minmap, char *maxmap, uint hpx, uint Nside, 
		rect_field *curfield)
{
	char *visited = malloc(12 * Nside * Nside * sizeof(char));
	uint *neighbours = malloc(8 * sizeof(uint));
	struct ll_node *head = NULL;

	int i, j;

	for (j = 0; j < 12 * Nside * Nside; j++) visited[j] = 0;

	while (hpx != -1)
	{
		int nneighbours; 
		double thishpx_coords[3];
		
		healpix_to_xyzarr_lex(0.5, 0.5, hpx, Nside, thishpx_coords);
		if (!is_inside_rect_field(curfield, thishpx_coords))
			goto getnext;
		
		maxmap[hpx] = 1;

		nneighbours = healpix_get_neighbours_nside(hpx, neighbours, Nside);
		
		for (i = 0; i < nneighbours; i++)
		{
			double coords[3];
			healpix_to_xyzarr_lex(0.5, 0.5, neighbours[i], Nside, coords);
			if (!is_inside_rect_field(curfield, coords))
				break;
		}

		if (i == nneighbours) {
			minmap[hpx] = 1;
		}
		for (i = 0; i < nneighbours; i++) {
			if (!visited[neighbours[i]]) {
				struct ll_node *n = malloc(sizeof(struct ll_node));
				n->data = neighbours[i];
				n->next = head;
				head = n;
				visited[neighbours[i]] = 1;
			}
		}
		getnext:
		if (head == NULL)
			hpx = -1;
		else {
			struct ll_node *oldhead = head;
			hpx = head->data;
			head = head->next;
			free(oldhead);
		}
	}
	free(neighbours);
	free(visited);
}

int main(int argc, char **argv)
{
	double max;
	rect_field curfield;
	int filled_min = 0, filled_max = 0;
	int *hpmap_min, *hpmap_max;
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
	hpmap_min = malloc(12 * Nside * Nside * sizeof(int));
	hpmap_max = malloc(12 * Nside * Nside * sizeof(int));

	for (i = 0; i < 12 * Nside * Nside; i++) {
		hpmap_min[i] = 0;
		hpmap_max[i] = 0;
	}
		
	curfield.corners = malloc(3 * 4 * sizeof(double));
	fields = 0;
	while (fgets(buf, BUFSIZE, stdin) != NULL)
	{
		uint centerhp;
		uint *neighbours;
		int nneighbours;
		int i, j;
		double center[3];
		center[0] = center[1] = center[2] = 0;
		fields++;
		//printf("Doing field %d\n",fields);
		curfield.corners[0] = atof(strtok(buf, "\t"));
		
		/* 12 = 3 coords x 4 pts, got 1 */
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
		
		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 3; j++)
			{
				center[j] += curfield.corners[3*i + j];
			}
		}
		printf("center = %f, %f, %f\n",center[0], center[1], center[2]);
		for (i = 0; i < 3; i++)
			center[i] /= 4;
		normalize_3(center);
		centerhp = xyztohealpix_nside(center[0], center[1], 
				center[2], (uint)Nside);
		fill_maps(hpmap_min, hpmap_max, centerhp, (uint)Nside, &curfield);
	}
	for (i = 0; i < 12 * Nside * Nside; i++)
	{
		if (hpmap_min[i])
			filled_min++;
		if (hpmap_max[i])
			filled_max++;
	}
	
	max = 12 * Nside * Nside;
	printf("Min: %f, Max: %f\n",((double)filled_min) / max, ((double)filled_max) / max);

	return 0;
}
