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

#define OPTIONS "s:S:q:"

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char fn[256];
	int i, j;

	char* sdss_file_template = "/p/learning/stars/SDSS_FIELDS/sdssfield%02i.rd.fits";

	int filenum = -1;
	int fieldnum = -1;
	rdlist* rdls;
	double* radec;
	int Nstars;
	il* quads = il_new(16);

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
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
		
	sprintf(fn, sdss_file_template, filenum);
	rdls = rdlist_open(fn);
	if (!rdls) {
		fprintf(stderr, "Failed to open RDLS file.\n");
		exit(-1);
	}
	Nstars = rdlist_n_entries(rdls, fieldnum);
	if (Nstars == -1) {
		fprintf(stderr, "Failed to read RDLS file.\n");
		exit(-1);
	}
	radec = malloc(Nstars * 2 * sizeof(double));
	if (rdlist_read_entries(rdls, fieldnum, 0, Nstars, radec)) {
		fprintf(stderr, "Failed to read RDLS file.\n");
		free(radec);
		exit(-1);
	}

	fprintf(stderr, "Got %i stars.\n", Nstars);

	printf("<quads>\n");
	for (j=0; j<il_size(quads)/4; j++) {
		printf("  <quad");
		for (i=0; i<4; i++) {
			int ind = il_get(quads, j*4 + i);
			if (ind < 0 || ind >= Nstars)
				break;
			printf(" ra%i=\"%g\" dec%i=\"%g\"", i, radec[2*ind], i, radec[2*ind+1]);
		}
		printf("/>\n");
	}
	printf("</quads>\n");

	free(radec);

	return 0;
}
