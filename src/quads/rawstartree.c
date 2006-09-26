#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "fileutil.h"
#include "starutil.h"
#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "an_catalog.h"
#include "mathutil.h"

#define OPTIONS "ho:l:m:"

/* make a raw startree from a catalog; no cutting */

void printHelp(char* progname)
{
	fprintf(stderr, "%s usage:\n"
	        "   -o <output-file>\n"
	        "  [-l <n-leaf-point>]: Target number of points in the leaves of the tree (default 15).\n"
	        "\n"
	        "  <input-catalog> ...\n"
	        "\n"
	        "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args)
{
	int argchar;
	char* progname = args[0];
	char* outfn = NULL;
	int i;
	int Nleaf = 15;
	time_t start;
	int N;
	int infile;
	double* xyz;
	kdtree_t* kd;

	start = time(NULL);

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
		case 'o':
			outfn = optarg;
			break;
		case 'l':
			Nleaf = atoi(optarg);
			break;
		}

	if (!outfn || (optind == argc)) {
		printHelp(progname);
		exit( -1);
	}

	N = 0;
	for (infile = optind; infile < argc; infile++) {
		char* fn;
		an_catalog* ancat;
		fn = args[infile];
		ancat = an_catalog_open(fn);
		if (!ancat) {
			fprintf(stderr, "Failed to open Astrometry.net catalog %s\n", fn);
			exit( -1);
		}
		N += ancat->nentries;
		an_catalog_close(ancat);
	}

	xyz = malloc(N * 3 * sizeof(double));

	if (!xyz) {
		fprintf(stderr, "Failed to allocate arrays for star positions.\n");
		exit( -1);
	}

	i = 0;
	for (infile = optind; infile < argc; infile++) {
		char* fn;
		int j;
		an_catalog* ancat;
		int lastgrass = 0;

		fn = args[infile];
		ancat = an_catalog_open(fn);
		if (!ancat) {
			fprintf(stderr, "Failed to open Astrometry.net catalog %s\n", fn);
			exit( -1);
		}

		fprintf(stderr, "Reading %i entries for catalog file %s.\n", ancat->nentries, fn);

		for (j = 0; j < ancat->nentries; j++, i++) {
			int grass;
			an_entry* entry;

			entry = an_catalog_read_entry(ancat);
			if (!entry)
				break;

			grass = (j * 80 / ancat->nentries);
			if (grass != lastgrass) {
				fprintf(stderr, ".");
				fflush(stderr);
				lastgrass = grass;
			}

			radec2xyzarr(deg2rad(entry->ra), deg2rad(entry->dec), xyz+3*i);
		}
		an_catalog_close(ancat);
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Creating kdtree...\n");
	{
		int levels;
		levels = kdtree_compute_levels(N, Nleaf);
		kd = kdtree_build(xyz, N, 3, levels + 1);
		if (!kd) {
			fprintf(stderr, "Failed to build kdtree.\n");
			exit( -1);
		}

		fprintf(stderr, "Built kdtree with %i levels, %i nodes\n",
		        levels, kd->nnodes);
	}

	fprintf(stderr, "Writing output to file %s...\n", outfn);
	if (kdtree_fits_write_file(kd, outfn, NULL)) { // FIXME add extra headers?
		fprintf(stderr, "Failed to write kdtree to file %s.\n", outfn);
		exit( -1);
	}

	kdtree_free(kd);

	return 0;
}

