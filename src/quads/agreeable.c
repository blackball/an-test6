/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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
#include "bl.h"
#include "matchobj.h"
#include "hitlist.h"
#include "matchfile.h"
#include "solvedclient.h"
#include "solvedfile.h"
#include "boilerplate.h"
//#include "handlehits.h"

char* OPTIONS = "hA:B:I:J:L:M:m:n:o:f:s:S:Fa";

#define DEFAULT_AGREE_TOL 10.0

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s [options]\n"
			"   [-A first-field]\n"
			"   [-B last-field]\n"
			"   [-I first-field-filenum]\n"
			"   [-J last-field-filenum]\n"
			"   [-L write-leftover-matches-file]\n"
			"   [-M write-successful-matches-file]\n"
			"   [-m agreement-tolerance-in-arcsec] (default %g)\n"
 			"   [-n matches-needed-to-agree] (default 1)\n"
			"   [-o overlap-needed-to-solve]\n"
			"   [-f minimum-field-objects-needed-to-solve] (default: no minimum)\n"
			"   (      [-F]: write out the first sufficient match to surpass the solve threshold.\n"
			"     or   [-a]: write out all matches passing the solve threshold.\n"
			"          (default is to write out the single best match (largest overlap))\n"
			"   )\n"
			"   [-s <solved-server-address>]\n"
			"   [-S <solved-file-template>]\n"
			"   <input-match-file> ...\n"
			"\n", progname, DEFAULT_AGREE_TOL);
}

static void write_field(pl* agreeing,
						pl* leftover,
						int fieldfile,
						int fieldnum);

unsigned int min_matches_to_agree = 1;
char* leftoverfname = NULL;
matchfile* leftovermf = NULL;
char* agreefname = NULL;
matchfile* agreemf = NULL;
il* solved;
il* unsolved;
char* solvedserver = NULL;
char* solvedfile = NULL;
double overlap_tokeep = 0.0;
int ninfield_tokeep = 0;

extern char *optarg;
extern int optind, opterr, optopt;

