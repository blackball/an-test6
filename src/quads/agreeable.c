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
#include "hitsfile.h"
#include "hitlist_healpix.h"
#include "matchfile.h"

char* OPTIONS = "hH:n:A:B:L:M:m:o:f:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]\n"
			"   [-B last-field]\n"
			"   [-H hits-file]\n"
			"   [-L write-leftover-matches-file]\n"
			"   [-M write-successful-matches-file]\n"
			"   [-m agreement-tolerance-in-arcsec]\n"
 			"   [-n matches_needed_to_agree]\n"
			"   [-o overlap_needed_to_solve]\n"
			"   [-f minimum-field-objects-needed-to-solve]\n"
			"\nIf filename FLUSH is specified, agreeing matches will"
			" be written out.\n",
			progname);
}

int find_correspondences(pl* hits, uint* starids, uint* fieldids, int* p_ok);

void write_field(hitlist* hl,
				 int fieldnum,
				 bool doleftovers,
				 bool doagree,
				 bool unsolvedstubs);

#define DEFAULT_MIN_MATCHES_TO_AGREE 3
#define DEFAULT_AGREE_TOL 7.0

unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;

FILE *hitfid = NULL;
char *hitfname = NULL;
char* leftoverfname = NULL;
matchfile* leftovermf = NULL;
char* agreefname = NULL;
matchfile* agreemf = NULL;
il* solved;
il* unsolved;

double overlap_needed = 0.0;
int min_ninfield = 0;

int* agreehist = NULL;
int  sizeagreehist = 0;

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	hits_header hitshdr;
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	int firstfield=0, lastfield=INT_MAX;
	bool leftovers = FALSE;
	bool agree = FALSE;
	//double ramin, ramax, decmin, decmax;
	double agreetolarcsec = DEFAULT_AGREE_TOL;
	matchfile** mfs;
	MatchObj** mos;
	bool* eofs;
	int nread = 0;
	int f;
	hitlist* hl = NULL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'm':
			agreetolarcsec = atof(optarg);
			break;
			/*
			  case 'r':
			  ramin = atof(optarg);
			  break;
			  case 'R':
			  ramax = atof(optarg);
			  break;
			  case 'd':
			  decmin = atof(optarg);
			  break;
			  case 'D':
			  decmax = atof(optarg);
			  break;
			*/
		case 'o':
			overlap_needed = atof(optarg);
			break;
		case 'f':
			min_ninfield = atoi(optarg);
			break;
		case 'M':
			agreefname = optarg;
			break;
		case 'L':
			leftoverfname = optarg;
			break;
		case 'A':
			firstfield = atoi(optarg);
			break;
		case 'B':
			lastfield = atoi(optarg);
			break;
		case 'H':
			hitfname = optarg;
			break;
		case 'n':
			min_matches_to_agree = atoi(optarg);
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

	if (hitfname) {
		fopenout(hitfname, hitfid);
	} else {
		hitfid = stdout;
	}

	if (leftoverfname) {
		leftovermf = matchfile_open_for_writing(leftoverfname);
		if (!leftovermf) {
			fprintf(stderr, "Failed to open file %s to write leftover matches.\n", leftoverfname);
			exit(-1);
		}
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
		if (matchfile_write_header(agreemf)) {
			fprintf(stderr, "Failed to write agreeing matchfile header.\n");
			exit(-1);
		}
		agree = TRUE;
	}

	solved = il_new(256);
	unsolved = il_new(256);

	// write HITS header.
	hits_header_init(&hitshdr);
	hitshdr.min_matches_to_agree = min_matches_to_agree;
	hits_write_header(hitfid, &hitshdr);

	mfs = malloc(ninputfiles * sizeof(matchfile*));
	mos =  calloc(ninputfiles, sizeof(MatchObj*));
	eofs = calloc(ninputfiles, sizeof(bool));

	hl = hitlist_healpix_new(agreetolarcsec);

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
		bool alldone = TRUE;
		int fieldnum = f;

		for (i=0; i<ninputfiles; i++)
			if (!eofs[i])
				alldone = FALSE;
		if (alldone)
			break;

		fprintf(stderr, "Field %i.\n", f);

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
				assert(mos[i]->fieldnum >= fieldnum);
				if (mos[i]->fieldnum != fieldnum)
					break;
				nread++;
				nr++;

				if (nread % 10000 == 9999) {
					fprintf(stderr, ".");
					fflush(stderr);
				}

				mocopy = malloc(sizeof(MatchObj));
				memcpy(mocopy, mos[i], sizeof(MatchObj));

				// compute (x,y,z) center, scale, rotation.
				hitlist_healpix_compute_vector(mocopy);
				// add the match...
				hitlist_healpix_add_hit(hl, mocopy, NULL);

				mos[i] = NULL;
			}
			fprintf(stderr, "File %s: read %i matches.\n", inputfiles[i], nr);
		}

		write_field(hl, fieldnum, leftovers, agree, TRUE);

		fprintf(stderr, "So far: %i fields solved, %i unsolved.\n", il_size(solved), il_size(unsolved));

		hitlist_healpix_clear(hl);
	}
	fprintf(stderr, "\nRead %i matches.\n", nread);
	fflush(stderr);

	hitlist_healpix_free(hl);

	for (i=0; i<ninputfiles; i++) {
		if (!mfs[i])
			continue;
		matchfile_close(mfs[i]);
	}

	il_free(solved);
	il_free(unsolved);

	// finish up HITS file...
	hits_write_tailer(hitfid);
	if (hitfname)
		fclose(hitfid);

	if (leftovermf) {
		matchfile_fix_header(leftovermf);
		matchfile_close(leftovermf);
	}

	if (agreemf) {
		matchfile_fix_header(agreemf);
		matchfile_close(agreemf);
	}

	fprintf(stderr, "Number of agreeing quads histogram:\n  [ ");
	for (i=0; i<sizeagreehist; i++) {
		fprintf(stderr, "%i, ", agreehist[i]);
	}
	fprintf(stderr, "]\n");
	free(agreehist);

	free(mfs);
	free(mos);
	free(eofs);
	return 0;
}

