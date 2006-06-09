/**
   \file  A replacement for \c startree using Keir's new
   kdtree.

   Input: .objs
   Output: .skdt
*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "starutil.h"
#include "fileutil.h"
#include "catalog.h"
#include "fitsioutils.h"

#define OPTIONS "hR:f:k:d:"

void printHelp(char* progname) {
	printf("%s -f <input-catalog-name>\n"
		   "    [-R Nleaf]: number of points in a kdtree leaf node\n"
		   "    [-k keep]:  number of points to keep\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar;
    int nkeep = 0;
    kdtree_t* starkd = NULL;
    int levels;
    catalog* cat;
    int Nleaf = 25;
    char* basename = NULL;
    char* treefname = NULL;
    char* catfname = NULL;
	char* progname = argv[0];
	qfits_header* hdr;
	char val[32];

    if (argc <= 2) {
		printHelp(progname);
        return 0;
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'R':
            Nleaf = (int)strtoul(optarg, NULL, 0);
            break;
        case 'k':
            nkeep = atoi(optarg);
            if (nkeep == 0) {
                printf("Couldn't parse \'keep\': \"%s\"\n", optarg);
                exit(-1);
            }
            break;
        case 'f':
            basename = optarg;
            break;
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        case 'h':
			printHelp(progname);
            return 0;
        default:
            return -1;
        }

    if (optind < argc) {
        for (argidx = optind; argidx < argc; argidx++)
            fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		printHelp(progname);
        return 0;
    }

    if (!basename) {
        printHelp(progname);
        exit(-1);
    }

    catfname = mk_catfn(basename);
	treefname = mk_streefn(basename);
    fprintf(stderr, "%s: building KD tree for %s\n", argv[0], catfname);
	fprintf(stderr, "Will write output to %s\n", treefname);

    fprintf(stderr, "Reading star catalogue...");
    cat = catalog_open(catfname, 1);
    free_fn(catfname);
    if (!cat) {
        fprintf(stderr, "Couldn't read catalogue.\n");
        exit(-1);
    }
    fprintf(stderr, "got %i stars.\n", cat->numstars);

    if (nkeep && (nkeep < cat->numstars)) {
        cat->numstars = nkeep;
        fprintf(stderr, "keeping at most %i stars.\n", nkeep);
    }

    fprintf(stderr, "  Building star KD tree...");
    fflush(stderr);

    levels = (int)((log((double)cat->numstars) - log((double)Nleaf))/log(2.0));
    if (levels < 1)
        levels = 1;
    fprintf(stderr, "Requesting %i levels.\n", levels);

    starkd = kdtree_build(catalog_get_base(cat), cat->numstars, DIM_STARS, levels);
    if (!starkd) {
        catalog_close(cat);
        fprintf(stderr, "Couldn't build kdtree.\n");
        exit(-1);
    }
    fprintf(stderr, "done (%d nodes)\n", starkd->nnodes);

	fprintf(stderr, "Writing output to %s ...\n", treefname);
	fflush(stderr);
	hdr = qfits_header_new();
	qfits_header_add(hdr, "AN_FILE", "SKDT", "This file is a star kdtree.", NULL);
	sprintf(val, "%u", Nleaf);
	qfits_header_add(hdr, "NLEAF", val, "Target number of points in leaves.", NULL);
	sprintf(val, "%u", levels);
	qfits_header_add(hdr, "LEVELS", val, "Number of kdtree levels.", NULL);
	sprintf(val, "%u", nkeep);
	qfits_header_add(hdr, "KEEP", val, "Number of stars kept.", NULL);

	fits_copy_header(hdr, hdr, "HEALPIX");
	qfits_header_add(hdr, "COMMENT", NULL, "startree command line:", NULL);
	fits_add_args(hdr, argv, argc);

	if (kdtree_fits_write_file(starkd, treefname, hdr)) {
		fprintf(stderr, "Failed to write star kdtree.\n");
		exit(-1);
	}
	qfits_header_destroy(hdr);
    free_fn(treefname);
    fprintf(stderr, "done.\n");

    kdtree_free(starkd);
    catalog_close(cat);
    return 0;
}

