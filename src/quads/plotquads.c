#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "kdtree_io.h"
#include "qidxfile.h"
#include "quadfile.h"
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "intmap.h"

static const char* OPTIONS = "hf:R:D:r:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   -f <index-basename>\n"
			"   -R <ra> -D <dec> -r <radius-in-arcmin>\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	kdtree_t* startree;
	char* indexfname = NULL;
	char* fn;
	kdtree_qres_t* res;
	double ra;
	double dec;
	bool ra_set = FALSE;
	bool dec_set = FALSE;
	double radius = 0.0;
	double radius2;
	double xyz[3];
	qidxfile* qidx;
	quadfile* qf;
	il* quadlist;
	il* goodquads;
	int i, j;
	intmap* indmap;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'R':
			ra = atof(optarg);
			ra_set = TRUE;
			break;
		case 'D':
			dec = atof(optarg);
			dec_set = TRUE;
			break;
		case 'r':
			radius = atof(optarg);
			break;
		case 'f':
			indexfname = optarg;
			break;
		default:
		case 'h':
			printHelp(progname);
			exit(0);
		}
	}

	if (!indexfname || !ra_set || !dec_set || radius == 0.0) {
		printHelp(progname);
		exit(-1);
	}

	ra  = deg2rad(ra);
	dec = deg2rad(dec);
	xyz[0] = radec2x(ra, dec);
	xyz[1] = radec2y(ra, dec);
	xyz[2] = radec2z(ra, dec);
	radius2 = arcsec2distsq(radius * 60.0);
	
	fn = mk_streefn(indexfname);
	fprintf(stderr, "Reading star kdtree from %s ...\n", fn);
	startree = kdtree_fits_read_file(fn);
	if (!startree) {
		fprintf(stderr, "Failed to open star kdtree from file %s .\n", fn);
		exit(-1);
	}
	free_fn(fn);

	fn = mk_qidxfn(indexfname);
	fprintf(stderr, "Reading qidxfile %s...\n", fn);
	qidx = qidxfile_open(fn, 0);
	if (!qidx) {
		fprintf(stderr, "Failed to open qidx file.\n");
		exit(-1);
	}
	free_fn(fn);

	fn = mk_quadfn(indexfname);
	fprintf(stderr, "Reading quadfile %s...\n", fn);
	qf = quadfile_open(fn, 0);
	if (!qf) {
		fprintf(stderr, "Failed to open quad file.\n");
		exit(-1);
	}
	free_fn(fn);

	res = kdtree_rangesearch(startree, xyz, radius2);
	fprintf(stderr, "Found %i stars within range.\n", res->nres);

	quadlist = il_new(32);
	indmap = intmap_new(INTMAP_ONE_TO_ONE);

	printf("starxy=[");
	for (i=0; i<res->nres; i++) {
		double x, y;
		double* starpos;
		uint nquads, *quads;
		uint star = res->inds[i];
		if (qidxfile_get_quads(qidx, star, &quads, &nquads)) {
			fprintf(stderr, "Failed to get quads for star %u.\n", star);
			exit(-1);
		}
		for (j=0; j<nquads; j++)
			il_insert_ascending(quadlist, quads[j]);
		// project the star
		starpos = res->results + i*3;
		star_coords(starpos, xyz, &x, &y);
		printf("%g,%g;", x, y);
		// record the mapping from star id to index in the starxy array.
		intmap_add(indmap, star, i+1);
	}
	printf("];\n");

	fprintf(stderr, "Found %i quads involving stars in this field.\n",
			il_size(quadlist));

	goodquads = il_new(32);

	// find quads that are composed of 4 stars in this field.
	for (i=0; i<il_size(quadlist); i++) {
		int quad = il_get(quadlist, i);
		for (j=0; (j<4) && (i+j)<il_size(quadlist); j++)
			if (il_get(quadlist, i+j) != quad)
				break;
		if (j == 4)
			il_append(goodquads, quad);
	}
	il_free(quadlist);

	fprintf(stderr, "Found %i quads involving only stars in this field.\n",
			il_size(goodquads));

	// draw lines to indicate quads.
	printf("quadlinesx=zeros(6,%i);\n", il_size(goodquads));
	printf("quadlinesy=zeros(6,%i);\n", il_size(goodquads));
	for (i=0; i<il_size(goodquads); i++) {
		uint sa, sb, sc, sd;
		int quad = il_get(goodquads, i);
		int ia, ib, ic, id;
		quadfile_get_starids(qf, quad, &sa, &sb, &sc, &sd);
		ia = intmap_get(indmap, sa, -1);
		ib = intmap_get(indmap, sb, -1);
		ic = intmap_get(indmap, sc, -1);
		id = intmap_get(indmap, sd, -1);
		if ((ia == -1) || (ib == -1) || (ic == -1) || (id == -1)) {
			fprintf(stderr, "Failed to find index for stars %u, %u, %u, %u.\n", sa, sb, sc, sd);
			exit(-1);
		}
		printf("quadlinesx(:,%i)=[starxy(%i,1);starxy(%i,1);starxy(%i,1);starxy(%i,1);starxy(%i,1);starxy(%i,1);];\n",
			   i+1, ia, ic, ib, id, ia, ib);
		printf("quadlinesy(:,%i)=[starxy(%i,2);starxy(%i,2);starxy(%i,2);starxy(%i,2);starxy(%i,2);starxy(%i,2);];\n",
			   i+1, ia, ic, ib, id, ia, ib);
	}

	il_free(goodquads);

	intmap_free(indmap);

	qidxfile_close(qidx);
	quadfile_close(qf);
	kdtree_close(startree);

	return 0;
}
