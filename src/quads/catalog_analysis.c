#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "healpix.h"
#include "mathutil.h"
#include "pnpoly.h"

#define BUFSIZE 1000

typedef struct {
	double *corners;
} rect_field;

struct ll_node {
	uint data;
	struct ll_node *next;
};

static Inline bool ispowerof4(uint x) {
	if (x >= 0x40000)
		return (					x == 0x40000   ||
				 x == 0x100000   || x == 0x400000  ||
				 x == 0x1000000  || x == 0x4000000 ||
				 x == 0x10000000 || x == 0x40000000);
	else
		return (x == 0x1		|| x == 0x4	   ||
				x == 0x10	   || x == 0x40	  ||
				x == 0x100	  || x == 0x400	 ||
				x == 0x1000	 || x == 0x4000	||
				x == 0x10000);
}


static double nmin(double *p, int n)
{
	int i;
	double cmin;
	assert (n > 0);
	cmin = p[0];
	for (i = 1; i < n; i++)
	{   
		if (p[i] < cmin)
		cmin = p[i];
	}
	return cmin;
}

static int nminind(double *p, int n)
{
	int i;
	int cminind;
	
	assert (n > 0);
	cminind = 0;
	for (i = 1; i < n; i++)
	{   
		if (p[i] < p[cminind])
		{   
			cminind = i;
		}
	}
	return cminind;
}

static double nmax(double *p, int n)
{
	int i;
	double cmax;

	assert (n > 0);
	cmax = p[0];
	for (i = 1; i < n; i++)
	{   
		if (p[i] > cmax)
			cmax = p[i];
	}
	return cmax;															  
}																			 

static double range(double *p, int n)
{
	double max = nmax (p, n);
	double min = nmax (p, n);
	return fabs (max - min);
}


int is_inside_rect_field(rect_field *f, double *p)
{
	double coordtrans[12];
	double *coord1;
	double *coord2;
	double *x = coordtrans;
	double *y = coordtrans + 4;
	double *z = coordtrans + 8;
	double t_coord1, t_coord2;
	double ranges[3];
	int i;
	int xgot = 0, ygot = 0, zgot = 0;
	for (i = 0; i < 12; i++)
	{
		int j = i % 3;
		switch (j)
		{
			case 0:
				coordtrans[xgot++] = f->corners[i];
				break;
			case 1:
				coordtrans[4 + ygot++] = f->corners[i];
				break;
			case 2:
				coordtrans[8 + zgot++] = f->corners[i];
				break;
		}
	}
	ranges[0] = range(x, 4);
	ranges[1] = range(y, 4);
	ranges[2] = range(z, 4);
	
	switch (nminind(ranges, 3))
	{
		case 0:
			coord1 = y;
			coord2 = z;
			t_coord1 = p[1];
			t_coord2 = p[2];
			break;
		case 1:
			coord1 = x;
			coord2 = z;
			t_coord1 = p[0];
			t_coord2 = p[2];
			break;
		case 2:
			coord1 = x;
			coord2 = y;
			t_coord1 = p[0];
			t_coord2 = p[1];
			break;
		default:
			return -1;
	}
	return point_in_poly(coord1, coord2, 4, t_coord1, t_coord2);

}

void fill_maps_recursive(char *minmap, char *maxmap, uint hpx, uint Nside,
		rect_field *curfield, char *visited)
{
	double thishpx_coords[3];
	uint neighbours[8];
	uint nn;
	if (visited[hpx / 8] & (1 << (hpx % 8)))
		return;
	visited[hpx / 8] |= 1 << (hpx % 8);
	healpix_to_xyzarr_lex(0.5, 0.5, hpx, Nside, thishpx_coords);
	//printf("Examining healpix %d, centered at (%f, %f, %f)\n", hpx, 
	//		thishpx_coords[0], thishpx_coords[1], thishpx_coords[2]);
	if (is_inside_rect_field(curfield, thishpx_coords))
	{
		int j;
		maxmap[hpx / 8] |= (1 << (hpx % 8));
		nn = healpix_get_neighbours_nside(hpx, neighbours, Nside);
		for (j = 0; j < nn; j++)
		{
				
			double ncoords[3];
			healpix_to_xyzarr_lex(0.5, 0.5, neighbours[j], Nside, ncoords);

			//printf("- Examining neighbour healpix %d, centered at (%f, %f, %f)\n", neighbours[j],
			//	ncoords[0], ncoords[1], ncoords[2]);
			
			
			if (!is_inside_rect_field(curfield, ncoords)) {
				//printf("-- Not in field, breaking off neighbour search\n");
				break;
			}
		}
		if (j == nn)
			minmap[(hpx / 8)] |= (1 << (hpx % 8));
		for (j = 0; j < nn; j++)
		{
			//printf("Recursing on neighbour of %d, %d\n", hpx, neighbours[j]);
			fill_maps_recursive(minmap, maxmap, neighbours[j], Nside,
					curfield, visited);
		}
	}
	else {
		//printf("Healpix %d not in field\n", hpx);
		return; // unnecessary I know
	}
}
void fill_maps(char *minmap, char *maxmap, uint hpx, uint Nside,
		rect_field *curfield)
{
	char *visited = malloc(2 * Nside * Nside * sizeof(char));
	uint i;
	uint visitedcnt = 0;
	for (i = 0; i < 2 * Nside * Nside; i++)
		visited[i] = 0;

	fill_maps_recursive(minmap, maxmap, hpx, Nside, curfield, visited);
}
#if 0
void fill_maps(char *minmap, char *maxmap, uint hpx, uint Nside,
				rect_field *curfield)