enum modes {
	MODE_BEST,
	MODE_FIRST,
	MODE_ALL
};

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	int firstfield=0, lastfield=INT_MAX-1;
	int firstfieldfile=1, lastfieldfile=INT_MAX-1;
	bool leftovers = FALSE;
	bool agree = FALSE;
	double agreetolarcsec = DEFAULT_AGREE_TOL;
	matchfile** mfs;
	MatchObj** mos;
	bool* eofs;
	bool* eofieldfile;
	int nread = 0;
	int f;
	handlehits* hits = NULL;
	int fieldfile;
	int totalsolved, totalunsolved;
	int mode = MODE_BEST;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'S':
			solvedfile = optarg;
			break;
		case 's':
			solvedserver = optarg;
			break;
		case 'F':
			mode = MODE_FIRST;
			break;
		case 'a':
			mode = MODE_ALL;
			break;
		case 'm':
			agreetolarcsec = atof(optarg);
			break;
		case 'o':
			overlap_tokeep = atof(optarg);
			break;
		case 'f':
			ninfield_tokeep = atoi(optarg);
			break;
		case 'M':
			agreefname = optarg;
			break;
		case 'L':
			leftoverfname = optarg;
			break;
		case 'I':
			firstfieldfile = atoi(optarg);
			break;
		case 'J':
			lastfieldfile = atoi(optarg);
			break;
		case 'A':
			firstfield = atoi(optarg);
			break;
		case 'B':
			lastfield = atoi(optarg);
			break;
		case 'n':
			min_matches_to_agree = atoi(optarg);
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = argv + optind;
	} else {
		printHelp(progname);
		exit(-1);
	}

	if (lastfield < firstfield) {
		fprintf(stderr, "Last field (-B) must be at least as big as first field (-A)\n");
		exit(-1);
	}

	if (solvedserver)
		if (solvedclient_set_server(solvedserver)) {
			fprintf(stderr, "Failed to set solved server.\n");
			exit(-1);
		}

	if (leftoverfname) {
		leftovermf = matchfile_open_for_writing(leftoverfname);
		if (!leftovermf) {
			fprintf(stderr, "Failed to open file %s to write leftover matches.\n", leftoverfname);
			exit(-1);
		}
		boilerplate_add_fits_headers(leftovermf->header);
		qfits_header_add(leftovermf->header, "HISTORY", "This file was created by the program \"agreeable\".", NULL, NULL);
		if (matchfile_write_header(leftovermf)) {
			fprintf(stderr, "Failed to write leftovers matchfile header.\n");
			exit(-1);
		}
		leftovers = TRUE;
	}
	if (agreefname) {
		agreemf = matchfile_open_for_writing(agreefname);
		if (!agreemf) {
			fprintf(stderr, "Failed to open file %s to write agreeing matches.\n", agreefname);
			exit(-1);
		}
		boilerplate_add_fits_headers(agreemf->header);
		qfits_header_add(agreemf->header, "HISTORY", "This file was created by the program \"agreeable\".", NULL, NULL);
		if (matchfile_write_header(agreemf)) {
			fprintf(stderr, "Failed to write agreeing matchfile header.\n");
			exit(-1);
		}
		agree = TRUE;
	}

	hits = handlehits_new();
	hits->agreetol = agreetolarcsec;
	hits->nagree_toverify = min_matches_to_agree;
	hits->overlap_tokeep  = overlap_tokeep;
	hits->overlap_tosolve = overlap_tokeep;
	hits->ninfield_tokeep  = ninfield_tokeep;
	hits->ninfield_tosolve = ninfield_tokeep;

	solved = il_new(256);
	unsolved = il_new(256);

	totalsolved = totalunsolved = 0;

	mos =  calloc(ninputfiles, sizeof(MatchObj*));
	eofs = calloc(ninputfiles, sizeof(bool));
	eofieldfile = malloc(ninputfiles * sizeof(bool));
	mfs = malloc(ninputfiles * sizeof(matchfile*));

	for (i=0; i<ninputfiles; i++) {
		fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
		mfs[i] = matchfile_open(inputfiles[i]);
		if (!mfs[i]) {
			fprintf(stderr, "Failed to open matchfile %s.\n", inputfiles[i]);
			exit(-1);
		}
	}

	// we assume the matchfiles are sorted by field id and number.
	for (fieldfile=firstfieldfile; fieldfile<=lastfieldfile; fieldfile++) {
		bool alldone = TRUE;

		memset(eofieldfile, 0, ninputfiles * sizeof(bool));

		for (f=firstfield; f<=lastfield; f++) {
			int fieldnum = f;
			bool donefieldfile;
			bool solved_it;
			bl* allmatches;
			pl* writematches = NULL;
			pl* leftovermatches = NULL;

			// quit if we've reached the end of all the input files.
			alldone = TRUE;
			for (i=0; i<ninputfiles; i++)
				if (!eofs[i]) {
					alldone = FALSE;
					break;
				}
			if (alldone)
				break;

			// move on to the next fieldfile if all the input files have been
			// exhausted.
			donefieldfile = TRUE;
			for (i=0; i<ninputfiles; i++)
				if (!eofieldfile[i] && !eofs[i]) {
					donefieldfile = FALSE;
					break;
				}
			if (donefieldfile)
				break;

			// start a new field.
			fprintf(stderr, "File %i, Field %i.\n", fieldfile, f);
			solved_it = FALSE;
			allmatches = bl_new(256, sizeof(MatchObj));

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

					if (mos[i]->fieldfile > fieldfile)
						eofieldfile[i] = TRUE;

					if (mos[i]->fieldfile != fieldfile)
						break;

					assert(mos[i]->fieldnum >= fieldnum);
					if (mos[i]->fieldnum != fieldnum)
						break;
					nread++;
					if (nread % 10000 == 9999) {
						fprintf(stderr, ".");
						fflush(stderr);
					}

					// if we've already found a solution, skip past the
					// remaining matches in this file...
					if (solved_it && (mode == MODE_FIRST)) {
						ns++;
						mos[i] = NULL;
						continue;
					}

					nr++;

					// make a copy of this MatchObj by adding it to our
					// list, and get a pointer to its copied location.
					mos[i] = bl_append(allmatches, mos[i]);

					solved_it = handlehits_add(hits, mos[i]);
					mos[i] = NULL;

				}
				if (nr || ns)
					fprintf(stderr, "File %s: read %i matches, skipped %i matches.\n", inputfiles[i], nr, ns);
			}

			// which matches do we want to write out?
			if (agree) {
				writematches = pl_new(32);

				switch (mode) {
				case MODE_BEST:
				case MODE_FIRST:
					if (hits->bestmo)
						pl_append(writematches, hits->bestmo);
					break;
				case MODE_ALL:
					pl_merge_lists(writematches, hits->keepers);
					break;
				}
			}

			if (leftovers) {
				int i;
				/* Hack - steal the list directly from the hitlist...
				   so much for information hiding! */
				leftovermatches = hits->hits->matchlist;
				hits->hits->matchlist = NULL;
				// remove all the "successful" matches.
				for (i=0; writematches && i<pl_size(writematches); i++)
					pl_remove_value(leftovermatches,
									pl_get(writematches, i));
			}

			write_field(writematches, leftovermatches, fieldfile, fieldnum);

			if (leftovers)
				pl_free(leftovermatches);
			if (agree)
				pl_free(writematches);

			handlehits_clear(hits);
			bl_free(allmatches);

			fprintf(stderr, "This file: %i fields solved, %i unsolved.\n", il_size(solved), il_size(unsolved));
			fprintf(stderr, "Grand total: %i solved, %i unsolved.\n", totalsolved + il_size(solved), totalunsolved + il_size(unsolved));
		}
		totalsolved += il_size(solved);
		totalunsolved += il_size(unsolved);
		
		il_remove_all(solved);
		il_remove_all(unsolved);

		if (alldone)
			break;
	}

	for (i=0; i<ninputfiles; i++)
		matchfile_close(mfs[i]);
	free(mfs);
	free(mos);
	free(eofs);

	fprintf(stderr, "\nRead %i matches.\n", nread);
	fflush(stderr);

	handlehits_free(hits);

	il_free(solved);
	il_free(unsolved);

	if (leftovermf) {
		matchfile_fix_header(leftovermf);
		matchfile_close(leftovermf);
	}

	if (agreemf) {
		matchfile_fix_header(agreemf);
		matchfile_close(agreemf);
	}

	return 0;
}

