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
#include "healpix.h"

#define OPTIONS "ho:l:m:t:d:bsScM:N:i:H:"

void printHelp(char* progname) {
	fprintf(stderr, "%s usage:\n"
			"   -o <output-file>\n"
			"   -M <Mercator grid size>\n"
			"   -N <Mercator grid number>\n"
			"   -i <input-catalog-pattern>\n"
			"   -H <Nside of AN catalog healpixelization>\n"
			"  [-m <max-y-val>]: Maximum y value of the projection (default: Pi)\n"
			"  [-l <n-leaf-point>]: Target number of points in the leaves of the tree (default 15).\n"
			"  [-t  <tree type>]:  {double,float,u32,u16}, default u32.\n"
			"  [-d  <data type>]:  {double,float,u32,u16}, default u32.\n"
			"   (   [-b]: build bounding boxes\n"
			"    OR [-s]: build splitting planes   )\n"
			"  [-S]: include separate splitdim array\n"
			"  [-c]: run kdtree_check on the resulting tree\n"
			"\n"
			"    (Input files are Astrometry.net catalogs.)\n"
			"\n", progname);
}

// dec in radians
// returns [0,1] for mercator [-pi,pi]
static double dec2merc(double dec) {
	return (asinh(tan(dec)) + M_PI) / (2.0 * M_PI);
}

