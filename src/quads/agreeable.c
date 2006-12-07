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
#include "handlehits.h"

char* OPTIONS = "hn:A:B:L:M:m:o:f:bFs:I:J:S:at";

/*
  There is one weird case in which "agreeable" does not let you replay
  exactly what happened when "blind" ran.

  Assume that there is a match that produces overlap insufficient to pass
  the "keep" thresholds.  Assume that a correct match agrees with this one
  and that there are enough agreeing matches to put it over the "solve"
  threshold.  The second match will be written out, but the first one will
  not.
 */

void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s [options]\n"
			"   [-A first-field]\n"
			"   [-B last-field]\n"
			"   [-I first-field-filenum]\n"
			"   [-J last-field-filenum]\n"
			"   [-L write-leftover-matches-file]\n"
			"   [-M write-successful-matches-file]\n"
			"   [-m agreement-tolerance-in-arcsec]\n"
 			"   [-n matches-needed-to-agree]\n"
			"   [-o overlap-needed-to-solve]\n"
			"   [-f minimum-field-objects-needed-to-solve]\n"
			"   (      [-b]: best-overlap mode\n"
			"     or   [-F]: first-solved mode (smallest number of field objs)\n"
			"     or   [-a]: agreement mode (waits until enough agreeing matches have been found,\n"
			"                         prints the last one if it is above the overlap thresh,\n"
			"                         otherwise prints the last one but steals the 'overlap'\n"
			"                         from the largest-overlap match)\n"
			"   )\n"
			"   [-t]: print agreement distances.\n"
			"   [-s <solved-server-address>]\n"
			"   [-S <solved-file-template>]\n"
			"   <input-match-file> ...\n"
			"\n", progname);
}

static void write_field(handlehits* hits,
						int fieldfile,
						int fieldnum,
						bool doleftovers,
						bool doagree,
						bool unsolvedstubs);

#define DEFAULT_AGREE_TOL 10.0

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

bool print_agree = FALSE;
dl* agreedistlist;

static int compare_objs_used(const void* v1, const void* v2) {
	const MatchObj* mo1 = v1;
	const MatchObj* mo2 = v2;
	// this is backward so the list is sorted in INCREASING order of objs_tried.
	int diff = mo1->objs_tried - mo2->objs_tried;
	if (diff > 0)
		return 1;
	if (diff == 0)
		return 0;
	return -1;
}

extern char *optarg;
extern int optind, opterr, optopt;

enum modes {
	MODE_BEST,
	MODE_FIRST,
	MODE_AGREE,
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
			/*
			  case 'b':
			  mode = MODE_BEST;
			  break;
			*/
		case 'F':
			mode = MODE_FIRST;
			break;
		case 'a':
			mode = MODE_AGREE;
			break;
		case 't':
			print_agree = TRUE;
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

	if (print_agree) {
		agreedistlist = dl_new(256);
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
	//hits->verify_dist2 = verify_dist2;
	//hits->do_corr = 

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
			bool solvedit;
			bl* allmatches;

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

			fprintf(stderr, "File %i, Field %i.\n", fieldfile, f);
			solvedit = FALSE;
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
					if (solvedit && (mode == MODE_AGREE)) {
						ns++;
						continue;
					}

					nr++;

					bl_append(allmatches, mos[i]);
					mos[i] = bl_access(allmatches, bl_size(allmatches)-1);

					solvedit = handlehits_add(hits, mos[i]);
					mos[i] = NULL;

					/*
					  if (solvedit && (mode == MODE_AGREE)) {
					  break;
					  }
					*/
				}
				if (nr || ns)
					fprintf(stderr, "File %s: read %i matches, skipped %i matches.\n", inputfiles[i], nr, ns);
			}

			write_field(hits, fieldfile, fieldnum, leftovers, agree, TRUE);
			handlehits_clear(hits);
			//bl_remove_all_but_first(allmatches);
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

	if (print_agree) {
		printf("agree_dists = [");
		for (i=0; i<dl_size(agreedistlist); i++)
			printf("%g, ", dl_get(agreedistlist, i));
		printf("];\n");
		dl_free(agreedistlist);
	}

	return 0;
}

