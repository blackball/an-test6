#include <stdio.h>
#include <errno.h>
#include <string.h>

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "blocklist.h"
#include "matchobj.h"
#include "matchfile.h"
#include "lsfile.h"

char* OPTIONS = "hR:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   -R rdls-file\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	char* rdlsfname = NULL;
	FILE* rdlsfid = NULL;
	blocklist* rdls;
	int i;
	int correct, incorrect;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'R':
			rdlsfname = optarg;
			break;
		default:
			return (OPT_ERR);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = argv + optind;
	} else {
		fprintf(stderr, "You must specify at least one input match file.\n");
		printHelp(progname);
		exit(-1);
	}
	if (!rdlsfname) {
		fprintf(stderr, "You must specify an RDLS file!\n");
		printHelp(progname);
		exit(-1);
	}

	fopenin(rdlsfname, rdlsfid);
	fprintf(stderr, "Reading rdls file...\n");
	rdls = read_ls_file(rdlsfid, 2);
	if (!rdls) {
		fprintf(stderr, "Couldn't read rdls file.\n");
		exit(-1);
	}
	fclose(rdlsfid);

	correct = incorrect = 0;

	for (i=0; i<ninputfiles; i++) {
		FILE* infile = NULL;
		char* fname;
		int nread;

		fname = inputfiles[i];
		fopenin(fname, infile);

		fprintf(stderr, "Reading from %s...\n", fname);
		fflush(stderr);
		nread = 0;
		for (;;) {
			MatchObj* mo;
			matchfile_entry me;
			int c;
			int fieldnum;
			double x1,y1,z1;
			double x2,y2,z2;
			double xc,yc,zc;
			double rac, decc, arc;
			double ra,dec;
			double dist2, r;
			double radius2;
			blocklist* rdlist;
			int j, M;
			bool ok = TRUE;

			// detect EOF and exit gracefully...
			c = fgetc(infile);
			if (c == EOF)
				break;
			else
				ungetc(c, infile);

			if (matchfile_read_match(infile, &mo, &me)) {
				fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
				fflush(stderr);
				break;
			}
			nread++;
			fieldnum = me.fieldnum;

			/*
			  printf("\n");
			  printf("Field %i\n", fieldnum);
			  printf("Parity %i\n", me.parity);
			  printf("IndexPath %s\n", me.indexpath);
			  printf("FieldPath %s\n", me.fieldpath);
			  printf("CodeTol %g\n", me.codetol);
			  //printf("FieldUnits [%g, %g]\n", me.fieldunits_lower, me.fieldunits_upper);
			  printf("Quad %i\n", (int)mo->quadno);
			  printf("Stars %i %i %i %i\n", (int)mo->iA, (int)mo->iB, (int)mo->iC, (int)mo->iD);
			  printf("FieldObjs %i %i %i %i\n", (int)mo->fA, (int)mo->fB, (int)mo->fC, (int)mo->fD);
			  printf("CodeErr %g\n", mo->code_err);
			*/

			x1 = star_ref(mo->sMin, 0);
			y1 = star_ref(mo->sMin, 1);
			z1 = star_ref(mo->sMin, 2);
			// normalize.
			r = sqrt(square(x1) + square(y1) + square(z1));
			x1 /= r;
			y1 /= r;
			z1 /= r;
			x2 = star_ref(mo->sMax, 0);
			y2 = star_ref(mo->sMax, 1);
			z2 = star_ref(mo->sMax, 2);
			r = sqrt(square(x2) + square(y2) + square(z2));
			x2 /= r;
			y2 /= r;
			z2 /= r;

			xc = (x1 + x2) / 2.0;
			yc = (y1 + y2) / 2.0;
			zc = (z1 + z2) / 2.0;
			r = sqrt(square(xc) + square(yc) + square(zc));
			xc /= r;
			yc /= r;
			zc /= r;

			radius2 = square(xc - x1) + square(yc - y1) + square(zc - z1);
			rac  = rad2deg(xy2ra(xc, yc));
			decc = rad2deg(z2dec(zc));
			arc  = 60.0 * rad2deg(distsq2arc(square(x2-x1)+square(y2-y1)+square(z2-z1)));

			rdlist = (blocklist*)blocklist_pointer_access(rdls, fieldnum);
			if (!rdlist) {
				fprintf(stderr, "Couldn't get RDLS entry for field %i!\n", fieldnum);
				exit(-1);
			}

			// read the RDLS entries for this field and make sure they're all
			// within radius of the center.
			M = blocklist_count(rdlist) / 2;
			for (j=0; j<M; j++) {
				double x, y, z;
				ra  = blocklist_double_access(rdlist, j*2);
				dec = blocklist_double_access(rdlist, j*2 + 1);
				// in degrees
				ra  = deg2rad(ra);
				dec = deg2rad(dec);
				x = radec2x(ra, dec);
				y = radec2y(ra, dec);
				z = radec2z(ra, dec);
				dist2 = square(x - xc) + square(y - yc) + square(z - zc);

				// 1.1 is a "fudge factor"
				if (dist2 > (radius2 * 1.1)) {
					fprintf(stderr, "\nField %i: matchfile says center is (%g, %g) degrees and scale is %g arcmin,\n",
							fieldnum, rac, decc, arc);
					fprintf(stderr, "  but rdls entry %i is at (%g, %g).\n", j, rad2deg(ra), rad2deg(dec));
					ok = FALSE;
					break;
				}
			}

			if (ok) {
				correct++;
				fprintf(stderr, "Field %5i is correct: (%8.3f, %8.3f), scale %6.3f arcmin.\n", fieldnum, rac, decc, arc);
			} else {
				incorrect++;
			}

			free_star(mo->sMin);
			free_star(mo->sMax);
			free_MatchObj(mo);
			free(me.indexpath);
			free(me.fieldpath);
		}
		fprintf(stderr, "Read %i matches.\n", nread);
		fflush(stderr);

		fclose(infile);
	}
	ls_file_free(rdls);

	fprintf(stderr, "%i hits correct, %i incorrect.\n",
			correct, incorrect);

	return 0;
}

