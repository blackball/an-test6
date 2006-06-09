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

char* OPTIONS = "hH:n:A:B:F:L:M:f:m:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]\n"
			"   [-B last-field]\n"
			"   [-H hits-file]\n"
			"   [-F flush-interval]\n"
			"   [-f flush-field-interval]\n"
			"   [-L write-leftover-matches-file]\n"
			"   [-M write-successful-matches-file]\n"
			"   [-m agreement-tolerance-in-arcsec]\n"
 			"   [-n matches_needed_to_agree]\n"
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
pl* hitlists;
il* solved;
il* unsolved;

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
	int flushinterval = 0;
	int flushfieldinterval = 0;
	bool leftovers = FALSE;
	bool agree = FALSE;
	double ramin, ramax, decmin, decmax;
	double agreetolarcsec = DEFAULT_AGREE_TOL;
	matchfile** mfs;
	matchfile_entry* mes;
	bool* mes_valid;
	bool* eofs;
	int nread = 0;
	int f;
	hitlist* hl = NULL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'm':
			agreetolarcsec = atof(optarg);
			break;
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
		case 'f':
			flushfieldinterval = atoi(optarg);
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
		case 'F':
			flushinterval = atoi(optarg);
			break;
		case 'H':
			hitfname = optarg;
			break;
		case 'n':
			min_matches_to_agree = (unsigned int)atoi(optarg);
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
	hitshdr.nfields = 0;
	hitshdr.min_matches_to_agree = min_matches_to_agree;
	hits_write_header(hitfid, &hitshdr);

	mfs = malloc(ninputfiles * sizeof(matchfile*));
	mes = malloc(ninputfiles * sizeof(matchfile_entry));
	mes_valid = calloc(ninputfiles, sizeof(bool));
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
			MatchObj* mo;
			MatchObj* mocopy;
			matchfile_entry* mecopy = NULL;
			int nr = 0;
			char* fname = inputfiles[i];
			int k;

			if (eofs[i])
				continue;

			if (!mes_valid[i]) {
				if (matchfile_next_table(mfs[i], mes + i)) {
					eofs[i] = TRUE;
					continue;
				}
				mes_valid[i] = TRUE;
			}

			assert(mes[i].fieldnum >= fieldnum);

			if (mes[i].fieldnum != fieldnum)
				continue;

			// LEAK
			if (leftovers || agree) {
				mecopy = malloc(sizeof(matchfile_entry));
				memcpy(mecopy, mes+i, sizeof(matchfile_entry));

				fprintf(stderr, "fieldnum %i, parity %i, index %s, field %s, codetol %g\n",
					   mecopy->fieldnum, (int)mecopy->parity, mecopy->indexpath,
					   mecopy->fieldpath, mecopy->codetol);
			}

			for (k=0; k<mfs[i]->nrows; k++) {
				mo = matchfile_buffered_read_match(mfs[i]);
				if (!mo) {
					fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
					break;
				}
				nread++;
				nr++;

				if (nread % 10000 == 9999) {
					fprintf(stderr, ".");
					fflush(stderr);
				}

				mocopy = malloc(sizeof(MatchObj));
				memcpy(mocopy, mo, sizeof(MatchObj));

				if (leftovers || agree) {
					mocopy->extra = mecopy;
				} else {
					mocopy->extra = NULL;
				}

				// compute (x,y,z) center, scale, rotation.
				hitlist_healpix_compute_vector(mocopy);
				// add the match...
				hitlist_healpix_add_hit(hl, mocopy, NULL);
			}
			fprintf(stderr, "File %s: read %i matches.\n", inputfiles[i], nr);

			if (!(leftovers || agree)) {
				free(mes[i].indexpath);
				free(mes[i].fieldpath);
			}

			// we're done with this table...
			mes_valid[i] = FALSE;
		}

		write_field(hl, fieldnum, leftovers, agree, TRUE);

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

	if (leftovermf)
		matchfile_close(leftovermf);

	if (agreemf)
		matchfile_close(agreemf);

	fprintf(stderr, "Number of agreeing quads histogram:\n  [ ");
	for (i=0; i<sizeagreehist; i++) {
		fprintf(stderr, "%i, ", agreehist[i]);
	}
	fprintf(stderr, "]\n");

	free(mfs);
	free(mes);
	free(mes_valid);
	free(eofs);

	return 0;
}

void free_extra(MatchObj* mo) {
	matchfile_entry* me;
	if (!mo->extra) return;
	me = (matchfile_entry*)mo->extra;
	/*
	  free(me->indexpath);
	  free(me->fieldpath);
	  free(me);
	*/
	mo->extra = NULL;
}

int qsort_matchobj_me(const void* v1, const void* v2) {
	const MatchObj* mo1 = v1;
	const MatchObj* mo2 = v2;
	if (mo1->extra == mo2->extra)
		return 0;
	if (mo1->extra > mo2->extra)
		return 1;
	return -1;
}

