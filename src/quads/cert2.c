#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "matchfile.h"
#include "rdlist.h"
#include "fieldcheck.h"
#include "histogram.h"

char* OPTIONS = "hr:A:B:I:J:n:t:f:b:C:o:F:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] <input-match-file> ...\n"
			"   ( -r rdls-file-template   OR   -F fieldcheck-file )\n"
			"   [-A <first-field>]  (default 0)\n"
			"   [-B <last-field>]   (default the largest field encountered)\n"
			"   [-I first-field-filenum]\n"
			"   [-J last-field-filenum]\n"
			"   [-n <negative-fields-rdls>]"
			"   [-f <false-positive-fields-rdls>]\n"
			"   [-t <true-positive-fields-rdls>]\n"
			"   [-b <bin-size>]: histogram overlap %% into bins of this size (default 1)\n"
			"   [-C <number-of-RDLS-stars-to-compute-center>]\n"
			"   [-o <number-of-RDLS-stars-to-check>]\n\n",
			progname);
}

double* xyz = NULL;
histogram* overlap_hist_right = NULL;
histogram* overlap_hist_wrong = NULL;
int bin;
int correct, incorrect, warning;
il** rightfieldbins;
il** wrongfieldbins;
il** negfieldbins;
double overlap_lowcorrect = 1.0;
double overlap_highwrong = 0.0;
dl** neglist;
dl** fplist;
dl** tplist;
int Ncenter = 0;
int Ncheck = 0;
double* fieldcenters = NULL;
double* radecs = NULL;

