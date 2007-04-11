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
#include "starutil.h"
#include "an_catalog.h"
#include "mathutil.h"

#define OPTIONS "ho:l:m:t:d:bsSc" // T

void printHelp(char* progname) {
	fprintf(stderr, "%s usage:\n"
			"   -o <output-file>\n"
			"  [-m <max-y-val>]: Maximum y value of the projection (default: Pi)\n"
			"  [-l <n-leaf-point>]: Target number of points in the leaves of the tree (default 15).\n"
			"  [-t  <tree type>]:  {double,float,u32,u16}, default u32.\n"
			"  [-d  <data type>]:  {double,float,u32,u16}, default u32.\n"
			"   (   [-b]: build bounding boxes\n"
			"    OR [-s]: build splitting planes   )\n"
			"  [-S]: include separate splitdim array\n"
			"  [-c]: run kdtree_check on the resulting tree\n"
			"\n"
			"  <input-catalog> ...\n"
			"\n"
			"    (Input files are Astrometry.net catalogs.)\n"
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
	int exttype  = KDT_EXT_DOUBLE;
	int datatype = KDT_DATA_NULL;
	int treetype = KDT_TREE_NULL;
	int tt;
	int buildopts = 0;
	int checktree = 0;

	start = time(NULL);

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
		case 'c':
			checktree = 1;
			break;
		case 'o':
			outfn = optarg;
			break;
		case 'l':
			Nleaf = atoi(optarg);
			break;
		case 'm':
			maxy = atof(optarg);
			break;
		case 't':
			treetype = kdtree_kdtype_parse_tree_string(optarg);
			break;
		case 'd':
			datatype = kdtree_kdtype_parse_data_string(optarg);
			break;
		case 'b':
			buildopts |= KD_BUILD_BBOX;
			break;
		case 's':
			buildopts |= KD_BUILD_SPLIT;
			break;
		case 'S':
			buildopts |= KD_BUILD_SPLITDIM;
			break;
		}

	if (!outfn || (optind == argc)) {
		printHelp(progname);
		exit(-1);
	}

	if (!(buildopts & (KD_BUILD_BBOX | KD_BUILD_SPLIT))) {
		printf("You need bounding-boxes or splitting planes!\n");
		printHelp(progname);
		exit(-1);
	}

	// defaults
	if (!datatype)
		datatype = KDT_DATA_U32;
	if (!treetype)
		treetype = KDT_TREE_U32;

	tt = kdtree_kdtypes_to_treetype(exttype, treetype, datatype);

	mt = merctree_new();
	if (!mt) {
		fprintf(stderr, "Failed to allocate a merctree.\n");
		exit(-1);
	}

	// Read all the catalogs to see how many points there are in total.
	fprintf(stderr, "Reading catalogs to find total number of points...\n");
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
		fprintf(stderr, "  %i in %s\n", ancat->nentries, fn);
		N += ancat->nentries;
		an_catalog_close(ancat);
	}
	fprintf(stderr, "Total: %i points.\n", N);

	// Allocate space for star locations & flux.
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

		for (j=0; j<ancat->nentries; j++) {
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

			if (x < 0.0 || x >= 1.0) {
				fprintf(stderr, "x out of range: %g\n", x);
				continue;
			}
			if (y < 0.0 || y > 1.0) {
				// This is to be expected for points in the polar caps.
				//fprintf(stderr, "y out of range: %g\n", y);
				continue;
			}

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

				flux = mag2flux(ob->mag) * vertscale;
				if (red)
					mt->flux[i].rflux += flux;
				if (blue)
					mt->flux[i].bflux += flux;
				if (ir)
					mt->flux[i].nflux += flux;
			}

			i++;
		}
		an_catalog_close(ancat);
		fprintf(stderr, "\n");
	}

	// The total number of points kept:
	N = i;

	fprintf(stderr, "Kep: %i points within range.\n", N);

	fprintf(stderr, "Creating kdtree...\n");

	mt->tree = kdtree_new(N, 2, Nleaf);
	// Set the range...
	{
		double lo[] = { 0.0, 0.0 };
		double hi[] = { 1.0, 1.0 };
		kdtree_set_limits(mt->tree, lo, hi);
	}

	if (datatype != exttype) {
		fprintf(stderr, "Converting data...\n");
		fflush(stderr);
		mt->tree = kdtree_convert_data(mt->tree, xy, N, 2, Nleaf, tt);
		free(xy);
		fprintf(stderr, "Building tree...\n");
		fflush(stderr);
		mt->tree = kdtree_build(mt->tree, mt->tree->data.any, N, 2,
								Nleaf, tt, buildopts);
	} else {
		fprintf(stderr, "Building tree...\n");
		fflush(stderr);
		mt->tree = kdtree_build(NULL, xy, N, 2, Nleaf, tt, buildopts);
	}

	if (!mt->tree) {
		fprintf(stderr, "Failed to build kdtree.\n");
		exit(-1);
	}

	fprintf(stderr, "Built kdtree with %i nodes\n", mt->tree->nnodes);

	if (checktree) {
		fprintf(stderr, "Checking tree...\n");
		if (kdtree_check(mt->tree)) {
			fprintf(stderr, "\n\nTree check failed!!\n\n\n");
		}
	}

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

