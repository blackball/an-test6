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

char* OPTIONS = "hR:A:B:n:t:f:L:H:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] <input-match-file> ...\n"
			"   -R rdls-file\n"
			"   [-A <first-field>]  (default 0)\n"
			"   [-B <last-field>]   (default the largest field encountered)\n"
			"   [-L <agree-threshold-low>]  (default 3)\n"
			"   [-H <agree-threshold-low>]  (default 10)\n"
			"   [-n <negative-fields-rdls>]"
			"   [-f <false-positive-fields-rdls>]\n"
			"   [-t <true-positive-fields-rdls>]\n\n",
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
	rdlist* rdls;
	int i;
	int correct, incorrect, warning;
	int firstfield = 0;
	int lastfield = -1;

	int nfields;
	int *corrects;
	int *incorrects;
	int *warnings;

	double* fieldcenters;

	char* truefn = NULL;
	char* fpfn = NULL;
	char* negfn = NULL;

	int thresh;
	il* fplist;
	il* neglist;
	il* truelist;
	pl* fplists;
	pl* neglists;
	pl* truelists;
	il* totals;
	il* rights;
	il* wrongs;
	il* unsolveds;

	double overlap_lowcorrect = 1.0;
	double overlap_highwrong = 0.0;

	int TL = 3;
	int TH = 10;

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
		case 'L':
			TL = atoi(optarg);
			break;
		case 'H':
			TH = atoi(optarg);
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
	rdls = rdlist_open(rdlsfname);
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
		matchfile* mf;
		int nread;
		char* fname = inputfiles[i];

		fprintf(stderr, "Opening matchfile %s...\n", fname);
		mf = matchfile_open(fname);
		if (!mf) {
			fprintf(stderr, "Failed to open matchfile %s.\n", fname);
			exit(-1);
		}
		nread = 0;

		for (;;) {
			matchfile_entry me;
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
			uint k;
			int res;

			if ((res = matchfile_next_table(mf, &me))) {
				if (res == -1)
					fprintf(stderr, "Failed to read the next table from matchfile %s.\n", fname);
				break;
			}

			fieldnum = me.fieldnum;
			if (fieldnum < firstfield)
				continue;
			if (fieldnum > lastfield)
				// here we assume the fields are ordered...
				break;

			for (k=0; k<mf->nrows; k++) {
				MatchObj* mo;
				mo = matchfile_buffered_read_match(mf);
				if (!mo) {
					fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
					break;
				}

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
				rdlist = (dl*)rdlist_get_field(rdls, fieldnum);
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
					if (mo->overlap > overlap_highwrong)
						overlap_highwrong = mo->overlap;
				} else if (warn) {
					warning++;
					warnings[fieldnum]++;
					if ((mo->overlap > 0.0) && (mo->overlap < overlap_lowcorrect))
						overlap_lowcorrect = mo->overlap;
				} else {
					corrects[fieldnum]++;
					correct++;
					fprintf(stderr, "Field %5i: correct hit: (%8.3f, %8.3f), scale %6.3f arcmin, overlap %4.1f%%\n",
							fieldnum, rac, decc, arc, 100.0 * mo->overlap);
					if ((mo->overlap > 0.0) && (mo->overlap < overlap_lowcorrect))
						overlap_lowcorrect = mo->overlap;
				}

				//free_MatchObj(mo);
			}
			free(me.indexpath);
			free(me.fieldpath);
		}

		fprintf(stderr, "Read %i matches.\n", nread);
		fflush(stderr);

		matchfile_close(mf);
	}

	fprintf(stderr, "%i hits correct, %i warnings, %i errors.\n",
			correct, warning, incorrect);

	// here we sort of assume that the hits file has been processed with "agreeable" so that
	// only agreeing hits are included for each field.
	fplists = pl_new(10);
	neglists = pl_new(10);
	truelists = pl_new(10);
	totals = il_new(10);
	rights = il_new(10);
	wrongs = il_new(10);
	unsolveds = il_new(10);

	for (thresh=TL; thresh<=TH; thresh++) {
		int right=0, wrong=0, unsolved=0, total=0;
		fplist  = il_new(32);
		neglist = il_new(32);
		truelist = il_new(256);
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
				il_append(truelist, i);
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
		pl_append(truelists, truelist);
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
		truelist = pl_get(truelists, thresh-TL);
		printf("Threshold=%i: solved:\n  [ ", thresh);
		for (i=0; i<il_size(truelist); i++)
			printf("%i, ", il_get(truelist, i));
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
		rdlist = (dl*)rdlist_get_field(rdls, i);
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

	{
		char* fns[3] = { fpfn,    truefn,    negfn };
		pl* lists[3] = { fplists, truelists, neglists };
		char* descstrs[3] = {
			"False positive fields: true locations.",
			"Correctly solved fields.",
			"Negative (non-solving) fields."
		};
		int nfn;
		for (nfn=0; nfn<3; nfn++) {
			char* fn = fns[nfn];
			if (!fn) continue;
			rdlist* rdls = rdlist_open_for_writing(fn);
			if (!rdls) {
				fprintf(stderr, "Couldn't open file %s to write rdls.\n", fn);
				exit(-1);
			}
			qfits_header_add(rdls->header, "COMMENT", descstrs[nfn], NULL, NULL);
			qfits_header_add(rdls->header, "COMMENT", "Extension x holds results for", NULL, NULL);
			qfits_header_add(rdls->header, "COMMENT", "   nagree threshold=x.", NULL, NULL);
			rdlist_write_header(rdls);
			for (thresh=0; thresh<TL; thresh++) {
				rdlist_write_new_field(rdls);
				rdlist_fix_field(rdls);
			}
			for (thresh=TL; thresh<=TH; thresh++) {
				il* fldlist = (il*)pl_get(lists[nfn], thresh-TL);
				rdlist_write_new_field(rdls);
				for (i=0; i<il_size(fldlist); i++) {
					int fld = il_get(fldlist, i);
					rdlist_write_entries(rdls, fieldcenters+(2*fld), 1);
				}
				rdlist_fix_field(rdls);
			}
			rdlist_fix_header(rdls);
			rdlist_close(rdls);
		}
	}

	for (thresh=TL; thresh<=TH; thresh++) {
		fplist  = (il*)pl_get(fplists,  thresh-TL);
		neglist = (il*)pl_get(neglists, thresh-TL);
		truelist = (il*)pl_get(truelists, thresh-TL);
		il_free(fplist );
		il_free(neglist);
		il_free(truelist);
	}
	pl_free(fplists );
	pl_free(neglists);
	pl_free(truelists);
	il_free(totals);
	il_free(rights);
	il_free(wrongs);
	il_free(unsolveds);

	rdlist_close(rdls);

	free(corrects);
	free(warnings);
	free(incorrects);

	free(fieldcenters);

	return 0;
}