static void check_field(int fieldfile, int fieldnum, rdlist* rdls,
						fieldcheck* fc,
						bl* matches) {
	int i, j;
	int M;
	double xavg, yavg, zavg;
	double fieldrad2 = 0.0;
	double dist2;
	double x, y, z;
	double ra, dec;

	if (!bl_size(matches) && !neglist)
		return;

	xavg = yavg = zavg = 0.0;
	if (rdls) {
		// read the RDLS entries for this field
		M = rdlist_n_entries(rdls, fieldnum);
		if (Ncheck && Ncheck < M)
			M = Ncheck;
		if (!bl_size(matches)) {
			if (Ncenter && Ncenter < M)
				M = Ncenter;
		}
		radecs = realloc(radecs, 2*M*sizeof(double));
		rdlist_read_entries(rdls, fieldnum, 0, M, radecs);
		xyz = realloc(xyz, M * 3 * sizeof(double));

		for (j=0; j<M; j++) {
			ra  = radecs[2*j];
			dec = radecs[2*j + 1];
			// in degrees
			ra  = deg2rad(ra);
			dec = deg2rad(dec);
			x = radec2x(ra, dec);
			y = radec2y(ra, dec);
			z = radec2z(ra, dec);
			xavg += x;
			yavg += y;
			zavg += z;
			xyz[j*3 + 0] = x;
			xyz[j*3 + 1] = y;
			xyz[j*3 + 2] = z;
		}
		xavg /= (double)M;
		yavg /= (double)M;
		zavg /= (double)M;

		ra = xy2ra(xavg, yavg);
		dec = z2dec(zavg);
	} else {
		assert(fc->filenum == fieldfile);
		assert(fc->fieldnum == fieldnum);
		M = 1;
		ra  = fc->ra;
		dec = fc->dec;
		fieldrad2 = arcsec2distsq(fc->radius);
		xyz = realloc(xyz, M * 3 * sizeof(double));
		xyz[0] = radec2x(deg2rad(ra), deg2rad(dec));
		xyz[1] = radec2y(deg2rad(ra), deg2rad(dec));
		xyz[2] = radec2z(deg2rad(ra), deg2rad(dec));
	}
	fieldcenters[fieldnum * 2 + 0] = rad2deg(ra);
	fieldcenters[fieldnum * 2 + 1] = rad2deg(dec);

	if (!bl_size(matches)) {
		return;
	}

	if (rdls) {
		// make another sweep through, finding the field star furthest from the mean.
		// this gives an estimate of the field radius.
		fieldrad2 = 0.0;
		for (j=0; j<M; j++) {
			x = xyz[j*3 + 0];
			y = xyz[j*3 + 1];
			z = xyz[j*3 + 2];
			dist2 = square(x - xavg) + square(y - yavg) + square(z - zavg);
			if (dist2 > fieldrad2)
				fieldrad2 = dist2;
		}
	}

	for (i=0; i<bl_size(matches); i++) {
		MatchObj* mo;
		double x1,y1,z1;
		double x2,y2,z2;
		double xc,yc,zc;
		double rac, decc, arc;
		double r;
		double radius2;
		int j;
		bool warn = FALSE;
		bool err  = FALSE;

		mo = bl_access(matches, i);

		x1 = mo->sMin[0];
		y1 = mo->sMin[1];
		z1 = mo->sMin[2];
		x2 = mo->sMax[0];
		y2 = mo->sMax[1];
		z2 = mo->sMax[2];

		// normalize.
		r = sqrt(x1*x1 + y1*y1 + z1*z1);
		x1 /= r;
		y1 /= r;
		z1 /= r;
		r = sqrt(x2*x2 + y2*y2 + z2*z2);
		x2 /= r;
		y2 /= r;
		z2 /= r;
				
		xc = (x1 + x2) / 2.0;
		yc = (y1 + y2) / 2.0;
		zc = (z1 + z2) / 2.0;
		r = sqrt(xc*xc + yc*yc + zc*zc);
		xc /= r;
		yc /= r;
		zc /= r;

		radius2 = square(xc - x1) + square(yc - y1) + square(zc - z1);
		rac  = rad2deg(xy2ra(xc, yc));
		decc = rad2deg(z2dec(zc));
		arc  = rad2arcmin(distsq2arc(square(x2-x1)+square(y2-y1)+square(z2-z1)));

		for (j=0; j<M; j++) {
			x = xyz[j*3 + 0];
			y = xyz[j*3 + 1];
			z = xyz[j*3 + 2];
			dist2 = square(x - xc) + square(y - yc) + square(z - zc);

			// 1.2 is a "fudge factor"
			if (dist2 > (radius2 * 1.2)) {
				printf("\nError: Field %i: match says center (%g, %g), scale %g arcmin, but\n",
					   fieldnum, rac, decc, arc);
				ra = xy2ra(x, y);
				dec = z2dec(z);
				printf("rdls %i is (%g, %g).  Overlap %4.1f%% (%i/%i)\n", j, rad2deg(ra), rad2deg(dec),
					   100.0 * mo->overlap, mo->noverlap, mo->ninfield);
				err = TRUE;
				break;
			}
		}
		if (fieldrad2 * 1.2 < radius2) {
			printf("\nWarning: Field %i: match says scale is %g, but field radius is %g.\n", fieldnum,
				   60.0 * rad2deg(distsq2arc(radius2)),
				   60.0 * rad2deg(distsq2arc(fieldrad2)));
			warn = TRUE;
		}

		bin = histogram_add(err ? overlap_hist_wrong : overlap_hist_right,
							100.0 * mo->overlap);

		if (err) {
			incorrect++;

			if (!wrongfieldbins[bin])
				wrongfieldbins[bin] = il_new(256);
			il_append(wrongfieldbins[bin], fieldnum);

			if (mo->overlap > overlap_highwrong)
				overlap_highwrong = mo->overlap;
		} else {
			if (warn) {
				warning++;
			} else {
				printf("File %2i: Field %5i: correct hit: (%8.3f, %8.3f), scale %6.3f arcmin, overlap %4.1f%% (%i/%i)\n",
					   fieldfile, fieldnum, rac, decc, arc, 100.0 * mo->overlap, mo->noverlap, mo->ninfield);
				correct++;
			}

			if (!rightfieldbins[bin])
				rightfieldbins[bin] = il_new(256);
			il_append(rightfieldbins[bin], fieldnum);

			if ((mo->overlap != 0.0) && (mo->overlap < overlap_lowcorrect))
				overlap_lowcorrect = mo->overlap;
		}
		fflush(stdout);
	}
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char* rdlsfname = NULL;
	char* fcfname = NULL;
	fieldcheck* fclist = NULL;
	int Nfc = 0;
	int fcind = 0;

	rdlist* rdls = NULL;
	fieldcheck_file* fcf = NULL;

	int nfields;

	char* truefn = NULL;
	char* fpfn = NULL;
	char* negfn = NULL;

	int* thresh_right;
	int* thresh_wrong;
	int* thresh_unsolved;

	double binsize = 1.0;
	int Nbins;

	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	int firstfield=0, lastfield=INT_MAX-1;
	int firstfieldfile=1, lastfieldfile=INT_MAX-1;

	matchfile** mfs;
	MatchObj** mos;
	bool* eofs;
	int nread = 0;
	int f;
	int fieldfile;
	bl* matches;

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
		case 'I':
			firstfieldfile = atoi(optarg);
			break;
		case 'J':
			lastfieldfile = atoi(optarg);
			break;
		case 'C':
			Ncenter = atoi(optarg);
			break;
		case 'o':
			Ncheck = atoi(optarg);
			break;
		case 'r':
			rdlsfname = optarg;
			break;
		case 'F':
			fcfname = optarg;
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
	if (!(rdlsfname || fcfname)) {
		fprintf(stderr, "You must specify an RDLS or fieldcheck file!\n");
		printHelp(progname);
		exit(-1);
	}
	if (lastfield < firstfield) {
		fprintf(stderr, "Last field (-B) must be at least as big as first field (-A)\n");
		exit(-1);
	}

	overlap_hist_right = histogram_new_binsize(0.0, 100.0, binsize);
	overlap_hist_wrong = histogram_new_binsize(0.0, 100.0, binsize);

	Nbins = overlap_hist_right->Nbins;

	rightfieldbins = calloc(Nbins, sizeof(il*));
	wrongfieldbins = calloc(Nbins, sizeof(il*));
	negfieldbins   = calloc(Nbins, sizeof(il*));

	thresh_right    = calloc(Nbins, sizeof(int));
	thresh_wrong    = calloc(Nbins, sizeof(int));
	thresh_unsolved = calloc(Nbins, sizeof(int));

	if (negfn) {
		neglist = malloc(Nbins * sizeof(dl*));
		for (i=0; i<Nbins; i++) {
			neglist[i] = dl_new(256);
		}
	} else
		neglist = NULL;
	if (fpfn) {
		fplist = malloc(Nbins * sizeof(dl*));
		for (i=0; i<Nbins; i++) {
			fplist[i] = dl_new(256);
		}
	} else
		fplist = NULL;
	if (truefn) {
		tplist = malloc(Nbins * sizeof(dl*));
		for (i=0; i<Nbins; i++) {
			tplist[i] = dl_new(256);
		}
	} else
		tplist = NULL;

	mos =  calloc(ninputfiles, sizeof(MatchObj*));
	eofs = calloc(ninputfiles, sizeof(bool));
	mfs = malloc(ninputfiles * sizeof(matchfile*));

	for (i=0; i<ninputfiles; i++) {
		fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
		mfs[i] = matchfile_open(inputfiles[i]);
		if (!mfs[i]) {
			fprintf(stderr, "Failed to open matchfile %s.\n", inputfiles[i]);
			exit(-1);
		}
	}

	matches = bl_new(256, sizeof(MatchObj));
	correct = incorrect = warning = 0;

	if (fcfname) {
		fcf = fieldcheck_file_open(fcfname);
		if (!fcf) {
			fprintf(stderr, "Failed to open fieldcheck file %s.\n", fcfname);
			exit(-1);
		}
		Nfc = fcf->nrows;
		fclist = malloc(Nfc * sizeof(fieldcheck));
		fieldcheck_file_read_entries(fcf, fclist, 0, Nfc);
		fieldcheck_file_close(fcf);
		fcf = NULL;
		fcind = 0;
	}

	// we assume the matchfiles are sorted by field id and number.
	for (fieldfile=firstfieldfile; fieldfile<=lastfieldfile; fieldfile++) {
		bool alldone = TRUE;

		if (rdlsfname) {
			char fn[256];
			sprintf(fn, rdlsfname, fieldfile);
			printf("Reading rdls file %s ...\n", fn);
			fflush(stdout);
			rdls = rdlist_open(fn);
			if (!rdls) {
				fprintf(stderr, "Couldn't read rdls file.\n");
				exit(-1);
			}
			rdls->xname = "RA";
			rdls->yname = "DEC";

			nfields = rdls->nfields;
			printf("Read %i fields from rdls file.\n", nfields);
			if (nfields < lastfield) {
				lastfield = nfields - 1;
			}
		} else {
			int endind;
			//int maxfieldnum = 0;
			for (; fcind<Nfc; fcind++) {
				if (fclist[fcind].filenum >= fieldfile)
					break;
			}
			for (endind=fcind; endind<Nfc; endind++) {
				if (fclist[endind].filenum != fieldfile)
					break;
				/*
				  if (fclist[endind].fieldnum > maxfieldnum)
				  maxfieldnum = fclist[endind].fieldnum;
				*/
			}
			//nfields = maxfieldnum+1;
			nfields = endind - fcind;
			if (nfields < lastfield) {
				lastfield = nfields - 1;
			}
		}

		fieldcenters = realloc(fieldcenters, (lastfield+1) * 2 * sizeof(double));

		for (f=firstfield; f<=lastfield; f++) {
			int fieldnum = f;

			alldone = TRUE;
			for (i=0; i<ninputfiles; i++)
				if (!eofs[i])
					alldone = FALSE;
			if (alldone)
				break;

			fprintf(stderr, "File %i, Field %i.\n", fieldfile, f);

			bl_remove_all_but_first(matches);

			for (i=0; i<ninputfiles; i++) {
				int nr = 0;
				int ns = 0;

				while (1) {
					if (eofs[i])
						break;
					if (!mos[i])
						mos[i] = matchfile_buffered_read_match(mfs[i]);
					if (unlikely(!mos[i])) {
						eofs[i] = TRUE;
						break;
					}

					// skip past entries that are out of range...
					if ((mos[i]->fieldfile < firstfieldfile) ||
						(mos[i]->fieldfile > lastfieldfile) ||
						(mos[i]->fieldnum < firstfield) ||
						(mos[i]->fieldnum > lastfield)) {
						mos[i] = NULL;
						ns++;
						continue;
					}
					if (mos[i]->fieldfile != fieldfile)
						break;
					assert(mos[i]->fieldnum >= fieldnum);
					if (mos[i]->fieldnum != fieldnum)
						break;
					nread++;
					nr++;
					bl_append(matches, mos[i]);
					mos[i] = NULL;
				}
				if (nr || ns)
					fprintf(stderr, "File %s: read %i matches, skipped %i matches.\n", inputfiles[i], nr, ns);
			}

			check_field(fieldfile, fieldnum, rdls,
						fclist + fcind + fieldnum,
						matches);
		}

		if (rdls)
			rdlist_close(rdls);

		if (alldone)
			break;

		{
			int sumright=0, sumwrong=0;
			int ntotal = lastfield - firstfield + 1;
			unsigned char* solved = calloc(1, ntotal);

			for (bin=Nbins-1; bin>=0; bin--) {
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
				thresh_right[bin] += sumright;
				thresh_wrong[bin] += sumwrong;
				thresh_unsolved[bin] += nunsolved;
			}
			free(solved);
		}

		printf("File %i.\n", fieldfile);
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

		if (fplist) {
			for (bin=0; bin<Nbins; bin++) {
				il* list = wrongfieldbins[bin];
				if (!list)
					continue;
				for (i=0; i<il_size(list); i++) {
					int fld = il_get(list, i);
					double ra, dec;
					ra  = fieldcenters[2 * fld + 0];
					dec = fieldcenters[2 * fld + 1];
					dl_append(fplist[bin], ra);
					dl_append(fplist[bin], dec);
				}
			}
		}
		if (tplist) {
			for (bin=0; bin<Nbins; bin++) {
				il* list = rightfieldbins[bin];
				if (!list)
					continue;
				for (i=0; i<il_size(list); i++) {
					int fld = il_get(list, i);
					double ra, dec;
					ra  = fieldcenters[2 * fld + 0];
					dec = fieldcenters[2 * fld + 1];
					dl_append(tplist[bin], ra);
					dl_append(tplist[bin], dec);
				}
			}
		}
		if (neglist) {
			for (bin=0; bin<Nbins; bin++) {
				il* list = negfieldbins[bin];
				if (!list)
					continue;
				for (i=0; i<il_size(list); i++) {
					int fld = il_get(list, i);
					double ra, dec;
					ra  = fieldcenters[2 * fld + 0];
					dec = fieldcenters[2 * fld + 1];
					dl_append(neglist[bin], ra);
					dl_append(neglist[bin], dec);
				}
			}
		}



		for (bin=0; bin<Nbins; bin++) {
			if (rightfieldbins[bin])
				il_free(rightfieldbins[bin]);
			if (wrongfieldbins[bin])
				il_free(wrongfieldbins[bin]);
			if (negfieldbins[bin])
				il_free(negfieldbins[bin]);
		}
		memset(rightfieldbins, 0, Nbins*sizeof(il*));
		memset(wrongfieldbins, 0, Nbins*sizeof(il*));
		memset(  negfieldbins, 0, Nbins*sizeof(il*));
	}
	bl_free(matches);

	for (i=0; i<ninputfiles; i++)
		matchfile_close(mfs[i]);
	free(mfs);
	free(mos);
	free(eofs);

	fprintf(stderr, "\nRead %i matches.\n", nread);
	fflush(stderr);

	printf("%i hits correct, %i warnings, %i errors.\n",
		   correct, warning, incorrect);

	{
		int ntotal = thresh_unsolved[0] + thresh_right[0] + thresh_wrong[0];
		double pct = 100.0 / (double)ntotal;
		printf("\n\nTotal of %i fields.\n", ntotal);
		printf("Threshold%%   #Solved  %%Solved     #Unsolved  %%Unsolved   #FalsePositive\n");
		for (bin=Nbins-1; bin>=0; bin--) {
			printf("  %5.1f       %4i     %6.2f         %4i      %6.2f          %i\n",
				   (bin+1)*binsize, thresh_right[bin], pct*thresh_right[bin],
				   thresh_unsolved[bin], pct*thresh_unsolved[bin], thresh_wrong[bin]);
		}
		printf("\n\n");
	}

	printf("Largest overlap of an incorrect match: %4.1f%%.\n",
		   100.0 * overlap_highwrong);
	printf("Smallest overlap of a correct match: %4.1f%%.\n",
		   100.0 * overlap_lowcorrect);

	printf("\noverlap_hist_wrong = [ ");
	for (i=0; i<Nbins; i++) {
		printf("%i, ", overlap_hist_wrong->hist[i]);
	}
	printf("]\n");

	printf("overlap_hist_right = [ ");
	for (i=0; i<Nbins; i++) {
		printf("%i, ", overlap_hist_right->hist[i]);
	}
	printf("]\n\n");


	if (truefn || fpfn || negfn) {
		char* fns[3]  = { fpfn,    truefn,    negfn };
		dl** lists[3] = { fplist,  tplist,    neglist };
		char* descstrs[3] = {
			"False positive fields: true locations.",
			"Correctly solved fields.",
			"Negative (non-solving) fields."
		};
		double* tmparr = NULL;
		int nfn;
		for (nfn=0; nfn<3; nfn++) {
			char* fn = fns[nfn];
			dl** fields = lists[nfn];
			char buf[256];
			if (!fn) continue;
			printf("Writing %s...\n", fn);
			rdlist* rdls = rdlist_open_for_writing(fn);
			if (!rdls) {
				fprintf(stderr, "Couldn't open file %s to write rdls.\n", fn);
				exit(-1);
			}
			sprintf(buf, "%.10g", binsize);
			qfits_header_add(rdls->header, "BINSIZE", buf, NULL, NULL);
			qfits_header_add(rdls->header, "COMMENT", descstrs[nfn], NULL, NULL);
			qfits_header_add(rdls->header, "COMMENT", "Extension x holds results for", NULL, NULL);
			qfits_header_add(rdls->header, "COMMENT", "   bin x.", NULL, NULL);
			rdlist_write_header(rdls);

			for (bin=0; bin<Nbins; bin++) {
				rdlist_write_new_field(rdls);
				dl* list = fields[bin];
				if (!list)
					continue;
				tmparr = realloc(tmparr, dl_size(list) * 2 * sizeof(double));
				dl_copy(list, 0, dl_size(list), tmparr);
				rdlist_write_entries(rdls, tmparr, dl_size(list));
				rdlist_fix_field(rdls);
			}
			rdlist_fix_header(rdls);
			rdlist_close(rdls);
		}

		free(tmparr);
	}
	free(fieldcenters);
	free(radecs);

	if (tplist) {
		for (i=0; i<Nbins; i++)
			dl_free(tplist[i]);
		free(tplist);
	}
	if (fplist) {
		for (i=0; i<Nbins; i++)
			dl_free(fplist[i]);
		free(fplist);
	}
	if (neglist) {
		for (i=0; i<Nbins; i++)
			dl_free(neglist[i]);
		free(neglist);
	}

	free(rightfieldbins);
	free(wrongfieldbins);
	free(negfieldbins);

	free(thresh_right);
	free(thresh_wrong);
	free(thresh_unsolved);

	histogram_free(overlap_hist_right);
	histogram_free(overlap_hist_wrong);

	return 0;
}

