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

#include "starkd.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_macros.h"
#include "kdtree/kdtree_access.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "bl.h"

#define OPTIONS "q:H:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* skdt_template = "/p/learning/stars/INDEXES/sdss-18/sdss-18_%02i.skdt.fits";
	char fn[256];
	int i, j;
	int healpix = -1;
	startree* skdt;
	il* quads = il_new(16);

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'H':
			healpix = atoi(optarg);
			break;
		case 'q':
			il_append(quads, atoi(optarg));
			break;
		}

	if (healpix < 0 || healpix >= 12  || !il_size(quads) || (il_size(quads) % 4)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "Healpix %i\n", healpix);
	fprintf(stderr, "Quads: ");
	for (i=0; i<il_size(quads); i++)
		fprintf(stderr, "%i ", il_get(quads, i));
	fprintf(stderr, "\n");

	sprintf(fn, skdt_template, healpix);
	fprintf(stderr, "Opening file %s...\n", fn);
	skdt = startree_open(fn);
	if (!skdt) {
		fprintf(stderr, "Failed to open startree for healpix %i.\n", healpix);
		exit(-1);
	}

	printf("<quads>\n");
	for (j=0; j<il_size(quads)/4; j++) {
		printf("  <quad");
		for (i=0; i<4; i++) {
			double* xyz;
			double ra, dec;
			int ind = il_get(quads, j*4 + i);
			if (ind < 0 || ind >= skdt->tree->ndata)
				break;
			xyz = skdt->tree->data + ind * 3;
			xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);
			ra = rad2deg(ra);
			dec = rad2deg(dec);
			printf(" ra%i=\"%g\" dec%i=\"%g\"", i, ra, i, dec);
		}
		printf("/>\n");
	}
	printf("</quads>\n");

	startree_close(skdt);
	return 0;
}

