/*
  This file is part of the Astrometry.net suite.
  Copyright 2009,2010 Dustin Lang.

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

#include <stdint.h>
#include <stdio.h>

#include "starkd.h"
#include "log.h"
#include "errors.h"
#include "boilerplate.h"
#include "starutil.h"
#include "rdlist.h"

static const char* OPTIONS = "hvr:d:R:t:Io:";//T";

void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] <star-kdtree-file>\n"
		   "    [-o <ra-dec-list>]: write FITS table (default: print ASCII to stdout)\n"
		   "    [-r <ra>] (deg)\n"
		   "    [-d <dec>] (deg)\n"
		   "    [-R <radius>] (deg)\n"
		   "    [-t <tagalong-column>]\n"
		   //"    [-T]: tag-along all\n"
		   "    [-I]: print indices too\n"
		   "    [-v]: +verbose\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char **argv) {
    int argchar;
	startree_t* starkd;
	double ra=0.0, dec=0.0, radius=0.0;
	sl* tag = sl_new(4);
	bool tagall = FALSE;
	char* starfn = NULL;
	int loglvl = LOG_MSG;
	char** myargs;
	int nmyargs;
	double xyz[3];
	double r2;
	bool getinds = FALSE;
	double* radec;
	int* inds;
	int N;
	pl* tagdata = pl_new(4);
	int i;
	char* rdfn = NULL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'o':
			rdfn = optarg;
			break;
		case 'I':
			getinds = TRUE;
			break;
		case 'r':
			ra = atof(optarg);
			break;
		case 'd':
			dec = atof(optarg);
			break;
		case 'R':
			radius = atof(optarg);
			break;
		case 't':
			sl_append(tag, optarg);
			break;
		case 'T':
			tagall = TRUE;
			break;
		case 'v':
			loglvl++;
			break;
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			printHelp(argv[0]);
			break;
		default:
			return -1;
		}

	nmyargs = argc - optind;
	myargs = argv + optind;

	if (nmyargs != 1) {
		printHelp(argv[0]);
		exit(-1);
	}
	starfn = myargs[0];

	log_init(loglvl);

	starkd = startree_open(starfn);
	if (!starkd) {
		ERROR("Failed to open star kdtree");
		exit(-1);
	}

	radecdeg2xyzarr(ra, dec, xyz);
	r2 = deg2distsq(radius);

	logmsg("Searching kdtree %s at RA,Dec = (%g,%g), radius %g deg.\n",
		   starfn, ra, dec, radius);

	startree_search_for(starkd, xyz, r2,
						NULL, &radec, &inds, &N);

	logmsg("Got %i results.\n", N);

	for (i=0; i<sl_size(tag); i++) {
		void* data = startree_get_data_column(starkd, sl_get(tag, i), inds, N);
		if (!data) {
			ERROR("Failed to read tag-along column %s\n", sl_get(tag, i));
			break;
		}
		pl_append(tagdata, data);
	}

	if (rdfn) {
		rdlist_t* rd = rdlist_open_for_writing(rdfn);
		il* colnums = il_new(16);
		if (!rd) {
			ERROR("Failed to open output file %s", rdfn);
			exit(-1);
		}
		if (rdlist_write_primary_header(rd)) {
			ERROR("Failed to write header to output file %s", rdfn);
			exit(-1);
		}
		for (i=0; i<sl_size(tag); i++) {
			// HACK -- assume double!
			int colnum = rdlist_add_tagalong_column(rd, fitscolumn_double_type(), 1,
													fitscolumn_double_type(), sl_get(tag, i), NULL);
			il_append(colnums, colnum);
		}
		if (rdlist_write_header(rd)) {
			ERROR("Failed to write header to output file %s", rdfn);
			exit(-1);
		}

		for (i=0; i<N; i++) {
			if (rdlist_write_one_radec(rd, radec[i*2+0], radec[i*2+1])) {
				ERROR("Failed to write RA,Dec to output file %s", rdfn);
				exit(-1);
			}
		}
		for (i=0; i<sl_size(tag); i++) {
			// HACK -- assume double!
			if (rdlist_write_tagalong_column(rd, il_get(colnums, i), 0, N, pl_get(tagdata, i), sizeof(double))) {
				ERROR("Failed to write tag-along data column %s", sl_get(tag, i));
				exit(-1);
			}
		}
		if (rdlist_fix_header(rd) ||
			rdlist_fix_primary_header(rd) ||
			rdlist_close(rd)) {
			ERROR("Failed to close output file %s", rdfn);
			exit(-1);
		}
		il_free(colnums);

	} else {
		// Header
		printf("# RA, Dec");
		if (getinds)
			printf(", index");
		for (i=0; i<sl_size(tag); i++)
			printf(", %s", sl_get(tag, i));
		printf("\n");

		for (i=0; i<N; i++) {
			int j;
			printf("%g, %g", radec[i*2+0], radec[i*2+1]);
			if (getinds)
				printf(", %i", inds[i]);
			for (j=0; j<sl_size(tagdata); j++) {
				double* data = pl_get(tagdata, j);
				printf(", %g", data[i]);
			}
			printf("\n");
		}
	}

	free(radec);
	free(inds);
	// etc

	return 0;
}


