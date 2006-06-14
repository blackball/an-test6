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

char* OPTIONS = "hH:A:B:F:L:M:f:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]  (default 0)\n"
			"   [-B last-field]\n"
			"   [-H hits-file]\n"
			"   <-M output-file>\n",
			progname);
}

void write_field(pl* hits,
				 int fieldnum,
				 bool unsolvedstubs);

FILE *hitfid = NULL;
char *hitfname = NULL;
char* agreefname = NULL;
matchfile* agreemf = NULL;
il* empty;
il* nonempty;

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
	matchfile** mfs;
	matchfile_entry* mes;
	bool* mes_valid;
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
		case 'H':
			hitfname = optarg;
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

	if (hitfname)
		fopenout(hitfname, hitfid);

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

	if (hitfid) {
		// write HITS header.
		hits_header_init(&hitshdr);
		hits_write_header(hitfid, &hitshdr);
	}

	mfs = malloc(ninputfiles * sizeof(matchfile*));
	mes = malloc(ninputfiles * sizeof(matchfile_entry));
	mes_valid = calloc(ninputfiles, sizeof(bool));
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
			MatchObj* mo;
			MatchObj* mocopy;
			int nr = 0;
			char* fname = inputfiles[i];
			int k;

			// allow an input matchfile to have multiple tables for a single
			// field (eg, if there are different settings).
			while (1) {
				if (eofs[i])
					break;
				if (!mes_valid[i]) {
					if (matchfile_next_table(mfs[i], mes + i)) {
						eofs[i] = TRUE;
						break;
					}
					mes_valid[i] = TRUE;
				}
				fprintf(stderr, " File %s: current table is field %i.\n", fname, mes[i].fieldnum);

				assert(mes[i].fieldnum >= fieldnum);

				if (mes[i].fieldnum != fieldnum)
					break;

				/*
				  fprintf(stderr, "fieldnum %i, parity %i, index %s, field %s, codetol %g\n",
				  mes[i].fieldnum, (int)mes[i].parity, mes[i].indexpath,
				  mes[i].fieldpath, mes[i].codetol);
				*/

				for (k=0; k<mfs[i]->nrows; k++) {
					mo = matchfile_buffered_read_match(mfs[i]);
					if (!mo) {
						fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
						break;
					}
					nread++;
					nr++;

					mocopy = malloc(sizeof(MatchObj));
					memcpy(mocopy, mo, sizeof(MatchObj));
					mocopy->extra = mes + i;

					pl_append(hits, mocopy);
				}
				fprintf(stderr, "File %s: read %i matches.\n", inputfiles[i], nr);

				mes_valid[i] = FALSE;
			}

		}

		write_field(hits, fieldnum, TRUE);

		// HACK - this isn't quite right - we can miss tables since we allow
		// multiple tables for each field.
		for (i=0; i<ninputfiles; i++) {
			if (eofs[i] || !mes_valid[i] || mes[i].fieldnum != fieldnum)
				continue;
			// we're done with these tables...
			free(mes[i].indexpath);
			free(mes[i].fieldpath);
		}

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

	if (hitfid) {
		// finish up HITS file...
		hits_write_tailer(hitfid);
		fclose(hitfid);
	}

	matchfile_close(agreemf);

	free(mfs);
	free(mes);
	free(mes_valid);
	free(eofs);

	return 0;
}

int find_correspondences(pl* hits, uint* starids, uint* fieldids, int* p_ok);

void write_field(pl* hits,
				 int fieldnum,
				 bool unsolvedstubs) {
	hits_field fieldhdr;
	int j;
	matchfile_entry me;

	// HACK -
	memset(&me, 0, sizeof(matchfile_entry));
	me.fieldnum = fieldnum;

	if (!pl_size(hits)) {
		if (unsolvedstubs && hitfid) {
			hits_field_init(&fieldhdr);
			fieldhdr.field = fieldnum;
			fieldhdr.nmatches = 0;
			fieldhdr.nagree = 0;
			fieldhdr.failed = TRUE;
			hits_write_field_header(hitfid, &fieldhdr);
			hits_start_hits_list(hitfid);
			hits_end_hits_list(hitfid);
			hits_write_field_tailer(hitfid);
			fflush(hitfid);
		}
		if (matchfile_start_table(agreemf, &me) ||
			matchfile_write_table(agreemf) ||
			matchfile_fix_table(agreemf)) {
			fprintf(stderr, "Failed to write agreeing matchfile header.\n");
		}
		il_append(empty, fieldnum);
		return;
	}

	fprintf(stderr, "Field %i: writing %i hits.\n", fieldnum, pl_size(hits));

	il_append(nonempty, fieldnum);

	hits_field_init(&fieldhdr);
	fieldhdr.field = fieldnum;
	fieldhdr.nmatches = pl_size(hits);
	fieldhdr.nagree = 0;

	if (matchfile_start_table(agreemf, &me) ||
		matchfile_write_table(agreemf)) {
		fprintf(stderr, "Failed to write agreeing matchfile header.\n");
	}

	for (j=0; j<pl_size(hits); j++) {
		matchfile_entry* me;
		MatchObj* mo = (MatchObj*)pl_get(hits, j);
		me = (matchfile_entry*)mo->extra;

		if (j == 0) {
			if (me)
				fieldhdr.fieldpath = me->fieldpath;
			if (hitfid) {
				hits_write_field_header(hitfid, &fieldhdr);
				hits_start_hits_list(hitfid);
			}
		}
		if (hitfid)
			hits_write_hit(hitfid, mo, me);
		if (matchfile_write_match(agreemf, mo)) {
			fprintf(stderr, "Error writing an agreeing match.");
		}
	}
	if (hitfid)
		hits_end_hits_list(hitfid);

	if (matchfile_fix_table(agreemf)) {
		fprintf(stderr, "Failed to fix agreeing matchfile header.\n");
	}

	if (hitfid) {
		uint* starids;
		uint* fieldids;
		int correspond_ok = 1;
		int Ncorrespond;
		starids  = malloc(pl_size(hits) * 4 * sizeof(uint));
		fieldids = malloc(pl_size(hits) * 4 * sizeof(uint));
		Ncorrespond = find_correspondences(hits, starids, fieldids, &correspond_ok);
		hits_write_correspondences(hitfid, starids, fieldids, Ncorrespond, correspond_ok);
		free(starids);
		free(fieldids);
		hits_write_field_tailer(hitfid);
		fflush(hitfid);
	}
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
