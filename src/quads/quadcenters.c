/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "quadfile.h"
#include "kdtree.h"
#include "fileutil.h"
#include "starutil.h"
#include "bl.h"
#include "starkd.h"
#include "boilerplate.h"
#include "rdlist.h"

#define OPTIONS "hr:"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname)
{
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n"
			"   -r <rdls-output-file>\n"
			"   [-h]: help\n"
			"   <base-name> [<base-name> ...]\n\n"
			"Reads .quad and .skdt files.  Writes an RDLS containing the quad centers (midpoints of AB), one field per input file.\n\n",
	        progname);
}

int main(int argc, char** args) {
    int argchar;
	char* basename;
	char* outfn = NULL;
	char* fn;
	quadfile* qf;
    rdlist* rdls;
	startree* skdt = NULL;
	int i;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'r':
			outfn = optarg;
			break;
		case 'h':
			print_help(args[0]);
			exit(0);
		}

	if (!outfn || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

    rdls = rdlist_open_for_writing(outfn);
    if (!rdls) {
        fprintf(stderr, "Failed to open RDLS file %s for output.\n", outfn);
        exit(-1);
    }
    if (rdlist_write_header(rdls)) {
        fprintf(stderr, "Failed to write RDLS header.\n");
        exit(-1);
    }

	for (; optind<argc; optind++) {
		int Nstars;
		basename = args[optind];
		printf("Reading files with basename %s\n", basename);

		fn = mk_quadfn(basename);
		qf = quadfile_open(fn, 0);
		if (!qf) {
			fprintf(stderr, "Failed to open quad file %s.\n", fn);
            exit(-1);
		}
		free_fn(fn);

        fn = mk_streefn(basename);
        printf("Trying to open star kdtree %s...\n", fn);
        skdt = startree_open(fn);
        if (!skdt) {
            fprintf(stderr, "Failed to read star kdtree %s.\n", fn);
            exit(-1);
        }
        Nstars = startree_N(skdt);

        if (rdlist_write_new_field(rdls)) {
            fprintf(stderr, "Failed to write new RDLS field header.\n");
            exit(-1);
        }

		printf("Reading quads...\n");
		for (i=0; i<qf->numquads; i++) {
			uint stars[4];
            double axyz[3], bxyz[3];
            double midab[3];
            double radec[2];
			if (!(i % 200000)) {
				printf(".");
				fflush(stdout);
			}
			quadfile_get_starids(qf, i, stars, stars+1, stars+2, stars+3);
            startree_get(skdt, stars[0], axyz);
            startree_get(skdt, stars[1], bxyz);
            star_midpoint(midab, axyz, bxyz);
            xyzarr2radecdegarr(midab, radec);
            if (rdlist_write_entries(rdls, radec, 1)) {
                fprintf(stderr, "Failed to write a RA,Dec entry.\n");
                exit(-1);
            }
		}
		printf("\n");

        startree_close(skdt);
		quadfile_close(qf);

        if (rdlist_fix_field(rdls)) {
            fprintf(stderr, "Failed to fix RDLS field header.\n");
            exit(-1);
        }
	}

    if (rdlist_fix_header(rdls) ||
        rdlist_close(rdls)) {
        fprintf(stderr, "Failed to close RDLS file.\n");
        exit(-1);
    }

	return 0;
}
