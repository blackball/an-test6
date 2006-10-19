/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "merctree.h"
#include "fileutil.h"
#include "starutil.h"
#include "an_catalog.h"
#include "mathutil.h"

#define OPTIONS "ho:l:m:"

void printHelp(char* progname) {
	fprintf(stderr, "%s usage:\n"
			"   -o <output-file>\n"
			"  [-m <max-y-val>]: Maximum y value of the projection (default: Pi)\n"
			"  [-l <n-leaf-point>]: Target number of points in the leaves of the tree (default 15).\n"
			"\n"
			"  <input-catalog> ...\n"
			"\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int argchar;
	char* progname = args[0];
	char* outfn = NULL;
	float maxy = M_PI;
	int i;
	int Nleaf = 15;
	time_t start;
	merctree* mt;
	int N;
	int infile;
	double* xy;

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
		case 'm':
			maxy = atof(optarg);
			break;
		}

	if (!outfn || (optind == argc)) {
		printHelp(progname);
		exit(-1);
	}

	mt = merctree_new();
	if (!mt) {
		fprintf(stderr, "Failed to allocate a merctree.\n");
		exit(-1);
	}

	N = 0;
	for (infile = optind; infile<argc; infile++) {
		char* fn;
		an_catalog* ancat;
		fn = args[infile];
		ancat = an_catalog_open(fn);
		if (!ancat) {
			fprintf(stderr, "Failed to open Astrometry.net catalog %s\n", fn);
			exit(-1);
		}
		N += ancat->nentries;
		an_catalog_close(ancat);
	}

	xy = malloc(N * 2 * sizeof(double));
	mt->flux = calloc(N, sizeof(merc_flux));

	if (!xy || !mt->flux) {
		fprintf(stderr, "Failed to allocate arrays for merctree positions and flux.\n");
		exit(-1);
	}

	i=0;
	for (infile = optind; infile<argc; infile++) {
		char* fn;
		int j;
		an_catalog* ancat;
		int lastgrass = 0;

		fn = args[infile];
		ancat = an_catalog_open(fn);
		if (!ancat) {
			fprintf(stderr, "Failed to open Astrometry.net catalog %s\n", fn);
			exit(-1);
		}

		fprintf(stderr, "Reading %i entries for catalog file %s.\n", ancat->nentries, fn);

		for (j=0; j<ancat->nentries; j++, i++) {
			int grass;
			float vertscale;
			an_entry* entry;
			double x, y;
			int o;

			entry = an_catalog_read_entry(ancat);
			if (!entry)
				break;

			grass = (j * 80 / ancat->nentries);
			if (grass != lastgrass) {
				fprintf(stderr, ".");
				fflush(stderr);
				lastgrass = grass;
			}

			x = (entry->ra / 360.0);
			y = (maxy + asinh(tan(deg2rad(entry->dec)))) / (2.0 * maxy);
			vertscale = 1.0 / cos(deg2rad(entry->dec));

			xy[i*2] = x;
			xy[i*2+1] = y;

			for (o=0; o<entry->nobs; o++) {
				bool red = FALSE, blue = FALSE, ir = FALSE;
				float flux;
				an_observation* ob = entry->obs + o;
				switch (ob->catalog) {
				case AN_SOURCE_USNOB:
					switch (ob->band) {
					case 'J':
					case 'O':
						blue = TRUE;
						break;
					case 'E':
					case 'F':
						red = TRUE;
						break;
					case 'N':
						ir = TRUE;
						break;
					}
					break;
				case AN_SOURCE_TYCHO2:
					switch (ob->band) {
					case 'B':
						blue = TRUE;
						break;
					case 'V':
						red = TRUE;
						break;
					case 'H':
						blue = TRUE;
						red = TRUE;
						break;
					}
					break;
				}

				flux = exp(-ob->mag) * vertscale;
				if (red)
					mt->flux[i].rflux += flux;
				if (blue)
					mt->flux[i].bflux += flux;
				if (ir)
					mt->flux[i].nflux += flux;
			}
		}
		an_catalog_close(ancat);
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Creating kdtree...\n");

	mt->tree = kdtree_build(NULL, xy, N, 2, Nleaf, KDTT_DOUBLE,
							KD_BUILD_BBOX | KD_BUILD_SPLIT | KD_BUILD_SPLITDIM);
	if (!mt->tree) {
		fprintf(stderr, "Failed to build kdtree.\n");
		exit(-1);
	}

	fprintf(stderr, "Built kdtree with %i nodes\n", mt->tree->nnodes);


	// permute the fluxes to match the kdtree.
	fprintf(stderr, "Permuting fluxes...\n");
	{
		merc_flux* tmpflux = malloc(N * sizeof(merc_flux));
		if (!tmpflux) {
			fprintf(stderr, "Failed to allocate temp merctree flux.\n");
			exit(-1);
		}
		for (i=0; i<N; i++) {
			tmpflux[i] = mt->flux[mt->tree->perm[i]];
		}
		free(mt->flux);
		mt->flux = tmpflux;
		free(mt->tree->perm);
		mt->tree->perm = NULL;
	}

	fprintf(stderr, "Computing cached statistics...\n");
	mt->stats = malloc(mt->tree->nnodes * sizeof(merc_stats));
	if (!mt->stats) {
		fprintf(stderr, "Failed to allocate merctree stats.\n");
		exit(-1);
	}
	merctree_compute_stats(mt);

	fprintf(stderr, "Writing output to file %s...\n", outfn);
	if (merctree_write_to_file(mt, outfn)) {
		fprintf(stderr, "Failed to write merctree to file %s.\n", outfn);
		exit(-1);
	}

	free(mt->flux);
	free(mt->stats);

	merctree_close(mt);

	return 0;
}

