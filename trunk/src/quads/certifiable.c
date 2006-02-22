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

char* OPTIONS = "hR:PF";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   -R rdls-file\n"
			"   [-F] false-positive mode: read fields from stdin, write lists of false-positive fields.\n"
			"   [-P] print info about each hit\n",
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
	bool printhits = FALSE;
	bool falsepos = FALSE;
	bool fromstdin = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'F':
			falsepos = TRUE;
			break;
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'P':
			printhits = TRUE;
			break;
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
		fromstdin = TRUE;
		ninputfiles = 1;
	}
	/*
	  } else {
	  fprintf(stderr, "You must specify at least one input match file.\n");
	  printHelp(progname);
	  exit(-1);
	  }
	*/
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

		if (fromstdin) {
			fname = "stdin";
			infile = stdin;
		} else {
			fname = inputfiles[i];
			fopenin(fname, infile);
		}

		fprintf(stderr, "Reading from %s...\n", fname);
		fflush(stderr);
		nread = 0;

		if (falsepos) {
			fprintf(stdout, "false_positives=array([");
			fflush(stdout);
		}

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
			double xavg, yavg, zavg;
			double fieldrad2;

			// detect EOF and exit gracefully...
			c = fgetc(infile);
			if (c == EOF)
				break;
			else
				ungetc(c, infile);

			if (falsepos) {

				if (fscanf(infile, " %i, %lf, %lf, %lf, %lf, %lf, %lf, \n", &fieldnum, &x1, &y1, &z1, &x2, &y2, &z2) != 7) {
					fprintf(stderr, "Error reading a match from %s\n", fname);
					fflush(stderr);
					break;
				}

			} else {
				if (matchfile_read_match(infile, &mo, &me)) {
					fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
					fflush(stderr);
					break;
				}
				fieldnum = me.fieldnum;
				x1 = star_ref(mo->sMin, 0);
				y1 = star_ref(mo->sMin, 1);
				z1 = star_ref(mo->sMin, 2);
				x2 = star_ref(mo->sMax, 0);
				y2 = star_ref(mo->sMax, 1);
				z2 = star_ref(mo->sMax, 2);

				if (printhits) {
					fprintf(stderr, "\n");
					fprintf(stderr, "Field %i\n", fieldnum);
					fprintf(stderr, "Parity %i\n", me.parity);
					fprintf(stderr, "IndexPath %s\n", me.indexpath);
					fprintf(stderr, "FieldPath %s\n", me.fieldpath);
					fprintf(stderr, "CodeTol %g\n", me.codetol);
					//fprintf(stderr, "FieldUnits [%g, %g]\n", me.fieldunits_lower, me.fieldunits_upper);
					fprintf(stderr, "Quad %i\n", (int)mo->quadno);
					fprintf(stderr, "Stars %i %i %i %i\n", (int)mo->iA, (int)mo->iB, (int)mo->iC, (int)mo->iD);
					fprintf(stderr, "FieldObjs %i %i %i %i\n", (int)mo->fA, (int)mo->fB, (int)mo->fC, (int)mo->fD);
					fprintf(stderr, "CodeErr %g\n", mo->code_err);
				}

			}
			nread++;

			// normalize.
			r = sqrt(square(x1) + square(y1) + square(z1));
			x1 /= r;
			y1 /= r;
			z1 /= r;
			if (printhits) {
				ra =  rad2deg(xy2ra(x1, y1));
				dec = rad2deg(z2dec(z1));
				fprintf(stderr, "RaDecMin %8.3f, %8.3f degrees\n", ra, dec);
			}
			r = sqrt(square(x2) + square(y2) + square(z2));
			x2 /= r;
			y2 /= r;
			z2 /= r;
			if (printhits) {
				ra =  rad2deg(xy2ra(x2, y2));
				dec = rad2deg(z2dec(z2));
				fprintf(stderr, "RaDecMax %8.3f, %8.3f degrees\n", ra, dec);
			}

			xc = (x1 + x2) / 2.0;
			yc = (y1 + y2) / 2.0;
			zc = (z1 + z2) / 2.0;
			r = sqrt(square(xc) + square(yc) + square(zc));
			xc /= r;
			yc /= r;
			zc /= r;
			if (printhits) {
				ra =  rad2deg(xy2ra(xc, yc));
				dec = rad2deg(z2dec(zc));
				fprintf(stderr, "Center   %8.3f, %8.3f degrees\n", ra, dec);
			}

			radius2 = square(xc - x1) + square(yc - y1) + square(zc - z1);
			rac  = rad2deg(xy2ra(xc, yc));
			decc = rad2deg(z2dec(zc));
			arc  = 60.0 * rad2deg(distsq2arc(square(x2-x1)+square(y2-y1)+square(z2-z1)));
			if (printhits) {
				fprintf(stderr, "Scale %g arcmin\n", arc);
			}

			rdlist = (blocklist*)blocklist_pointer_access(rdls, fieldnum);
			if (!rdlist) {
				fprintf(stderr, "Couldn't get RDLS entry for field %i!\n", fieldnum);
				exit(-1);
			}

			// read the RDLS entries for this field and make sure they're all
			// within radius of the center.
			M = blocklist_count(rdlist) / 2;
			xavg = yavg = zavg = 0.0;
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
				xavg += x;
				yavg += y;
				zavg += z;
				dist2 = square(x - xc) + square(y - yc) + square(z - zc);

				// 1.1 is a "fudge factor"
				if (dist2 > (radius2 * 1.1)) {
					fprintf(stderr, "\nField %i: match says center (%g, %g), scale %g arcmin, but\n",
							fieldnum, rac, decc, arc);
					fprintf(stderr, "rdls %i is (%g, %g).\n", j, rad2deg(ra), rad2deg(dec));
					ok = FALSE;
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
				ra  = blocklist_double_access(rdlist, j*2);
				dec = blocklist_double_access(rdlist, j*2 + 1);
				// in degrees
				ra  = deg2rad(ra);
				dec = deg2rad(dec);
				x = radec2x(ra, dec);
				y = radec2y(ra, dec);
				z = radec2z(ra, dec);
				dist2 = square(x - xavg) + square(y - yavg) + square(z - zavg);
				if (dist2 > fieldrad2)
					fieldrad2 = dist2;
			}
			if (fieldrad2 * 1.1 < radius2) {
				fprintf(stderr, "\nField %i: match says scale is %g, but field radius is %g.\n", fieldnum,
						60.0 * rad2deg(distsq2arc(radius2)),
						60.0 * rad2deg(distsq2arc(fieldrad2)));
				ok = FALSE;
			}

			if (ok) {
				correct++;
				fprintf(stderr, "Field %5i is correct: (%8.3f, %8.3f), scale %6.3f arcmin.\n", fieldnum, rac, decc, arc);
			} else {
				incorrect++;
				if (falsepos) {
					fprintf(stdout, "%i,", fieldnum);
					fflush(stdout);
				}
			}

			if (!falsepos) {
				free_star(mo->sMin);
				free_star(mo->sMax);
				free_MatchObj(mo);
				free(me.indexpath);
				free(me.fieldpath);
			}
		}

		if (falsepos) {
			fprintf(stdout, "])\n");
			fflush(stdout);
		}

		fprintf(stderr, "Read %i matches.\n", nread);
		fflush(stderr);

		if (!fromstdin)
			fclose(infile);
	}
	ls_file_free(rdls);

	fprintf(stderr, "%i hits correct, %i incorrect.\n",
			correct, incorrect);

	return 0;
}

