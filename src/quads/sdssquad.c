#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>

#define TRUE 1
#include "ppm.h"
#include "pnm.h"
#undef bool

#include "starutil.h"
#include "mathutil.h"
#include "rdlist.h"
#include "bl.h"
#include "permutedsort.h"

#define OPTIONS "s:S:q:f"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char fn[256];
	int i, j;

	char* sdss_rdls_template = "/p/learning/stars/SDSS_FIELDS/sdssfield%02i.rd.fits";
	char* sdss_xyls_template = "/p/learning/stars/SDSS_FIELDS/sdssfield%02i.xy.fits";

	int filenum = -1;
	int fieldnum = -1;
	rdlist* rdls = NULL;
	xylist* xyls = NULL;
	double* radec;
	int Nstars;
	il* quads = il_new(16);
	int field = 0;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'f':
			field = 1;
			break;
		case 's':
			filenum = atoi(optarg);
			break;
		case 'S':
			fieldnum = atoi(optarg);
			break;
		case 'q':
			il_append(quads, atoi(optarg));
			break;
		}

	if ((filenum == -1) || (fieldnum == -1) || !il_size(quads) || (il_size(quads) % 4)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "File %i, Field %i.\n", filenum, fieldnum);
	fprintf(stderr, "Quads: ");
	for (i=0; i<il_size(quads); i++)
		fprintf(stderr, "%i ", il_get(quads, i));
	fprintf(stderr, "\n");
		
	if (!field) {
		sprintf(fn, sdss_rdls_template, filenum);
		rdls = rdlist_open(fn);
		if (!rdls) {
			fprintf(stderr, "Failed to open RDLS file.\n");
			exit(-1);
		}
		Nstars = rdlist_n_entries(rdls, fieldnum);
	} else {
		sprintf(fn, sdss_xyls_template, filenum);
		xyls = xylist_open(fn);
		if (!xyls) {
			fprintf(stderr, "Failed to open XYLS file.\n");
			exit(-1);
		}
		xyls->xname = "COLC";
		xyls->yname = "ROWC";
		Nstars = xylist_n_entries(xyls, fieldnum);
	}
	if (Nstars == -1) {
		fprintf(stderr, "Failed to read XYLS/RDLS file.\n");
		exit(-1);
	}
	radec = malloc(Nstars * 2 * sizeof(double));
	if (!field) {
		if (rdlist_read_entries(rdls, fieldnum, 0, Nstars, radec)) {
			fprintf(stderr, "Failed to read RDLS file.\n");
			free(radec);
			exit(-1);
		}
	} else {
		if (xylist_read_entries(xyls, fieldnum, 0, Nstars, radec)) {
			fprintf(stderr, "Failed to read XYLS file.\n");
			free(radec);
			exit(-1);
		}
	}

	fprintf(stderr, "Got %i stars.\n", Nstars);

	printf("<quads>\n");
	if (!field) {
		for (j=0; j<il_size(quads)/4; j++) {
			double xyzABCD[12];
			double xyz0[3];
			double theta[4];
			int inds[4];
			int perm[4];
			for (i=0; i<4; i++) {
				inds[i] = il_get(quads, j*4 + i);
				if (inds[i] < 0 || inds[i] >= Nstars) {
					fprintf(stderr, "Invalid starid %i\n", inds[i]);
					exit(-1);
				}
				radec2xyzarr(deg2rad(radec[2*inds[i]]), deg2rad(radec[2*inds[i]+1]),
							 xyzABCD + 3*i);
			}
			star_midpoint(xyz0, xyzABCD + 3*0, xyzABCD + 3*1);
			for (i=0; i<4; i++) {
				double x, y;
				star_coords(xyzABCD + 3*i, xyz0, &x, &y);
				theta[i] = atan2(y, x);
				perm[i] = i;
			}
			permuted_sort_set_params(theta, sizeof(double), compare_doubles);
			permuted_sort(perm, 4);
			printf("  <quad");
			for (i=0; i<4; i++) {
				int ind = inds[perm[i]];
				double x, y;
				x = radec[2*ind];
				y = radec[2*ind+1];
				printf(" ra%i=\"%g\" dec%i=\"%g\"", i, x, i, y);
			}
			printf("/>\n");
		}
	} else {
		// see also sdssfieldtile.c
		double Xmax = 2048.0;
		double Ymax = 1500.0;
		double Xoffset = 0.22;
		double Xscale = 0.56;
		double Yoffset = 0.22;
		double Yscale = 0.56;

		for (j=0; j<il_size(quads)/4; j++) {
			double xyABCD[8];
			double theta[4];
			int perm[4];
			int inds[4];
			double xy0[2];
			for (i=0; i<4; i++) {
				inds[i] = il_get(quads, j*4 + i);
				if (inds[i] < 0 || inds[i] >= Nstars) {
					fprintf(stderr, "Invalid starid %i\n", inds[i]);
					exit(-1);
				}
				xyABCD[i*2 + 0] = radec[2*inds[i] + 0];
				xyABCD[i*2 + 1] = radec[2*inds[i] + 1];
			}
			xy0[0] = (xyABCD[0*2+0] + xyABCD[1*2+0]) * 0.5;
			xy0[1] = (xyABCD[0*2+1] + xyABCD[1*2+1]) * 0.5;
			for (i=0; i<4; i++) {
				double dx, dy;
				dx = xyABCD[i*2 + 0] - xy0[0];
				dy = xyABCD[i*2 + 1] - xy0[1];
				theta[i] = atan2(dy, dx);
				perm[i] = i;
			}
			permuted_sort_set_params(theta, sizeof(double), compare_doubles);
			permuted_sort(perm, 4);
			printf("  <quad");
			for (i=0; i<4; i++) {
				int ind = inds[perm[i]];
				double x, y, tmp;
				x = radec[2*ind]   / Xmax;
				y = radec[2*ind+1] / Ymax;

				// Switcheroo! (parity)
				tmp = x; x = y; y = tmp;

				x = x * Xscale + Xoffset;
				y = y * Yscale + Yoffset;
				// now x,y are in (unit)-Mercator coordinates; un-project.
				x *= 360.0;
				y = atan(sinh((y * 2.0 * M_PI) - M_PI)) * 180.0 / M_PI;
				printf(" ra%i=\"%g\" dec%i=\"%g\"", i, x, i, y);
			}
			printf("/>\n");
		}
	}
	
	printf("</quads>\n");
	free(radec);

	return 0;
}
