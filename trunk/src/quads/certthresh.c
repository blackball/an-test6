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
#include "xylist.h"

char* OPTIONS = "hR:F:L:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] <input-match-file> ...\n"
			"   -R rdls-file\n"
			"   [-F <first-field>]  (default 0)\n"
			"   [-L <last-field>]   (default the largest field encountered)\n\n",
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
	xylist* rdls;
	int i;
	int correct, incorrect, warning;
	int firstfield = 0;
	int lastfield = -1;

	int nfields;
	int *corrects;
	int *incorrects;
	int *warnings;

	double* fieldcenters;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'F':
			firstfield = atoi(optarg);
			break;
		case 'L':
			lastfield = atoi(optarg);
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
		printHelp(progname);
		exit(-1);
	}
	if (!rdlsfname) {
		fprintf(stderr, "You must specify an RDLS file!\n");
		printHelp(progname);
		exit(-1);
	}

	fprintf(stderr, "Reading rdls file...\n");
	rdls = xylist_open(rdlsfname);
	if (!rdls) {
		fprintf(stderr, "Couldn't read rdls file.\n");
		exit(-1);
	}
	rdls->xname = "RA";
	rdls->yname = "DEC";

	nfields = rdls->nfields;
	fprintf(stderr, "Read %i fields from rdls file.\n", nfields);
	if ((lastfield != -1) && (nfields > lastfield)) {
		nfields = lastfield + 1;
	} else {
		lastfield = nfields - 1;
	}


	corrects =   calloc(sizeof(int), nfields);
	warnings =   calloc(sizeof(int), nfields);
	incorrects = calloc(sizeof(int), nfields);

	correct = incorrect = warning = 0;


	for (i=0; i<ninputfiles; i++) {
		FILE* infile = NULL;
		char* fname;
		int nread;

		fname = inputfiles[i];
		fprintf(stderr, "Reading from %s...\n", fname);
		fopenin(fname, infile);
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
			dl* rdlist;
			int j, M;
			bool warn = FALSE;
			bool err  = FALSE;
			double xavg, yavg, zavg;
			double fieldrad2;

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
			fieldnum = me.fieldnum;
			if (fieldnum < firstfield)
				continue;
			if (fieldnum > lastfield)
				continue;

			nread++;

			x1 = mo->sMin[0];
			y1 = mo->sMin[1];
			z1 = mo->sMin[2];
			x2 = mo->sMax[0];
			y2 = mo->sMax[1];
			z2 = mo->sMax[2];

			// normalize.
			r = sqrt(square(x1) + square(y1) + square(z1));
			x1 /= r;
			y1 /= r;
			z1 /= r;
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

			// read the RDLS entries for this field and make sure they're all
			// within radius of the center.
			rdlist = (dl*)xylist_get_field(rdls, fieldnum);
			M = dl_size(rdlist) / 2;
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

				// 1.1 is a "fudge factor"
				if (dist2 > (radius2 * 1.1)) {
					fprintf(stderr, "\nError: Field %i: match says center (%g, %g), scale %g arcmin, but\n",
							fieldnum, rac, decc, arc);
					fprintf(stderr, "rdls %i is (%g, %g).\n", j, rad2deg(ra), rad2deg(dec));
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
				ra  = dl_get(rdlist, j*2);
				dec = dl_get(rdlist, j*2 + 1);
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
			if (fieldrad2 * 1.2 < radius2) {
				fprintf(stderr, "\nWarning: Field %i: match says scale is %g, but field radius is %g.\n", fieldnum,
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
				corrects[fieldnum]++;
				correct++;
				fprintf(stderr, "Field %5i: correct hit: (%8.3f, %8.3f), scale %6.3f arcmin.\n", fieldnum, rac, decc, arc);
			}

			free_MatchObj(mo);
			free(me.indexpath);
			free(me.fieldpath);
		}

		fprintf(stderr, "Read %i matches.\n", nread);
		fflush(stderr);

		fclose(infile);
	}

	fprintf(stderr, "%i hits correct, %i warnings, %i errors.\n",
			correct, warning, incorrect);

	// here we sort of assume that the hits file has been processed with "agreeable" so that
	// only agreeing hits are included for each field.
	{
		int thresh;
		il* fplist;
		il* neglist;
		pl* fplists = pl_new(10);
		pl* neglists = pl_new(10);
		il* totals = il_new(10);
		il* rights = il_new(10);
		il* wrongs = il_new(10);
		il* unsolveds = il_new(10);
		int TL = 3;
		int TH = 10;
		for (thresh=TL; thresh<=TH; thresh++) {
			int right=0, wrong=0, unsolved=0, total=0;
			fplist  = il_new(32);
			neglist = il_new(32);
			for (i=0; i<nfields; i++) {
				if (i < firstfield)
					continue;
				if (i > lastfield)
					continue;
				if ((corrects[i] + warnings[i]) >= thresh) {
					if (incorrects[i]) {
						fprintf(stderr, "Warning: field %i has %i correct hits but also %i incorrect hits.\n",
								i, corrects[i] + warnings[i], incorrects[i]);
					}
					right++;
				} else if (incorrects[i] >= thresh) {
					wrong++;
					il_append(fplist, i);
				} else {
					unsolved++;
					il_append(neglist, i);
				}
				total++;
			}
			il_append(totals, total);
			il_append(rights, right);
			il_append(wrongs, wrong);
			il_append(unsolveds, unsolved);
			pl_append(fplists , fplist );
			pl_append(neglists, neglist);
		}
		printf("\n\n");
		for (thresh=TL; thresh<=TH; thresh++) {
			fplist  = (il*)pl_get(fplists,  thresh-TL);
			printf("Threshold=%2i: false positives:\n  [ ", thresh);
			for (i=0; i<il_size(fplist); i++)
				printf("%i, ", il_get(fplist, i));
			printf("];\n");
		}
		printf("\n");
		for (thresh=TL; thresh<=TH; thresh++) {
			neglist = (il*)pl_get(neglists, thresh-TL);
			printf("Threshold=%i: unsolved:\n  [ ", thresh);
			for (i=0; i<il_size(neglist); i++)
				printf("%i, ", il_get(neglist, i));
			printf("];\n");
		}

		printf("\n");
		for (thresh=TL; thresh<=TH; thresh++) {
			int right, wrong, unsolved, total;
			double pct;
			right    = il_get(rights,    thresh-TL);
			wrong    = il_get(wrongs,    thresh-TL);
			unsolved = il_get(unsolveds, thresh-TL);
			total    = il_get(totals,    thresh-TL);
			if (thresh == TL) {
				printf("Total of %i fields.\n", total);
				printf("Threshold   #Solved  %%Solved     #Unsolved %%Unsolved   #FalsePositive\n");
			}
			pct = 100.0 / (double)total;
			printf("  %2i         %4i     %6.2f         %4i      %6.2f          %i\n",
				   thresh, right, pct*right, unsolved, pct*unsolved, wrong);
		}
		printf("\n\n");

		printf("Finding field centers...\n");
		fflush(stdout);
		fieldcenters = malloc(2 * nfields * sizeof(double));
		for (i=0; i<(2*nfields); i++)
			fieldcenters[i] = -1e6;
		for (i=0; i<nfields; i++) {
			dl* rdlist;
			int j, M;
			double xavg, yavg, zavg;
			rdlist = (dl*)xylist_get_field(rdls, i);
			if (!rdlist) {
				fprintf(stderr, "Couldn't get RDLS entry for field %i!\n", i);
				exit(-1);
			}
			M = dl_size(rdlist) / 2;
			xavg = yavg = zavg = 0.0;
			for (j=0; j<M; j++) {
				double x, y, z, ra, dec;
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
			}
			xavg /= (double)M;
			yavg /= (double)M;
			zavg /= (double)M;
			fieldcenters[i*2 + 0] = rad2deg(xy2ra(xavg, yavg));
			fieldcenters[i*2 + 1] = rad2deg(z2dec(zavg));
			dl_free(rdlist);
		}

		for (thresh=TL; thresh<=TH; thresh++) {
			fplist  = (il*)pl_get(fplists,  thresh-TL);
			printf("Threshold=%2i: false positive field true centers (deg):\n  [ ", thresh);
			for (i=0; i<il_size(fplist); i++) {
				int fld = il_get(fplist, i);
				printf("%7.4f,%7.4f,  ", fieldcenters[fld*2], fieldcenters[fld*2+1]);
			}
			printf("];\n");
		}

		for (thresh=TL; thresh<=TH; thresh++) {
			neglist = (il*)pl_get(neglists, thresh-TL);
			printf("Threshold=%i: unsolved field centers (deg):\n  [ ", thresh);
			for (i=0; i<il_size(neglist); i++) {
				int fld = il_get(neglist, i);
				printf("%7.4f,%7.4f,  ", fieldcenters[fld*2], fieldcenters[fld*2+1]);
			}
			printf("];\n");
		}

		for (thresh=TL; thresh<=TH; thresh++) {
			fplist  = (il*)pl_get(fplists,  thresh-TL);
			neglist = (il*)pl_get(neglists, thresh-TL);
			il_free(fplist );
			il_free(neglist);
		}
		pl_free(fplists );
		pl_free(neglists);
		il_free(totals);
		il_free(rights);
		il_free(wrongs);
		il_free(unsolveds);
	}

	xylist_close(rdls);

	free(corrects);
	free(warnings);
	free(incorrects);

	free(fieldcenters);

	return 0;
}

