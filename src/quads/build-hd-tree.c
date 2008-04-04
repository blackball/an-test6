/*
  This file is part of the Astrometry.net suite.
  Copyright 2008 Dustin Lang.

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

/**
 Builds a kdtree from the Henry Draper catalog.

 I downloaded the text file from:
   http://cdsarc.u-strasbg.fr/viz-bin/VizieR?-meta.foot&-source=III/135A
 with the options:
 - "|-separated values",
 - only the "HD" column selected

 The result is a header including this:
#Column _RAJ2000        (F6.2)  Right ascension (FK5) Equinox=J2000.0 Epoch=J1900. (computed by VizieR, not part of the original data)  [ucd=pos.eq.ra;meta.main]
#Column _DEJ2000        (F6.2)  Declination (FK5) Equinox=J2000.0 Epoch=J1900. (computed by VizieR, not part of the original data)      [ucd=pos.eq.dec;meta.main]
#Column HD      (I6)    [1/272150]+ Henry Draper Catalog (HD) number    [ucd=meta.id;meta.main]
_RAJ2000|_DEJ2000|HD
deg|deg|
------|------|------

 And then data like this:

001.30|+67.84|     1
001.29|+57.77|     2
001.29|+45.22|     3
001.28|+30.32|     4
001.28| +2.37|     5

 It turns out that the HD numbers are all in sequence and contiguous, so there's no
 point storing them.

 A copy of this input file is available at 
 http://trac.astrometry.net/browser/binary/henry-draper/henry-draper.tsv
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
#include "errors.h"
#include "bl.h"
#include "tycho2.h"
#include "tycho2-fits.h"

static const char* OPTIONS = "hR:d:t:bsST:";

#define HD_NENTRIES 272150

void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s\n"
		   "   (   [-b]: build bounding boxes\n"
		   "    OR [-s]: build splitting planes   )\n"
		   "    [-R Nleaf]: number of points in a kdtree leaf node (default 25)\n"
		   "    [-t  <tree type>]:  {double,float,u32,u16}, default u32.\n"
		   "    [-d  <data type>]:  {double,float,u32,u16}, default u32.\n"
		   "    [-S]: include separate splitdim array\n"
           "    [-T <Tycho-2 catalog>]: cross-reference positions with Tycho-2.\n"
           "\n"
           "   <input.tsv>  <output.fits>\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
    kdtree_t* kd;
    int Nleaf = 25;
    char* infn = NULL;
    char* outfn = NULL;
    char* tychofn = NULL;
	char* progname = args[0];
    FILE* f;

    kdtree_t* tyckd = NULL;

	int exttype  = KDT_EXT_DOUBLE;
	int datatype = KDT_DATA_NULL;
	int treetype = KDT_TREE_NULL;
	bool convert = FALSE;
	int tt;
	int buildopts = 0;
	int i, N, D;

    dl* ras;
    dl* decs;
    dl* hds;
    int nbad = 0;

    int* hd;
    double* xyz;
    double r2 = 0;

    qfits_header* hdr;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
        case 'T':
            tychofn = optarg;
            break;
        case 'R':
            Nleaf = (int)strtoul(optarg, NULL, 0);
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
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        case 'h':
			printHelp(progname);
            return 0;
        default:
            return -1;
        }

    if (optind != argc - 2) {
        printHelp(progname);
        exit(-1);
    }

    infn = args[optind];
    outfn = args[optind+1];

	if (!(buildopts & (KD_BUILD_BBOX | KD_BUILD_SPLIT))) {
		printf("You need bounding-boxes or splitting planes!\n");
		printHelp(progname);
		exit(-1);
	}

    if (tychofn) {
        int i, N;
        double* xyz;
        tycho2_fits* tyc = tycho2_fits_open(tychofn);
        if (!tyc) {
            ERROR("Failed to open Tycho-2 catalog.");
            exit(-1);
        }
        printf("Reading Tycho-2 catalog...\n");
        N = tycho2_fits_count_entries(tyc);
        xyz = malloc(N * 3 * sizeof(double));
        for (i=0; i<N; i++) {
            tycho2_entry* te = tycho2_fits_read_entry(tyc);
            radecdeg2xyzarr(te->ra, te->dec, xyz + i*3);
        }
        tycho2_fits_close(tyc);

        printf("Building kdtree from Tycho-2...\n");
        tyckd = kdtree_build(kd, xyz, N, 3, 10, KDTT_DOUBLE, KD_BUILD_SPLIT);
        if (!tyckd) {
            ERROR("Failed to build a kdtree from Tycho-2 catalog.");
            exit(-1);
        }
    }

	// defaults
	if (!datatype)
		datatype = KDT_DATA_U32;
	if (!treetype)
		treetype = KDT_TREE_U32;
	// the outside world works in doubles.
	if (datatype != KDT_DATA_DOUBLE)
		convert = TRUE;

    f = fopen(infn, "rb");
    if (!f) {
        SYSERROR("Failed to open input file %s", infn);
        exit(-1);
    }

    ras = dl_new(1024);
    decs = dl_new(1024);
    hds = il_new(1024);

    for (;;) {
        char buf[1024];
        double ra, dec;
        int hd;

        if (!fgets(buf, sizeof(buf), f)) {
            if (ferror(f)) {
                SYSERROR("Failed to read a line of text from the input file");
                exit(-1);
            }
            break;
        }

        if (buf[0] == '#')
            continue;
        if (buf[0] == '\n')
            continue;

        if (sscanf(buf, " %lf| %lf| %d", &ra, &dec, &hd) < 3) {
            // ignore three invalid lines
            if (nbad > 3) {
                ERROR("Failed to parse line: \"%s\"", buf);
            }
            nbad++;
        } else {
            dl_append(ras, ra);
            dl_append(decs, dec);
            il_append(hds, hd);
        }
    }
    fclose(f);

    N = dl_size(ras);
    printf("Read %i entries and %i bad lines.\n", N, nbad);

    if (dl_size(ras) != HD_NENTRIES) {
        printf("WARNING: expected %i Henry Draper catalog entries.\n", HD_NENTRIES);
    }

    hd = malloc(sizeof(int) * N);
    il_copy(hds, 0, N, hd);
    il_free(hds);
    for (i=0; i<N; i++)
        if (hd[i] != i+1) {
            printf("Line %i is HD %i\n", i+1, hd[i]);
            break;
        }
    // HACK  - don't allocate 'em in the first place...
    free(hd);

    if (tyckd) {
        // HD catalog has only 2 digits of RA,Dec degrees.
        double arcsec = deg2arcsec(0.02);
        r2 = arcsec2distsq(arcsec);
    }

    xyz = malloc(sizeof(double) * 3 * N);
    for (i=0; i<N; i++) {
        radecdeg2xyzarr(dl_get(ras, i), dl_get(decs, i), xyz + 3*i);

        if (tyckd) {
            kdtree_qres_t* res;
            res = kdtree_rangesearch(tyckd, xyz + 3*i, r2);
            printf("%i results\n", res->nres);
        }
    }

    if (tyckd) {
        free(tyckd->data);
        kdtree_free(tyckd);
    }

    dl_free(ras);
    dl_free(decs);

	tt = kdtree_kdtypes_to_treetype(exttype, treetype, datatype);
	D = 3;
	if (convert) {
        double lo[] = {-1.0, -1.0, -1.0};
        double hi[] = { 1.0,  1.0,  1.0};
		printf("Converting data...\n");
        kd = kdtree_new(N, D, Nleaf);
        kdtree_set_limits(kd, lo, hi);
        kd = kdtree_convert_data(kd, xyz, N, D, Nleaf, tt);
		printf("Building tree...\n");
		kd = kdtree_build(kd, kd->data.any, N, D, Nleaf, tt, buildopts);
	} else {
		printf("Building tree...\n");
		kd = kdtree_build(NULL, xyz, N, D, Nleaf, tt, buildopts);
	}

    hdr = qfits_header_default();
    qfits_header_add(hdr, "AN_FILE", "HDTREE", "Henry Draper catalog kdtree", NULL);
    boilerplate_add_fits_headers(hdr);
    fits_add_long_history(hdr, "This file was created by the following command-line:");
    fits_add_args(hdr, args, argc);

    if (kdtree_fits_write(kd, outfn, hdr)) {
        ERROR("Failed to write kdtree");
        exit(-1);
    }

    printf("Done.\n");

	qfits_header_destroy(hdr);
    free(xyz);
    kdtree_free(kd);

    return 0;
}

