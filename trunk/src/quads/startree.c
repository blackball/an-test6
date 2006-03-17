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

#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#include "kdtree/kdtree_fits_io.h"
#include "starutil.h"
#include "fileutil.h"
#include "catalog.h"

#define OPTIONS "hR:f:k:d:F"

void printHelp(char* progname) {
	printf("%s -f <input-catalog-name>\n"
		   "    [-R Nleaf]: number of points in a kdtree leaf node\n"
		   "    [-k keep]:  number of points to keep\n"
		   "    [-d radius]: deduplication radius (arcsec)\n"
		   "    [-F]: write FITS format output.\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar;
    int nkeep = 0;
    double duprad;
    kdtree_t* starkd = NULL;
    int levels;
    catalog* cat;
    int Nleaf = 25;
    bool fits = FALSE;
    char* fitstreefname = NULL;
    char* treefname = NULL;
    char* catfname = NULL;
	char* progname = argv[0];

    if (argc <= 2) {
		printHelp(progname);
        return 0;
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'F':
            fits = TRUE;
            break;
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
        case 'd':
            duprad = atof(optarg);
            if (duprad < 0.0) {
                printf("Couldn't parse \'radius\': \"%s\"\n", optarg);
                exit(-1);
            }
            break;
        case 'f':
            fitstreefname = mk_fits_streefn(optarg);
            treefname = mk_streefn(optarg);
            catfname = mk_catfn(optarg);
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

    fprintf(stderr, "%s: building KD tree for %s\n", argv[0], catfname);

	if (fits)
		fprintf(stderr, "Will write FITS format output to %s\n", fitstreefname);
	else
		fprintf(stderr, "Will write output to %s\n", treefname);

    fprintf(stderr, "Reading star catalogue...");
    cat = catalog_open_file(catfname, 1);
    free_fn(catfname);
    if (!cat) {
        fprintf(stderr, "Couldn't read catalogue.\n");
        exit(-1);
    }
    fprintf(stderr, "got %i stars.\n", cat->nstars);

    if (nkeep && (nkeep < cat->nstars)) {
        cat->nstars = nkeep;
        fprintf(stderr, "keeping at most %i stars.\n", nkeep);
    }

    fprintf(stderr, "  Building star KD tree...");
    fflush(stderr);

    levels = (int)((log((double)cat->nstars) - log((double)Nleaf))/log(2.0));
    if (levels < 1)
        levels = 1;
    fprintf(stderr, "Requesting %i levels.\n", levels);

    starkd = kdtree_build(catalog_get_base(cat), cat->nstars, DIM_STARS, levels);
    if (!starkd) {
        catalog_close(cat);
        fprintf(stderr, "Couldn't build kdtree.\n");
        exit(-1);
    }
    fprintf(stderr, "done (%d nodes)\n", starkd->nnodes);

    if (fits) {
		fprintf(stderr, "Writing FITS format output to %s\n", fitstreefname);
		fflush(stderr);
        if (kdtree_fits_write_file(starkd, fitstreefname)) {
            fprintf(stderr, "Failed to write FITS-format star kdtree.\n");
            exit(-1);
        }
    } else {
		fprintf(stderr, "Writing output to %s ...\n", treefname);
		fflush(stderr);
        if (kdtree_write_file(starkd, treefname)) {
            fprintf(stderr, "Failed to write star kdtree.\n");
            exit(-1);
        }
    }
    free_fn(treefname);
    free_fn(fitstreefname);
    fprintf(stderr, "done.\n");

    kdtree_free(starkd);
    catalog_close(cat);
    return 0;
}

