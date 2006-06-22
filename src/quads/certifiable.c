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

char* OPTIONS = "hR:A:B:n:t:f:L:H:b:C:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] <input-match-file> ...\n"
			"   -R rdls-file\n"
			"   [-A <first-field>]  (default 0)\n"
			"   [-B <last-field>]   (default the largest field encountered)\n"
			"   [-n <negative-fields-rdls>]"
			"   [-f <false-positive-fields-rdls>]\n"
			"   [-t <true-positive-fields-rdls>]\n"
			"   [-b <bin-size>]: histogram overlap %% into bins of this size (default 1)\n"
			"   [-C <number-of-RDLS-stars-to-compute-center>]\n\n",
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

	il** rightfieldbins;
	il** wrongfieldbins;
	il** negfieldbins;

	double overlap_lowcorrect = 1.0;
	double overlap_highwrong = 0.0;

	double binsize = 1.0;
	int Nbins = 0;
	int* overlap_hist_right = NULL;
	int* overlap_hist_wrong = NULL;

	int Ncenter = 0;

	double* xyz = NULL;
	int bin;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'b':
			binsize = atof(optarg);
			break;
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

	Nbins = (int)ceil(100.0 / binsize) + 1;
	overlap_hist_right = calloc(Nbins, sizeof(int));
	overlap_hist_wrong = calloc(Nbins, sizeof(int));

	rightfieldbins = calloc(Nbins, sizeof(il*));
	wrongfieldbins = calloc(Nbins, sizeof(il*));
	negfieldbins = calloc(Nbins, sizeof(il*));

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
			double dist2, r;
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
					printf("rdls %i is (%g, %g).  Overlap %4.1f%% (%i/%i)\n", j, rad2deg(ra), rad2deg(dec),
							100.0 * mo->overlap, mo->noverlap, mo->ninfield);
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

			//bin = rint(100.0 * mo->overlap / binsize);
			bin = floor(100.0 * mo->overlap / binsize);
			if (bin < 0) bin = 0;
			if (bin >= Nbins) bin = Nbins-1;
			if (err)
				overlap_hist_wrong[bin]++;
			else
				overlap_hist_right[bin]++;
				
			if (err) {
				incorrect++;
				incorrects[fieldnum]++;

				if (!wrongfieldbins[bin])
					wrongfieldbins[bin] = il_new(256);
				il_append(wrongfieldbins[bin], fieldnum);

				if (mo->overlap > overlap_highwrong)
					overlap_highwrong = mo->overlap;
			} else if (warn) {
				warning++;
				warnings[fieldnum]++;

				if (!rightfieldbins[bin])
					rightfieldbins[bin] = il_new(256);
				il_append(rightfieldbins[bin], fieldnum);

				if ((mo->overlap != 0.0) && (mo->overlap < overlap_lowcorrect))
					overlap_lowcorrect = mo->overlap;
			} else {
				printf("Field %5i: correct hit: (%8.3f, %8.3f), scale %6.3f arcmin, overlap %4.1f%% (%i/%i)\n",
						fieldnum, rac, decc, arc, 100.0 * mo->overlap, mo->noverlap, mo->ninfield);
				corrects[fieldnum]++;
				correct++;

				if (!rightfieldbins[bin])
					rightfieldbins[bin] = il_new(256);
				il_append(rightfieldbins[bin], fieldnum);

				if ((mo->overlap != 0.0) && (mo->overlap < overlap_lowcorrect))
					overlap_lowcorrect = mo->overlap;
			}
			fflush(stdout);
		}

		printf("Read %i matches.\n", nread);

		matchfile_close(mf);
	}

	printf("%i hits correct, %i warnings, %i errors.\n",
			correct, warning, incorrect);

	// here we sort of assume that the hits file has been processed with "agreeable" so that
	// only agreeing hits are included for each field.

	for (bin=0; bin<Nbins; bin++) {
		il* list = wrongfieldbins[bin];
		if (list) {
			printf("Bin %i (overlap %4.1f to %4.1f %%) wrong [%i]: ",
				   bin, bin * binsize, (bin+1) * binsize, il_size(list));
			for (i=0; i<il_size(list); i++)
				printf("%i ", il_get(list, i));
			printf("\n");
		}
	}
	printf("\n");
	fflush(stdout);

	for (bin=0; bin<Nbins; bin++) {
		il* list = rightfieldbins[bin];
		if (list) {
			printf("Bin %i (overlap %4.1f to %4.1f %%) right [%i]: ", bin,
				   bin * binsize, (bin+1) * binsize, il_size(list));
			for (i=0; i<il_size(list); i++)
				printf("%i ", il_get(list, i));
			printf("\n");
		}
	}
	printf("\n");
	fflush(stdout);

	for (bin=0; bin<Nbins; bin++) {
		il* list = negfieldbins[bin];
		if (list) {
			printf("Bin %i (overlap %4.1f to %4.1f %%) unsolved [%i]: ", bin,
				   bin * binsize, (bin+1) * binsize, il_size(list));
			for (i=0; i<il_size(list); i++)
				printf("%i ", il_get(list, i));
			printf("\n");
		}
	}
	printf("\n");
	fflush(stdout);

	{
		int sumright=0, sumwrong=0;
		int ntotal = lastfield - firstfield + 1;
		double pct = 100.0 / (double)ntotal;
		unsigned char* solved = malloc(ntotal);

		printf("Total of %i fields.\n", ntotal);
		printf("Threshold%%   #Solved  %%Solved     #Unsolved  %%Unsolved   #FalsePositive\n");

		for (bin=0; bin<Nbins; bin++) {
			int nright, nwrong, nunsolved;
			il* list;

			list = rightfieldbins[bin];
			if (!list)
				nright = 0;
			else
				nright = il_size(list);

			list = wrongfieldbins[bin];
			if (!list)
				nwrong = 0;
			else
				nwrong = il_size(list);

			sumright += nright;
			sumwrong += nwrong;
			nunsolved = ntotal - sumright - sumwrong;

			if (nunsolved) {
				negfieldbins[bin] = il_new(256);
				memset(solved, 0, ntotal);
				list = rightfieldbins[bin];
				if (list)
					for (i=0; i<il_size(list); i++)
						solved[il_get(list, i) - firstfield] = 1;
				list = wrongfieldbins[bin];
				if (list)
					for (i=0; i<il_size(list); i++)
						solved[il_get(list, i) - firstfield] = 1;
				for (i=0; i<ntotal; i++)
					if (!solved[i])
						il_append(negfieldbins[bin], i + firstfield);
			}

			printf("  %5.1f       %4i     %6.2f         %4i      %6.2f          %i\n",
				   (bin+1)*binsize, sumright, pct*sumright, nunsolved, pct*nunsolved, sumwrong);
		}
		printf("\n\n");
		free(solved);
	}

	printf("Largest overlap of an incorrect match: %4.1f%%.\n",
		   100.0 * overlap_highwrong);
	printf("Smallest overlap of a correct match: %4.1f%%.\n",
		   100.0 * overlap_lowcorrect);

	printf("\noverlap_hist_wrong = [ ");
	for (i=0; i<Nbins; i++) {
		printf("%i, ", overlap_hist_wrong[i]);
	}
	printf("]\n");

	printf("overlap_hist_right = [ ");
	for (i=0; i<Nbins; i++) {
		printf("%i, ", overlap_hist_right[i]);
	}
	printf("]\n\n");

	printf("Finding field centers...\n");
	fflush(stdout);
	fieldcenters = malloc(2 * nfields * sizeof(double));
	for (i=0; i<(2*nfields); i++)
		fieldcenters[i] = -1e6;
	for (i=firstfield; i<=lastfield; i++) {
		dl* rdlist;
		int j, M;
		double xavg, yavg, zavg;
		rdlist = rdlist_get_field(rdls, i);
		if (!rdlist) {
			printf("Couldn't get RDLS entry for field %i!\n", i);
			exit(-1);
		}
		M = dl_size(rdlist) / 2;
		if (Ncenter && Ncenter < M)
			M = Ncenter;
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

	for (bin=0; bin<Nbins; bin++) {
		il* list;
		list = wrongfieldbins[bin];
		if (!list)
			continue;
		printf("Bin %i: false positive field true centers (deg):\n  [ ", bin);
		for (i=0; i<il_size(list); i++) {
			int fld = il_get(list, i);
			printf("%7.4f,%7.4f,  ", fieldcenters[fld*2], fieldcenters[fld*2+1]);
		}
		printf("];\n");
	}
	printf("\n\n");

	for (bin=0; bin<Nbins; bin++) {
		il* list;
		list = negfieldbins[bin];
		if (!list)
			continue;
		printf("Bin %i: unsolved field true centers (deg):\n  [ ", bin);
		for (i=0; i<il_size(list); i++) {
			int fld = il_get(list, i);
			printf("%7.4f,%7.4f,  ", fieldcenters[fld*2], fieldcenters[fld*2+1]);
		}
		printf("];\n");
	}
	printf("\n\n");

	{
		char* fns[3]  = { fpfn,    truefn,    negfn };
		il** lists[3] = { wrongfieldbins, rightfieldbins, negfieldbins };
		char* descstrs[3] = {
			"False positive fields: true locations.",
			"Correctly solved fields.",
			"Negative (non-solving) fields."
		};
		int nfn;
		for (nfn=0; nfn<3; nfn++) {
			char* fn = fns[nfn];
			il** fields = lists[nfn];
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

			for (bin=0; bin<Nbins; bin++) {
				int b;
				rdlist_write_new_field(rdls);
				for (b=0; b<=bin; b++) {
					il* list = fields[b];
					if (!list)
						continue;
					for (i=0; i<il_size(list); i++) {
						int fld = il_get(list, i);
						rdlist_write_entries(rdls, fieldcenters+(2*fld), 1);
					}
				}
				rdlist_fix_field(rdls);
			}
			rdlist_fix_header(rdls);
			rdlist_close(rdls);
		}
	}

	rdlist_close(rdls);

	free(corrects);
	free(warnings);
	free(incorrects);

	for (bin=0; bin<Nbins; bin++) {
		if (rightfieldbins[bin])
			il_free(rightfieldbins[bin]);
		if (wrongfieldbins[bin])
			il_free(wrongfieldbins[bin]);
		if (negfieldbins[bin])
			il_free(negfieldbins[bin]);
	}
	free(rightfieldbins);
	free(wrongfieldbins);
	free(negfieldbins);

	free(fieldcenters);

	free(overlap_hist_right);
	free(overlap_hist_wrong);

	return 0;
}