void write_field(hitlist* hl,
				 int fieldnum,
				 bool doleftovers,
				 bool doagree,
				 bool unsolvedstubs) {
	pl* best = NULL;
	hits_field fieldhdr;
	int j;
	uint* starids;
	uint* fieldids;
	int correspond_ok = 1;
	int Ncorrespond;
	bool issolved = FALSE;
	int nbest;

	nbest = hitlist_healpix_count_best(hl);

	hits_field_init(&fieldhdr);
	fieldhdr.field = fieldnum;
	fieldhdr.nmatches = hitlist_healpix_count_all(hl);
	fieldhdr.nagree = nbest;

	if (overlap_needed > 0.0) {
		int nlists = hitlist_healpix_count_lists(hl);
		for (j=0; j<nlists; j++) {
			int i;
			bool gotit = FALSE;
			pl* list = hitlist_healpix_copy_list(hl, j);
			if (!list)
				continue;
			for (i=0; i<pl_size(list); i++) {
				MatchObj* mo = pl_get(list, i);
				if ((mo->overlap >= overlap_needed) &&
					(mo->ninfield > min_ninfield)) {
					gotit = TRUE;
					break;
				}
			}
			if (gotit) {
				if (!best)
					best = pl_new(32);
				pl_merge_lists(best, list);
			}
			pl_free(list);
		}
		issolved = (best != NULL);
	} else {
		if (nbest < min_matches_to_agree) {
			issolved = FALSE;
		} else {
			issolved = TRUE;
			fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);
			best = hitlist_healpix_get_best(hl);
			//best = hitlist_healpix_get_all_best(hl);
			//best = hitlist_get_all_above_size(hl, min_matches_to_agree);
		}
	}

	if (!issolved) {
		il_append(unsolved, fieldnum);
		if (unsolvedstubs) {
			fieldhdr.failed = TRUE;
			hits_write_field_header(hitfid, &fieldhdr);
			hits_start_hits_list(hitfid);
			hits_end_hits_list(hitfid);
			hits_write_field_tailer(hitfid);
			fflush(hitfid);
		}
		if (doleftovers) {
			int NA;
			pl* all;
			all = hitlist_healpix_get_all(hl);
			NA = pl_size(all);
			fprintf(stderr, "Field %i: writing %i leftovers...\n", fieldnum, NA);
			for (j=0; j<NA; j++) {
				MatchObj* mo = pl_get(all, j);
				if (matchfile_write_match(leftovermf, mo)) {
					fprintf(stderr, "Error writing a leftover match.\n");
					break;
				}
			}
			pl_free(all);
		}
		return;
	}

	// solved field.
	il_append(solved, fieldnum);

	if ((nbest+1) > sizeagreehist) {
		agreehist = realloc(agreehist, (nbest+1) * sizeof(int));
		memset(agreehist + sizeagreehist, 0, ((nbest+1) - sizeagreehist) * sizeof(int));
		sizeagreehist = nbest + 1;
	}
	agreehist[nbest]++;

	hits_write_field_header(hitfid, &fieldhdr);
	hits_start_hits_list(hitfid);

	for (j=0; j<pl_size(best); j++) {
		MatchObj* mo = pl_get(best, j);
		hits_write_hit(hitfid, mo);
		if (doagree) {
			if (matchfile_write_match(agreemf, mo)) {
				fprintf(stderr, "Error writing an agreeing match.");
			}
		}
		if (overlap_needed > 0.0)
			fprintf(stderr, "Overlap %f\n", mo->overlap);
	}

	hits_end_hits_list(hitfid);

	nbest = pl_size(best);
	starids  = malloc(nbest * 4 * sizeof(uint));
	fieldids = malloc(nbest * 4 * sizeof(uint));
	Ncorrespond = find_correspondences(best, starids, fieldids, &correspond_ok);
	hits_write_correspondences(hitfid, starids, fieldids, Ncorrespond, correspond_ok);
	free(starids);
	free(fieldids);
	hits_write_field_tailer(hitfid);
	fflush(hitfid);
	pl_free(best);
}

inline void add_correspondence(uint* starids, uint* fieldids,
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