{
	bool done = FALSE;
	char *visited = malloc(2 * Nside * Nside * sizeof(char));
	int i;
	struct ll_node *queue = NULL;
	double thishpx_coords[3];
	uint visitedcnt = 0;
		
	for (i = 0; i < 2 * Nside * Nside; i++)
		visited[i] = 0;	

	do {
		uint nn;
		bool found_neighbour_outside = FALSE;
		uint neighbours[8];
		if (visited[hpx / 8] & (1 << (hpx % 8)))
		{
			assert(1 == 0);
			goto getnext;
		}
		visited[hpx / 8] |= (1 << (hpx % 8));
		
		healpix_to_xyzarr_lex(0.5, 0.5, hpx, Nside, thishpx_coords);
		if (!is_inside_rect_field(curfield, thishpx_coords))
			goto getnext;

		maxmap[hpx / 8] |= 1 << (hpx % 8);
		nn = healpix_get_neighbours_nside(hpx, neighbours, Nside);
		for (i = 0; i < nn; i++)
		{
			double ncoords[3];
			struct ll_node *newnode;
			healpix_to_xyzarr_lex(0.5, 0.5, neighbours[i], Nside, ncoords);
			if (!is_inside_rect_field(curfield, ncoords))
			{
				found_neighbour_outside = TRUE;
			}
			if (!(visited[neighbours[i] / 8] & (1 << (neighbours[i] % 8))))
			{
				newnode = malloc(sizeof(struct ll_node));
				newnode->next = queue;
				newnode->data = neighbours[i];
				queue = newnode;
			}
		}
		if (!found_neighbour_outside)
			minmap[hpx / 8] |= 1 << (hpx % 8);
		
		getnext:
		if (queue != NULL)
		{
			struct ll_node *newhead = queue->next;
			hpx = queue->data;
			free(queue);
			queue = newhead;
		}
		else {
			done = TRUE;
		}
	} while (!done);
	
	free(visited);
}
#endif

int main(int argc, char **argv)
{
	double max;
	rect_field curfield;
	int filled_min = 0, filled_max = 0;
	char *hpmap_min, *hpmap_max;
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
	hpmap_min = malloc(2 * Nside * Nside * sizeof(char));
	hpmap_max = malloc(2 * Nside * Nside * sizeof(char));
	if (hpmap_min == NULL || hpmap_max == NULL)
	{
		fprintf(stderr, "malloc failed!\n");
		exit(1);
	}	
	for (i = 0; i < 2 * Nside * Nside; i++) {
		hpmap_min[i] = 0;
		hpmap_max[i] = 0;
	}
		
	curfield.corners = malloc(3 * 4 * sizeof(double));
	fields = 0;
	while (fgets(buf, BUFSIZE, stdin) != NULL)
	{
		uint centerhp;
		uint *neighbours;
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
		for (i = 0; i < 3; i++)
			center[i] /= 4;
		normalize_3(center);
		//printf("center = %f, %f, %f\n",center[0], center[1], center[2]);
		centerhp = xyztohealpix_nside(center[0], center[1], 
				center[2], (uint)Nside);
		fill_maps(hpmap_min, hpmap_max, centerhp, (uint)Nside, &curfield);
	}
	for (i = 0; i < 12 * Nside * Nside; i++)
	{
		if (hpmap_min[i / 8] & (1 << (i % 8)))
			filled_min++;
		if (hpmap_max[i / 8] & (1 << (i % 8)))
			filled_max++;
	}
	max = 12 * Nside * Nside;
	printf("Min: %f, Max: %f\n",((double)filled_min) / max, ((double)filled_max) / max);

	return 0;
}
