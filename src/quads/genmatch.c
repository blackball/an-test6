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

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "matchobj.h"
#include "matchfile.h"
#include "fieldcheck.h"

char* OPTIONS = "hf:o:F:n:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -f <fieldcheck-file>\n"
			"   -o <output-match-file>\n"
			"   [-F <filenum>]  (default 1)\n"
			"   -n <fieldnum>\n\n",
			progname);
}

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char* fcfname = NULL;
	char* matchfname = NULL;
	fieldcheck_file* fcf = NULL;
	fieldcheck fc;

	int i;

	matchfile* mf;
	MatchObj mo;

	int filenum = 1;
	int fieldnum = -1;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'f':
			fcfname = optarg;
			break;
		case 'o':
			matchfname = optarg;
			break;
		case 'F':
			filenum = atoi(optarg);
			break;
		case 'n':
			fieldnum = atoi(optarg);
			break;
		}
	}
	if (!fcfname || !matchfname || (fieldnum == -1)) {
		printHelp(progname);
		exit(-1);
	}

	fcf = fieldcheck_file_open(fcfname);
	if (!fcf) {
		fprintf(stderr, "Failed to open fieldcheck file %s.\n", fcfname);
		exit(-1);
	}

	mf = matchfile_open_for_writing(matchfname);
	if (!mf) {
		fprintf(stderr, "Failed to open matchfile for writing: %s\n", matchfname);
		exit(-1);
	}

	for (i=0; i<fcf->nrows; i++) {
		fieldcheck_file_read_entries(fcf, &fc, i, 1);
		if ((fc.filenum == filenum) && (fc.fieldnum == fieldnum))
			break;
	}
	if (i == fcf->nrows) {
		fprintf(stderr, "No fieldcheck entry with filenum=%i and fieldnum=%i was found.\n",
				filenum, fieldnum);
		exit(-1);
	}
	fieldcheck_file_close(fcf);

	{
		double center[3];
		double minpt[3];
		double maxpt[3];
		double ra, dec;
		double dx, dy, dz;
		double r;
		double radius;

		printf("Field center RA,DEC (%g, %g) degrees.\n", fc.ra, fc.dec);
		printf("Field radius %g arcmin.\n", fc.radius / 60.0);

		ra  = deg2rad(fc.ra );
		dec = deg2rad(fc.dec);
		radius = sqrt(arcsec2distsq(fc.radius));
		center[0] = radec2x(ra, dec);
		center[1] = radec2y(ra, dec);
		center[2] = radec2z(ra, dec);
		// dx,dy,dz is a vector in the direction of increasing DEC.
		dx = -sin(dec) * cos(ra);
		dy = -sin(dec) * sin(ra);
		dz =  cos(dec);
		// normalize.
		r = sqrt(dx*dx + dy*dy + dz*dz);
		dx /= r;
		dy /= r;
		dz /= r;
		maxpt[0] = center[0] + dx * radius;
		maxpt[1] = center[1] + dy * radius;
		maxpt[2] = center[2] + dz * radius;
		r = sqrt(maxpt[0]*maxpt[0] + maxpt[1]*maxpt[1] + maxpt[2]*maxpt[2]);
		maxpt[0] /= r;
		maxpt[1] /= r;
		maxpt[2] /= r;
		minpt[0] = center[0] - dx * radius;
		minpt[1] = center[1] - dy * radius;
		minpt[2] = center[2] - dz * radius;
		r = sqrt(minpt[0]*minpt[0] + minpt[1]*minpt[1] + minpt[2]*minpt[2]);
		minpt[0] /= r;
		minpt[1] /= r;
		minpt[2] /= r;

		memset(&mo, 0, sizeof(MatchObj));
		mo.sMin[0] = minpt[0];
		mo.sMin[1] = minpt[1];
		mo.sMin[2] = minpt[2];
		mo.sMax[0] = maxpt[0];
		mo.sMax[1] = maxpt[1];
		mo.sMax[2] = maxpt[2];
	}
	if (matchfile_write_header(mf) ||
		matchfile_write_match(mf, &mo) ||
		matchfile_fix_header(mf) ||
		matchfile_close(mf)) {
		fprintf(stderr, "Error writing match.\n");
		exit(-1);
	}

	return 0;
}
