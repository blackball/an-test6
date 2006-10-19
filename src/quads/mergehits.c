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

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "hitlist_healpix.h"
#include "matchfile.h"

char* OPTIONS = "hA:B:M:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]  (default 0)\n"
			"   [-B last-field]\n"
			"   <-M output-file>\n",
			progname);
}

void write_field(pl* hits,
				 int fieldnum,
				 bool unsolvedstubs);

char* agreefname = NULL;
matchfile* agreemf = NULL;
il* empty;
il* nonempty;

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	int firstfield=0, lastfield=INT_MAX;
	matchfile** mfs;
	MatchObj** mos;
	bool* eofs;
	int nread = 0;
	int f;
	pl* hits;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'M':
			agreefname = optarg;
			break;
		case 'A':
			firstfield = atoi(optarg);
			break;
		case 'B':
			lastfield = atoi(optarg);
			break;
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
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

	if (lastfield < firstfield) {
		fprintf(stderr, "Last field (-B) must be at least as big as first field (-A)\n");
		exit(-1);
	}

	if (!agreefname) {
		printHelp(progname);
		exit(-1);
	}

	agreemf = matchfile_open_for_writing(agreefname);
	if (!agreemf) {
		fprintf(stderr, "Failed to open file %s to write agreeing matches.\n", agreefname);
		exit(-1);
	}
	if (matchfile_write_header(agreemf)) {
		fprintf(stderr, "Failed to write agreeing matchfile header.\n");
		exit(-1);
	}

	empty = il_new(256);
	nonempty = il_new(256);

	mfs = malloc(ninputfiles * sizeof(matchfile*));
	mos =  calloc(ninputfiles, sizeof(MatchObj*));
	eofs = calloc(ninputfiles, sizeof(bool));

	hits = pl_new(256);

	for (i=0; i<ninputfiles; i++) {
		fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
		mfs[i] = matchfile_open(inputfiles[i]);
		if (!mfs[i]) {
			fprintf(stderr, "Failed to open matchfile %s.\n", inputfiles[i]);
			exit(-1);
		}
	}
	// we assume the matchfiles are sorted by field number.
	for (f=firstfield; f<=lastfield; f++) {
		int fieldnum = f;
		fprintf(stderr, "Field %i.\n", f);

		if (lastfield == INT_MAX) {
			bool alldone = TRUE;
			for (i=0; i<ninputfiles; i++) {
				if (!eofs[i]) {
					alldone = FALSE;
					break;
				}
			}
			if (alldone)
				break;
		}

		for (i=0; i<ninputfiles; i++) {
			MatchObj* mocopy;
			int nr = 0;

			while (1) {
				if (eofs[i])
					break;
				if (!mos[i])
					mos[i] = matchfile_buffered_read_match(mfs[i]);
				if (!mos[i]) {
					eofs[i] = TRUE;
					continue;
				}
				//fprintf(stderr, " File %s: current table is field %i.\n", fname, mes[i].fieldnum);

				assert(mos[i]->fieldnum >= fieldnum);

				if (mos[i]->fieldnum != fieldnum)
					break;

				/*
				  fprintf(stderr, "fieldnum %i, parity %i, index %s, field %s, codetol %g\n",
				  mes[i].fieldnum, (int)mes[i].parity, mes[i].indexpath,
				  mes[i].fieldpath, mes[i].codetol);
				*/

				nread++;
				nr++;

				mocopy = malloc(sizeof(MatchObj));
				memcpy(mocopy, mos[i], sizeof(MatchObj));
				pl_append(hits, mocopy);

				mos[i] = NULL;
			}
			fprintf(stderr, "File %s: read %i matches.\n", inputfiles[i], nr);
		}

		write_field(hits, fieldnum, TRUE);
		pl_remove_all(hits);
	}
	fprintf(stderr, "\nRead %i matches.\n", nread);
	fflush(stderr);

	pl_free(hits);

	for (i=0; i<ninputfiles; i++) {
		if (!mfs[i])
			continue;
		matchfile_close(mfs[i]);
	}

	fprintf(stderr, "%i fields empty; %i field nonempty.\n",
			il_size(empty), il_size(nonempty));

	fprintf(stderr, "empty=[ ");
	for (i=0; i<il_size(empty); i++)
		fprintf(stderr, "%i, ", il_get(empty, i));
	fprintf(stderr, "];\n");

	fprintf(stderr, "nonempty=[ ");
	for (i=0; i<il_size(nonempty); i++)
		fprintf(stderr, "%i, ", il_get(nonempty, i));
	fprintf(stderr, "];\n");

	il_free(empty);
	il_free(nonempty);

	matchfile_fix_header(agreemf);
	matchfile_close(agreemf);

	free(mfs);
	free(mos);
	free(eofs);

	return 0;
}

int find_correspondences(pl* hits, uint* starids, uint* fieldids, int* p_ok);

void write_field(pl* hits,
				 int fieldnum,
				 bool unsolvedstubs) {
	int j;

	if (!pl_size(hits)) {
		il_append(empty, fieldnum);
		return;
	}

	fprintf(stderr, "Field %i: writing %i hits.\n", fieldnum, pl_size(hits));

	il_append(nonempty, fieldnum);

	for (j=0; j<pl_size(hits); j++) {
		MatchObj* mo = pl_get(hits, j);
		if (matchfile_write_match(agreemf, mo)) {
			fprintf(stderr, "Error writing a match.");
		}
	}
}

void add_correspondence(uint* starids, uint* fieldids,
						uint starid, uint fieldid,
						int* p_nids, int* p_ok) {
	int i;
	int ok = 1;
	for (i=0; i<(*p_nids); i++) {
		if ((starids[i] == starid) &&
			(fieldids[i] == fieldid)) {
			return;
		} else if ((starids[i] == starid) ||
				   (fieldids[i] == fieldid)) {
			ok = 0;
		}
	}
	starids[*p_nids] = starid;
	fieldids[*p_nids] = fieldid;
	(*p_nids)++;
	if (p_ok && !ok) *p_ok = 0;
}

int find_correspondences(pl* hits, uint* starids, uint* fieldids,
						 int* p_ok) {
	int i, N;
	int M;
	int ok = 1;
	MatchObj* mo;
	if (!hits) return 0;
	N = pl_size(hits);
	M = 0;
	for (i=0; i<N; i++) {
		mo = (MatchObj*)pl_get(hits, i);
		int k;
		for (k=0; k<4; k++)
			add_correspondence(starids, fieldids, mo->star[k], mo->field[k], &M, &ok);
	}
	if (p_ok && !ok) *p_ok = 0;
	return M;
}