// ra in radians.
static double ra2merc(double ra) {
	return ra / (2.0 * M_PI);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int argchar;
	char* progname = args[0];
	char* outfn = NULL;
	char* infn = NULL;
	double maxy = M_PI;
	int i;
	int Nleaf = 15;
	time_t start;
	merctree* mt;
	int N;
	double* xy;
	int exttype  = KDT_EXT_DOUBLE;
	int datatype = KDT_DATA_NULL;
	int treetype = KDT_TREE_NULL;
	int tt;
	int buildopts = 0;
	int checktree = 0;
	double xlo, xhi, ylo, yhi;
	int Nside = 0;

	int Nmerc = -1;
	int mercgrid = -1;
	int HP;
	il* hps;

	start = time(NULL);

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
		case 'i':
			infn = optarg;
			break;
		case 'H':
			Nside = atoi(optarg);
			break;
		case 'M':
			Nmerc = atoi(optarg);
			break;
		case 'N':
			mercgrid = atoi(optarg);
			break;
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

	if (!outfn || (optind != argc)) {
		printHelp(progname);
		exit(-1);
	}

	if (!(buildopts & (KD_BUILD_BBOX | KD_BUILD_SPLIT))) {
		printf("You need bounding-boxes or splitting planes!\n");
 		printHelp(progname);
		exit(-1);
	}

	if ((Nmerc == -1) || (mercgrid == -1)) {
		printf("You must specify -N and -M!\n");
 		printHelp(progname);
		exit(-1);
	}

	if (Nside<1 || !infn) {
		printf("You must specify -H and -i!\n");
 		printHelp(progname);
		exit(-1);
	}

	// defaults
	if (!datatype)
		datatype = KDT_DATA_U32;
	if (!treetype)
		treetype = KDT_TREE_U32;

	tt = kdtree_kdtypes_to_treetype(exttype, treetype, datatype);

	// ranges in Mercator space.
	{
		int xgrid = mercgrid % Nmerc;
		int ygrid = mercgrid / Nmerc;
		double ymax = (maxy + M_PI) / (2.0 * M_PI);
		double ymin = (-maxy + M_PI) / (2.0 * M_PI);
		xlo = xgrid / (double)Nmerc;
		xhi = (xgrid+1) / (double)Nmerc;
		ylo = ymin + (ymax - ymin) * ygrid / (double)Nmerc;
		yhi = ymin + (ymax - ymin) * (ygrid+1) / (double)Nmerc;
	}
	printf("Mercator ranges: x [%g,%g], y [%g,%g]\n", xlo, xhi, ylo, yhi);

	mt = merctree_new();
	if (!mt) {
		fprintf(stderr, "Failed to allocate a merctree.\n");
		exit(-1);
	}

	// Go through the healpixes and find the ones that might overlap
	// with this rectangle in Mercator space.
	HP = 12 * Nside * Nside;
	hps = il_new(256);

	for (i=0; i<HP; i++) {
		double ra1, ra2;
		double x1, x2;
		double dec1, dec2;
		double y1, y2;
		double nil;

		// leftmost point is (0,1); rightmost is (1,0).
		// southmost is (0,0); northmost is (1,1).
		healpix_to_radec(i, Nside, 0.0, 1.0, &ra1, &nil);
		healpix_to_radec(i, Nside, 1.0, 0.0, &ra2, &nil);
		x1 = ra2merc(ra1);
		x2 = ra2merc(ra2);

		if (x2 > x1) {
			if ((x1 > xhi) || (x2 < xlo))
				continue;
		} else {
			// wrap-around
			if ((x1 > xhi) && (x2 < xlo))
				continue;
		}

		healpix_to_radec(i, Nside, 0.0, 0.0, &nil, &dec1);
		healpix_to_radec(i, Nside, 1.0, 1.0, &nil, &dec2);
		y1 = dec2merc(dec1);
		y2 = dec2merc(dec2);

		if ((y1 > yhi) || (y2 < ylo))
			continue;

		il_append(hps, i);
	}

	// Count the total number of points in the overlapping healpixes.
	N = 0;
	for (i=0; i<il_size(hps); i++) {
		char fn[1024];
		int hp;
		an_catalog* cat;

		hp = il_get(hps, i);
		sprintf(fn, infn, hp);
		cat = an_catalog_open(fn);
		if (!cat) {
			fprintf(stderr, "Failed to open catalog %s.\n", fn);
			exit(-1);
		}
		fprintf(stderr, "  %i points in %s\n", cat->nentries, fn);
		N += cat->nentries;
		an_catalog_close(cat);
	}
	fprintf(stderr, "Total: %i points.\n", N);

	// Allocate space for star locations & flux.
	xy = malloc(N * 2 * sizeof(double));
	mt->flux = calloc(N, sizeof(merc_flux));

	if (!xy || !mt->flux) {
		fprintf(stderr, "Failed to allocate arrays for merctree positions and flux.\n");
		exit(-1);
	}

	N = 0;
	for (i=0; i<il_size(hps); i++) {
		char fn[1024];
		int hp;
		an_catalog* cat;
		int j;

		hp = il_get(hps, i);
		sprintf(fn, infn, hp);
		cat = an_catalog_open(fn);
		if (!cat) {
			fprintf(stderr, "Failed to open catalog %s.\n", fn);
			exit(-1);
		}

		for (j=0; j<cat->nentries; j++) {
			float vertscale;
			an_entry* entry;
			double x, y;
			int o;

			entry = an_catalog_read_entry(cat);
			if (!entry)
				break;

			x =  ra2merc(deg2rad(entry->ra ));
			if (x < xlo || x > xhi)
				continue;
			y = dec2merc(deg2rad(entry->dec));
			if (y < ylo || y > yhi)
				continue;
			vertscale = 1.0 / cos(deg2rad(entry->dec));

			xy[N*2] = x;
			xy[N*2+1] = y;

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
					mt->flux[N].rflux += flux;
				if (blue)
					mt->flux[N].bflux += flux;
				if (ir)
					mt->flux[N].nflux += flux;
			}

			N++;
		}

		an_catalog_close(cat);
	}
	fprintf(stderr, "Total: %i points overlap.\n", N);

	il_free(hps);

	xy = realloc(xy, N * 2 * sizeof(double));
	mt->flux = realloc(mt->flux, N * sizeof(merc_flux));

	fprintf(stderr, "Creating kdtree...\n");

	mt->tree = kdtree_new(N, 2, Nleaf);

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