void write_field(hitlist* hl,
				 int fieldnum,
				 bool doleftovers,
				 bool doagree,
				 bool unsolvedstubs) {
	pl* best;
	hits_field fieldhdr;
	int nbest;
	int j;
	uint* starids;
	uint* fieldids;
	int correspond_ok = 1;
	int Ncorrespond;

	nbest = hitlist_healpix_count_best(hl);

	// HACK, this needs to allow fields to be solved by 'verification'

	if (nbest < min_matches_to_agree) {
		il_append(unsolved, fieldnum);

		if (unsolvedstubs) {
			hits_field_init(&fieldhdr);
			fieldhdr.field = fieldnum;
			fieldhdr.nmatches = hitlist_healpix_count_all(hl);
			fieldhdr.nagree = nbest;
			fieldhdr.failed = TRUE;
			hits_write_field_header(hitfid, &fieldhdr);
			hits_start_hits_list(hitfid);
			hits_end_hits_list(hitfid);
			hits_write_field_tailer(hitfid);
			fflush(hitfid);
		}
		if (doleftovers) {
			int j;
			int NA;
			matchfile_entry* me;
			pl* all = hitlist_healpix_get_all(hl);
			MatchObj* sorted;
			NA = pl_size(all);
			fprintf(stderr, "Field %i: writing %i leftovers...\n", fieldnum, NA);

			// sort the matches by their matchfile_entry so we can write
			// them in groups.
			sorted = malloc(NA * sizeof(MatchObj));
			for (j=0; j<NA; j++)
				memcpy(sorted + j, pl_get(all, j), sizeof(MatchObj));
			pl_free(all);

			qsort(sorted, NA, sizeof(MatchObj), qsort_matchobj_me);

			// write the leftovers...
			me = NULL;
			for (j=0; j<NA; j++) {
				if (sorted[j].extra != me) {
					if (me) {
						if (matchfile_fix_table(leftovermf)) {
							fprintf(stderr, "Failed to fix leftover matchfile header.\n");
						}
					}
					me = sorted[j].extra;
					if (matchfile_start_table(leftovermf, me) ||
						matchfile_write_table(leftovermf)) {
						fprintf(stderr, "Failed to write leftover matchfile header.\n");
					}
				}
				if (matchfile_write_match(leftovermf, sorted + j)) {
					fprintf(stderr, "Error writing a leftover match.\n");
					break;
				}
			}
			if (me)
				if (matchfile_fix_table(leftovermf)) {
					fprintf(stderr, "Failed to fix leftover matchfile header.\n");
				}

			free(sorted);
		}
		return;
	}
	fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);

	if ((nbest+1) > sizeagreehist) {
		agreehist = realloc(agreehist, (nbest+1) * sizeof(int));
		memset(agreehist + sizeagreehist, 0,
			   ((nbest+1) - sizeagreehist) * sizeof(int));
		sizeagreehist = nbest + 1;
	}
	agreehist[nbest]++;

	best = hitlist_healpix_get_best(hl);
	//best = hitlist_healpix_get_all_best(hl);
	//best = hitlist_get_all_above_size(hl, min_matches_to_agree);

	il_append(solved, fieldnum);

	hits_field_init(&fieldhdr);
	fieldhdr.field = fieldnum;
	fieldhdr.nmatches = hitlist_healpix_count_all(hl);
	fieldhdr.nagree = nbest;

	if (doagree) {
		matchfile_entry me;
		// HACK -
		memset(&me, 0, sizeof(matchfile_entry));
		me.fieldnum = fieldnum;
		if (matchfile_start_table(agreemf, &me) ||
			matchfile_write_table(agreemf)) {
			fprintf(stderr, "Failed to write agreeing matchfile header.\n");
		}
	}

	for (j=0; j<nbest; j++) {
		matchfile_entry* me;
		MatchObj* mo = (MatchObj*)pl_get(best, j);
		me = (matchfile_entry*)mo->extra;

		if (j == 0) {
			if (me)
				fieldhdr.fieldpath = me->fieldpath;
			hits_write_field_header(hitfid, &fieldhdr);
			hits_start_hits_list(hitfid);
		}

		hits_write_hit(hitfid, mo, me);

		if (doagree) {
			if (matchfile_write_match(agreemf, mo)) {
				fprintf(stderr, "Error writing an agreeing match.");
			}
		}
	}
	hits_end_hits_list(hitfid);

	if (doagree) {
		if (matchfile_fix_table(agreemf)) {
			fprintf(stderr, "Failed to fix agreeing matchfile header.\n");
		}
	}

	starids  = (uint*)malloc(nbest * 4 * sizeof(uint));
	fieldids = (uint*)malloc(nbest * 4 * sizeof(uint));
	Ncorrespond = find_correspondences(best, starids, fieldids, &correspond_ok);
	hits_write_correspondences(hitfid, starids, fieldids, Ncorrespond, correspond_ok);
	free(starids);
	free(fieldids);
	hits_write_field_tailer(hitfid);
	fflush(hitfid);
	pl_free(best);

	fprintf(stderr, "So far, %i fields have been solved.\n", il_size(solved));
	fflush(hitfid);
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