static void write_field(pl* agreeing,
						pl* leftover,
						int fieldfile,
						int fieldnum) {
	int i;

	if (!pl_size(agreeing))
		il_append(unsolved, fieldnum);
	else {
		il_append(solved, fieldnum);
		if (solvedserver)
			solvedclient_set(fieldfile, fieldnum);
		if (solvedfile) {
			char fn[256];
			sprintf(fn, solvedfile, fieldfile);
			solvedfile_set(fn, fieldnum);
		}
	}

	for (i=0; agreeing && i<pl_size(agreeing); i++) {
		MatchObj* mo = pl_get(agreeing, i);
		if (matchfile_write_match(agreemf, mo))
			fprintf(stderr, "Error writing an agreeing match.");
		fprintf(stderr, "Field %i: Overlap %f\n", fieldnum, mo->overlap);
	}

	if (leftover && pl_size(leftover)) {
		fprintf(stderr, "Field %i: writing %i leftovers...\n", fieldnum,
				pl_size(leftover));
		for (i=0; i<pl_size(leftover); i++) {
			MatchObj* mo = pl_get(leftover, i);
			if (matchfile_write_match(leftovermf, mo))
				fprintf(stderr, "Error writing a leftover match.");
		}
	}
}


// HACK - we need a stub for this function because it is linked by
//   handlehits -> blind_wcs
void getstarcoord(uint iA, double *star) {
	fprintf(stderr, "ERROR: getstarcoord() called in agreeable.\n");
	exit(-1);
}
