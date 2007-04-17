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
#include <string.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "matchfile.h"
#include "rdlist.h"
#include "histogram.h"
#include "solvedfile.h"

char* OPTIONS = "hR:A:B:n:t:f:C:T:F:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] <input-match-file> ...\n"
			"   -R rdls-file-template\n"
			"   [-A <first-field>]  (default 0)\n"
			"   [-B <last-field>]   (default the largest field encountered)\n"
			"   [-n <negative-fields-rdls>]"
			"   [-f <false-positive-fields-rdls>]\n"
			"   [-t <true-positive-fields-rdls>]\n"
			"   [-C <number-of-RDLS-stars-to-compute-center>]\n"
			"   [-T <true-positive-solvedfile>]\n"
			"   [-F <false-positive-solvedfile>]  (note, both of these are per-MATCH, not per-FIELD)\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	char* rdlsfname = NULL;
	rdlist* rdls = NULL;
	int i;
	int correct, incorrect, warning;
	int firstfield = 0;
	int lastfield = -1;

	int nfields;
	int *corrects;
	int *incorrects;
	int *warnings;

	char* truefn = NULL;
	char* fpfn = NULL;
	char* negfn = NULL;

	int Ncenter = 0;

	double* xyz = NULL;

	char* fpsolved = NULL;
	char* tpsolved = NULL;
	int nfields_total;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'A':
			firstfield = atoi(optarg);
			break;
		case 'B':
			lastfield = atoi(optarg);
			break;
		case 'C':
			Ncenter = atoi(optarg);
			break;
		case 'R':
			rdlsfname = optarg;
			break;
		case 't':
			truefn = optarg;
			break;
		case 'f':
			fpfn = optarg;
			break;
		case 'n':
			negfn = optarg;
			break;
		case 'T':
			tpsolved = optarg;
			break;
		case 'F':
			fpsolved = optarg;
			break;
		default:
			return (OPT_ERR);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = argv + optind;
	} else {
		printHelp(progname);
		exit(-1);
	}
	if (!rdlsfname) {
		fprintf(stderr, "You must specify an RDLS file!\n");
		printHelp(progname);
		exit(-1);
	}

	printf("Reading rdls file...\n");
	fflush(stdout);
	rdls = rdlist_open(rdlsfname);
	if (!rdls) {
		fprintf(stderr, "Couldn't read rdls file.\n");
		exit(-1);
	}
	rdls->xname = "RA";
	rdls->yname = "DEC";

	nfields = rdls->nfields;
	printf("Read %i fields from rdls file.\n", nfields);
	if ((lastfield != -1) && (nfields > lastfield)) {
		nfields = lastfield + 1;
	} else {
		lastfield = nfields - 1;
	}

	corrects =   calloc(sizeof(int), nfields);
	warnings =   calloc(sizeof(int), nfields);
	incorrects = calloc(sizeof(int), nfields);

	correct = incorrect = warning = 0;
	nfields_total = 0;

	for (i=0; i<ninputfiles; i++) {
		matchfile* mf;
		int nread;
		char* fname = inputfiles[i];
		MatchObj* mo;
		int k;

		printf("Opening matchfile %s...\n", fname);
		mf = matchfile_open(fname);
		if (!mf) {
			fprintf(stderr, "Failed to open matchfile %s.\n", fname);
			exit(-1);
		}
		nread = 0;

		for (k=0; k<mf->nrows; k++) {
			int fieldnum;
			double x1,y1,z1;
			double x2,y2,z2;
			double xc,yc,zc;
			double rac, decc, arc;
			double ra,dec;
			double dist2;
			double radius2;
			dl* rdlist;
			int j, M;
			double xavg, yavg, zavg;
			double fieldrad2;
			bool warn = FALSE;
			bool err  = FALSE;

			mo = matchfile_buffered_read_match(mf);
			if (!mo) {
				fprintf(stderr, "Failed to read a match from file %s\n", fname);
				break;
			}

			fieldnum = mo->fieldnum;
			if (fieldnum < firstfield)
				continue;
			if (fieldnum > lastfield)
				// here we assume the fields are ordered...
				break;

			/*
			  printf("quad %u, stars { %u, %u, %u, %u }, fieldobjs { %u, %u, %u, %u }\n",
			  mo->quadno, mo->star[0], mo->star[1], mo->star[2], mo->star[3],
			  mo->field[0], mo->field[1], mo->field[2], mo->field[3]);
			  printf("    codeerr %f, mincorner { %g, %g, %g }, maxcorner { %g, %g %g }, noverlap %i\n",
			  mo->code_err, mo->sMin[0], mo->sMin[1], mo->sMin[2],
			  mo->sMax[0], mo->sMax[1], mo->sMax[2], (int)mo->noverlap);
			*/

			nread++;

			x1 = mo->sMin[0];
			y1 = mo->sMin[1];
			z1 = mo->sMin[2];
			x2 = mo->sMax[0];
			y2 = mo->sMax[1];
			z2 = mo->sMax[2];
			xc = (x1 + x2) / 2.0;
			yc = (y1 + y2) / 2.0;
			zc = (z1 + z2) / 2.0;

			// normalize.
			normalize(&x1, &y1, &z1);
			normalize(&x2, &y2, &z2);
			normalize(&xc, &yc, &zc);

			radius2 = square(xc - x1) + square(yc - y1) + square(zc - z1);
			rac  = rad2deg(xy2ra(xc, yc));
			decc = rad2deg(z2dec(zc));
			arc  = 60.0 * rad2deg(distsq2arc(square(x2-x1)+square(y2-y1)+square(z2-z1)));

			// read the RDLS entries for this field and make sure they're all
			// within "radius" of the center.
			rdlist = rdlist_get_field(rdls, fieldnum);
			M = dl_size(rdlist) / 2;
			xyz = realloc(xyz, M * 3 * sizeof(double));
			xavg = yavg = zavg = 0.0;
			for (j=0; j<M; j++) {
				double x, y, z;
				ra  = dl_get(rdlist, j*2);
				dec = dl_get(rdlist, j*2 + 1);
				// in degrees
				ra  = deg2rad(ra);
				dec = deg2rad(dec);
				x = radec2x(ra, dec);
				y = radec2y(ra, dec);
				z = radec2z(ra, dec);
				xavg += x;
				yavg += y;
				zavg += z;
				dist2 = square(x - xc) + square(y - yc) + square(z - zc);
				xyz[j*3 + 0] = x;
				xyz[j*3 + 1] = y;
				xyz[j*3 + 2] = z;

				// 1.2 is a "fudge factor"
				if (dist2 > (radius2 * 1.2)) {
					printf("\nError: Field %i: match says center (%g, %g), scale %g arcmin, but\n",
							fieldnum, rac, decc, arc);
					printf("rdls %i is (%g, %g).\n", j, rad2deg(ra), rad2deg(dec));
                                        printf("Logprob %g (%g).\n", mo->logodds, exp(mo->logodds));
					err = TRUE;
					break;
				}
			}
			// make another sweep through, finding the field star furthest from the mean.
			// this gives an estimate of the field radius.
			xavg /= (double)M;
			yavg /= (double)M;
			zavg /= (double)M;
			fieldrad2 = 0.0;
			for (j=0; j<M; j++) {
				double x, y, z;
				x = xyz[j*3 + 0];
				y = xyz[j*3 + 1];
				z = xyz[j*3 + 2];
				dist2 = square(x - xavg) + square(y - yavg) + square(z - zavg);
				if (dist2 > fieldrad2)
					fieldrad2 = dist2;
			}
			if (fieldrad2 * 1.2 < radius2) {
				printf("\nWarning: Field %i: match says scale is %g, but field radius is %g.\n", fieldnum,
						60.0 * rad2deg(distsq2arc(radius2)),
						60.0 * rad2deg(distsq2arc(fieldrad2)));
				warn = TRUE;
			}

			if (err) {
				incorrect++;
				incorrects[fieldnum]++;
			} else if (warn) {
				warning++;
				warnings[fieldnum]++;
			} else {
                            printf("Field %5i: correct hit: (%8.3f, %8.3f), scale %6.3f arcmin, logodds %g (%g)\n",
                                   fieldnum, rac, decc, arc, mo->logodds, exp(mo->logodds));
				corrects[fieldnum]++;
				correct++;
			}
			fflush(stdout);

			if (tpsolved && !err)
				solvedfile_set(tpsolved, nfields_total);
			if (fpsolved && err)
				solvedfile_set(fpsolved, nfields_total);

			nfields_total++;
		}

		printf("Read %i matches.\n", nread);

		matchfile_close(mf);
	}

	printf("%i hits correct, %i warnings, %i errors.\n",
			correct, warning, incorrect);

	if (tpsolved)
		solvedfile_setsize(tpsolved, nfields_total);
	if (fpsolved)
		solvedfile_setsize(fpsolved, nfields_total);

	rdlist_close(rdls);

	return 0;
}

