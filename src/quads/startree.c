/**
   Builds a star kdtree from a list of stars.

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
#include "starkd.h"
#include "boilerplate.h"

#define OPTIONS "hR:f:k:d:"

void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s\n"
		   "     -f <input-catalog-name>\n"
		   "    [-R Nleaf]: number of points in a kdtree leaf node (default 25)\n"
		   "    [-k keep]:  number of points to keep\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar;
    int nkeep = 0;
	startree* starkd;
    int levels;
    catalog* cat;
    int Nleaf = 25;
    char* basename = NULL;
    char* treefname = NULL;
    char* catfname = NULL;
	char* progname = argv[0];
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

    levels = kdtree_compute_levels(cat->numstars, Nleaf);
    fprintf(stderr, "Requesting %i levels.\n", levels);

	starkd = startree_new();
	if (!starkd) {
		fprintf(stderr, "Failed to allocate startree.\n");
		exit(-1);
	}
    starkd->tree = kdtree_build(catalog_get_base(cat), cat->numstars, DIM_STARS, levels);
    if (!starkd->tree) {
        catalog_close(cat);
        fprintf(stderr, "Couldn't build kdtree.\n");
        exit(-1);
    }
    fprintf(stderr, "done (%d nodes)\n", startree_N(starkd));

	fprintf(stderr, "Writing output to %s ...\n", treefname);
	fflush(stderr);
	sprintf(val, "%u", Nleaf);
	qfits_header_add(startree_header(starkd), "NLEAF", val, "Target number of points in leaves.", NULL);
	sprintf(val, "%u", levels);
	qfits_header_add(startree_header(starkd), "LEVELS", val, "Number of kdtree levels.", NULL);
	sprintf(val, "%u", nkeep);
	qfits_header_add(startree_header(starkd), "KEEP", val, "Number of stars kept.", NULL);

	fits_copy_header(cat->header, startree_header(starkd), "HEALPIX");

	boilerplate_add_fits_headers(startree_header(starkd));
	qfits_header_add(startree_header(starkd), "HISTORY", "This file was created by the program \"startree\".", NULL, NULL);
	qfits_header_add(startree_header(starkd), "HISTORY", "startree command line:", NULL, NULL);
	fits_add_args(startree_header(starkd), argv, argc);
	qfits_header_add(startree_header(starkd), "HISTORY", "(end of startree command line)", NULL, NULL);

	qfits_header_add(startree_header(starkd), "HISTORY", "** History entries copied from the input file:", NULL, NULL);
	fits_copy_all_headers(cat->header, startree_header(starkd), "HISTORY");
	qfits_header_add(startree_header(starkd), "HISTORY", "** End of history entries.", NULL, NULL);

	if (startree_write_to_file(starkd, treefname)) {
		fprintf(stderr, "Failed to write star kdtree.\n");
		exit(-1);
	}
    free_fn(treefname);
    fprintf(stderr, "done.\n");

    startree_close(starkd);
    catalog_close(cat);
    return 0;
}

