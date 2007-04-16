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

#include "starutil.h"
#include "fileutil.h"
#include "qidxfile.h"

#define OPTIONS "hf:"
const char HelpString[] =
"findquad -f fname \n";

extern char *optarg;
extern int optind, opterr, optopt;

char *qidxfname = NULL;
uint thestar;
uint thequad;
char buff[100], maxstarWidth;

signed int compare_sidx(const void *x, const void *y)
{
	uint xval, yval;
	xval = *(uint *)x;
	yval = *(uint *)y;
	if (xval > yval)
		return (1);
	else if (xval < yval)
		return ( -1);
	else
		return (0);
}

int main(int argc, char *argv[])
{
	int argidx, argchar; 
	uint numstars;
	char scanrez = 1;
	qidxfile* qidx;

	if (argc <= 2) {
		fprintf(stderr, "not enough arguments. usage:\n");
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			qidxfname = mk_qidxfn(optarg);
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

	fprintf(stderr, "findquad: looking up quads in %s\n", qidxfname);

	fprintf(stderr, "  Reading quad index...");
	fflush(stderr);

	qidx = qidxfile_open(qidxfname, 0);
	if (!qidx) {
		fprintf(stderr, "Failed to open qidxfile %s\n.", qidxfname);
		exit(-1);
	}
	numstars = qidx->numstars;
	fprintf(stderr, "  (%u stars)\n", numstars);

	while (!feof(stdin) && scanrez) {
		uint* quads;
		uint nquads;
		uint i;

		scanrez = fscanf(stdin, "%u", &thestar);

		if (qidxfile_get_quads(qidx, thestar, &quads, &nquads)) {
			fprintf(stderr, "Couldn't get quads for star %u.\n", thestar);
			fprintf(stdout, "[]\n");
			continue;
		}
		fprintf(stdout, "[");
		for (i=0; i<nquads; i++)
			fprintf(stdout, "%u, ", quads[i]);
		fprintf(stdout, "]\n");
		fflush(stdout);
	}

	qidxfile_close(qidx);
	free_fn(qidxfname);
	return 0;
}





