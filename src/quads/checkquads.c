/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <errno.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "quadfile.h"
#include "qidxfile.h"
#include "bl.h"
#include "fitsioutils.h"

#define OPTIONS "hf:"
const char HelpString[] =
"quadidx -f fname\n";

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
	int argidx, argchar;
	char *qidxfname = NULL;
	char *quadfname = NULL;
	quadfile* quad;
	qidxfile* qidx;
	uint q, s;
	
	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			qidxfname = mk_qidxfn(optarg);
			quadfname = mk_quadfn(optarg);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	quad = quadfile_open(quadfname, 0);
	if (!quad) {
		fprintf(stderr, "Couldn't open quads file %s.\n", quadfname);
		exit(-1);
	}

	qidx = qidxfile_open(qidxfname, 0);
	if (!qidx) {
		fprintf(stderr, "Couldn't open qidx file %s.\n", qidxfname);
		exit(-1);
	}

	if (quad->numquads != qidx->numquads) {
		fprintf(stderr, "Number of quads does not agree: %i vs %i\n",
				quad->numquads, qidx->numquads);
		exit(-1);
	}

	printf("Checking stars...\n");
	for (s=0; s<qidx->numstars; s++) {
		uint* quads, nquads;
		int j;
		qidxfile_get_quads(qidx, s, &quads, &nquads);
		for (j=0; j<nquads; j++) {
			uint star[4];
			int k, n;
			quadfile_get_starids(quad, quads[j], star, star+1, star+2, star+3);
			n = 0;
			for (k=0; k<4; k++) {
				if (star[k] == s)
					n++;
			}
			if (n != 1) {
				fprintf(stderr, "Star %i, quad %i: found %i instances of the quad in the qidx (not 1)\n",
						s, quads[j], n);
				fprintf(stderr, "  found: ");
				for (k=0; k<4; k++) {
					fprintf(stderr, "%i ", star[k]);
				}
				fprintf(stderr, "\n");
			}
		}
	}

	printf("Checking quads...\n");
	for (q=0; q<quad->numquads; q++) {
		uint star[4];
		uint* quads, nquads;
		int j;
		quadfile_get_starids(quad, q, star, star+1, star+2, star+3);
		for (j=0; j<4; j++) {
			uint k;
			int n;
			qidxfile_get_quads(qidx, star[j], &quads, &nquads);
			n = 0;
			for (k=0; k<nquads; k++) {
				if (quads[k] == q)
					n++;
			}
			if (n != 1) {
				fprintf(stderr, "Quad %i, star %i: found %i instances of the quad in the qidx (not 1)\n",
						q, star[j], n);
				fprintf(stderr, "  found: ");
				for (k=0; k<nquads; k++) {
					fprintf(stderr, "%i ", quads[k]);
				}
				fprintf(stderr, "\n");
			}
		}
	}

	quadfile_close(quad);
	qidxfile_close(qidx);

	return 0;
}