static void write_field(handlehits* hits,
						int fieldfile,
						int fieldnum,
						bool doleftovers,
						bool doagree,
						bool unsolvedstubs) {
	pl* best = NULL;
	int j;
	bool issolved = FALSE;
	int nbest;
	int nall;

	assert(hits || overlaps);

	if (hits) {
		/*
		  nbest = hitlist_healpix_count_best(hl);
		  nall = hitlist_healpix_count_all(hl);
		*/
	} else {
		nall = pl_size(overlaps);
		nbest = (nall ? 1 : 0);
	}

	if (overlap_tokeep > 0.0) {
		pl* lists = pl_new(32);
		if (hits) {
			/*
			  int nlists = hitlist_healpix_count_lists(hl);
			  for (j=0; j<nlists; j++) {
			  pl* list = hitlist_healpix_copy_list(hl, j);
			  if (!list)
			  continue;
			  pl_append(lists, list);
			  }
			*/
		} else {
			pl* copy = pl_dup(overlaps);
			pl_append(lists, copy);
		}

		for (j=0; j<pl_size(lists); j++) {
			int i;
			bool gotit = FALSE;
			pl* list = pl_get(lists, j);
			MatchObj* mo = NULL;
			MatchObj* mbest = NULL;
			double bestoverlap = 0.0;

			for (i=0; i<pl_size(list); i++) {
				mo = pl_get(list, i);

				if (i && hits && print_agree) {
					double bestd2 = 1e300;
					int j;
					for (j=0; j<i; j++) {
						double d2 = -1.0;
						MatchObj* mo2 = pl_get(list, j);
						//hitlist_hits_agree(mo, mo2, hits->agreedist2, &d2);
						if (d2 < bestd2)
							bestd2 = d2;
					}
					if (bestd2 < 1e300)
						dl_append(agreedistlist, distsq2arcsec(bestd2));
				}

				if (do_agree_overlap) {
					// slightly strange rules, as a result of the way slave
					// combines agreement and overlap.  Once "nagree" hits
					// have accumulated, overlap is computed for each of them
					// and if one passes threshold, the whole cluster is
					// written out.

					// For analyzing the solver parameters, we want to know
					// how many objects we had to look at before this
					// happened, so we want stats for the first object that
					// has sufficient nagree and is part of a cluster that
					// has sufficient overlap.  HOWEVER, in order to not
					// screw up further analysis, we want the overlap of the
					// best solution.

					if ((mo->overlap >= bestoverlap) &&
						(mo->ninfield >= ninfield_tokeep)) {
						mbest = mo;
						bestoverlap = mo->overlap;
					}
					if ((i >= (min_matches_to_agree-1)) &&
						(bestoverlap >= overlap_tokeep)) {
						gotit = TRUE;
						break;
					}
					
				} else {
					if ((mo->overlap >= overlap_tokeep) &&
						(mo->ninfield >= ninfield_tokeep)) {
						gotit = TRUE;
						break;
					}
				}
			}
			if (gotit) {
				if (!best)
					best = pl_new(32);
				if (overlaps) {
					if (mbest) {
						mo->noverlap  = mbest->noverlap;
						mo->nconflict = mbest->nconflict;
						mo->ninfield  = mbest->ninfield;
						mo->overlap   = mbest->overlap;
					}
					// just take the first/best one.
					pl_append(best, mo);
				}
				else 
					// take the whole list.
					pl_merge_lists(best, list);
			}
			pl_free(list);
		}
		pl_free(lists);
		issolved = (best != NULL);

	} else {
		if (hits) {
			if (nbest < min_matches_to_agree) {
				issolved = FALSE;
			} else {
				issolved = TRUE;
				fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);
				//best = hitlist_healpix_get_best(hl);
			}
		} else {
			issolved = (nbest ? TRUE : FALSE);
			if (issolved) {
				best = pl_new(32);
				pl_append(best, pl_get(overlaps, 0));
			}
		}
	}

	if (!issolved) {
		il_append(unsolved, fieldnum);
		if (doleftovers) {
			int NA;
			pl* all;
			if (hits) {
				//all = hitlist_healpix_get_all(hl);
			} else {
				all = overlaps;
			}
			NA = pl_size(all);
			fprintf(stderr, "Field %i: writing %i leftovers...\n", fieldnum, NA);
			for (j=0; j<NA; j++) {
				MatchObj* mo = pl_get(all, j);
				if (matchfile_write_match(leftovermf, mo)) {
					fprintf(stderr, "Error writing a leftover match.\n");
					break;
				}
			}
			if (hits)
				pl_free(all);
		}
		return;
	}

	// solved field.
	il_append(solved, fieldnum);

	if (solvedserver)
		solvedclient_set(fieldfile, fieldnum);
	if (solvedfile) {
		char fn[256];
		sprintf(fn, solvedfile, fieldfile);
		solvedfile_set(fn, fieldnum);
	}

	for (j=0; j<pl_size(best); j++) {
		MatchObj* mo = pl_get(best, j);
		if (doagree) {
			if (matchfile_write_match(agreemf, mo)) {
				fprintf(stderr, "Error writing an agreeing match.");
			}
		}
		if (overlap_tokeep > 0.0)
			fprintf(stderr, "Field %i: Overlap %f\n", fieldnum, mo->overlap);
	}

	pl_free(best);
}

