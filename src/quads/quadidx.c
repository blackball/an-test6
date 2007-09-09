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

/**
   Quadidx: create .qidx files from .quad files.

   A .quad file lists, for each quad, the stars comprising the quad.
   A .qidx file lists, for each star, the quads that star is a member of.

   Input: .quad
   Output: .qidx
 */

#include <errno.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "quadfile.h"
#include "qidxfile.h"
#include "bl.h"
#include "fitsioutils.h"
#include "boilerplate.h"

#define OPTIONS "hf:F"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s -f <filename-template>\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
	char* progname = argv[0];
	int argidx, argchar;
	char *idxfname = NULL;
	char *quadfname = NULL;
	il** quadlist;
	quadfile* quads;
	qidxfile* qidx;
	uint q;
	int i;
	uint numused;
	qfits_header* quadhdr;
	qfits_header* qidxhdr;
	
	if (argc <= 2) {
		printHelp(progname);
		exit(-1);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			idxfname = mk_qidxfn(optarg);
			quadfname = mk_quadfn(optarg);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			printHelp(progname);
			exit(-1);
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		printHelp(progname);
		exit(-1);
	}

	fprintf(stderr, "quadidx: indexing quads in %s...\n",
	        quadfname);

	fprintf(stderr, "will write to file %s .\n", idxfname);

	quads = quadfile_open(quadfname);
	if (!quads) {
		fprintf(stderr, "Couldn't open quads file %s.\n", quadfname);
		exit(-1);
	}

	fprintf(stderr, "%u quads, %u stars.\n", quads->numquads, quads->numstars);

	quadlist = calloc(quads->numstars, sizeof(il*));

	for (q=0; q<quads->numquads; q++) {
        // DIMQUADS
		uint inds[4];

		quadfile_get_stars(quads, q, inds);

		// append this quad index to the lists of each of
		// its four stars.
		for (i=0; i<4; i++) {
			il* list;
			uint starind = inds[i];
			list = quadlist[starind];
			// create the list if necessary
			if (!list) {
				list = il_new(10);
				quadlist[starind] = list;
			}
			il_append(list, q);
		}
	}
	
	// first count numused:
	// how many stars are members of quads.
	numused = 0;
	for (i=0; i<quads->numstars; i++) {
		il* list = quadlist[i];
		if (!list) continue;
		numused++;
	}
	printf("%u stars used\n", numused);

	qidx = qidxfile_open_for_writing(idxfname, quads->numstars, quads->numquads);
	if (!qidx) {
 		fprintf(stderr, "Couldn't open outfile qidx file %s.\n", idxfname);
		exit(-1);
	}

	quadhdr = quadfile_get_header(quads);
	qidxhdr = qidxfile_get_header(qidx);

	fits_copy_header(quadhdr, qidxhdr, "INDEXID");
	fits_copy_header(quadhdr, qidxhdr, "HEALPIX");

	boilerplate_add_fits_headers(qidxhdr);
	qfits_header_add(qidxhdr, "HISTORY", "This file was created by the program \"quadidx\".", NULL, NULL);
	qfits_header_add(qidxhdr, "HISTORY", "quadidx command line:", NULL, NULL);
	fits_add_args(qidxhdr, argv, argc);
	qfits_header_add(qidxhdr, "HISTORY", "(end of quadidx command line)", NULL, NULL);

	qfits_header_add(qidxhdr, "HISTORY", "** History entries copied from the input file:", NULL, NULL);
	fits_copy_all_headers(quadhdr, qidxhdr, "HISTORY");
	qfits_header_add(qidxhdr, "HISTORY", "** End of history entries.", NULL, NULL);

	if (qidxfile_write_header(qidx)) {
 		fprintf(stderr, "Couldn't write qidx header (%s).\n", idxfname);
		exit(-1);
	}

	for (i=0; i<quads->numstars; i++) {
		uint thisnumq;
		uint thisstar;
		uint* stars; // bad variable name - list of quads this star is in.
		il* list = quadlist[i];
		if (list) {
			thisnumq = (uint)il_size(list);
			stars = malloc(thisnumq * sizeof(uint));
			il_copy(list, 0, thisnumq, (int*)stars);
		} else {
			thisnumq = 0;
			stars = NULL;
		}
		thisstar = i;

		if (qidxfile_write_star(qidx,  stars, thisnumq)) {
			fprintf(stderr, "Couldn't write star to qidx file (%s).\n", idxfname);
			exit(-1);
		}

		if (list) {
			free(stars);
			il_free(list);
			quadlist[i] = NULL;
		}
	}
	free(quadlist);
	quadfile_close(quads);

	if (qidxfile_close(qidx)) {
		fprintf(stderr, "Failed to close qidx file.\n");
		exit(-1);
	}
	free_fn(idxfname);

	fprintf(stderr, "  done.\n");
	return 0;
}

