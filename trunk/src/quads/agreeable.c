#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "hitsfile.h"
#include "hitlist_healpix.h"
#include "matchfile.h"

char* OPTIONS = "hoH:n:A:B:F:L:M:f:m:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-o]: round-robin mode\n"
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
			" be written out.\n"
			"\nIf no match files are given, will read from stdin.\n",
			progname);
}

int find_correspondences(pl* hits, uint* starids, uint* fieldids, int* p_ok);

void flush_solved_fields(bool doleftovers, bool doagree, bool addunsolved, bool cleanup);

#define DEFAULT_MIN_MATCHES_TO_AGREE 3
#define DEFAULT_AGREE_TOL 7.0

unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;

FILE *hitfid = NULL;
char *hitfname = NULL;
char* leftoverfname = NULL;
FILE* leftoverfid = NULL;
char* agreefname = NULL;
FILE* agreefid = NULL;
pl* hitlists;
il* solved;
il* unsolved;
il* flushed;

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	hits_header hitshdr;
	char** inputfiles = NULL;
	int ninputfiles = 0;
	bool fromstdin = FALSE;
	int i;
	int firstfield=0, lastfield=INT_MAX;
	int flushinterval = 0;
	int flushfieldinterval = 0;
	bool leftovers = FALSE;
	bool agree = FALSE;
	bool roundrobin = FALSE;
	double ramin, ramax, decmin, decmax;
	double agreetolarcsec = DEFAULT_AGREE_TOL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'o':
			roundrobin = TRUE;
			break;
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
		fromstdin = TRUE;
		ninputfiles = 1;
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
		fopenout(leftoverfname, leftoverfid);
		leftovers = TRUE;
	}
	if (agreefname) {
		fopenout(agreefname, agreefid);
		agree = TRUE;
	}

	hitlists = pl_new(256);
	flushed = il_new(256);
	solved = il_new(256);
	unsolved = il_new(256);

	// write HITS header.
	hits_header_init(&hitshdr);
	hitshdr.nfields = 0;
	hitshdr.min_matches_to_agree = min_matches_to_agree;
	hits_write_header(hitfid, &hitshdr);


	if (roundrobin) {
		FILE* infiles[ninputfiles];
		int nread = 0;
		int f;
		if (fromstdin) {
			fprintf(stderr, "can't do round-robin from stdin!\n");
			exit(-1);
		}
		for (i=0; i<ninputfiles; i++) {
			fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
			fopenin(inputfiles[i], infiles[i]);
		}
		for (f=firstfield; f<=lastfield; f++) {
			bool alldone = TRUE;
			int fieldnum;
			hitlist* hl;

			for (i=0; i<ninputfiles; i++)
				if (infiles[i])
					alldone = FALSE;
			if (alldone)
				break;

			fprintf(stderr, "Field %i.\n", f);

			// get the existing hitlist for this field...
			fieldnum = f;
			if (fieldnum < pl_size(hitlists)) {
				hl = (hitlist*)pl_get(hitlists, fieldnum);
				if (!hl) {
					// check if it's NULL because it's been flushed.
					if (il_contains(flushed, fieldnum)) {
						continue;
					}
					hl = hitlist_healpix_new(agreetolarcsec);
					pl_set(hitlists, fieldnum, hl);
				}
			} else {
				int k;
				for (k=pl_size(hitlists); k<fieldnum; k++)
					pl_append(hitlists, NULL);
				hl = hitlist_healpix_new(agreetolarcsec);
				pl_append(hitlists, hl);
			}

			for (i=0; i<ninputfiles; i++) {
				MatchObj* mo;
				matchfile_entry me;
				matchfile_entry* mecopy;
				int c;
				FILE* infile = infiles[i];
				int nr = 0;
				char* fname = inputfiles[i];

				if (!infile)
					continue;
				// detect EOF and exit gracefully...
				c = fgetc(infile);
				if (c == EOF) {
					fclose(infile);
					infiles[i] = NULL;
					continue;
				} else
					ungetc(c, infile);

				for (;;) {
					off_t offset = ftello(infile);
					if (matchfile_read_match(infile, &mo, &me)) {
						fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
						fflush(stderr);
						break;
					}
					if (me.fieldnum != fieldnum) {
						fseeko(infile, offset, SEEK_SET);
						free_MatchObj(mo);
						free(me.indexpath);
						free(me.fieldpath);
						break;
					}
					nread++;
					nr++;

					if (nread % 10000 == 9999) {
						fprintf(stderr, ".");
						fflush(stderr);
					}

					if (leftovers || agree) {
						mecopy = (matchfile_entry*)malloc(sizeof(matchfile_entry));
						memcpy(mecopy, &me, sizeof(matchfile_entry));
						mo->extra = mecopy;
					} else {
						mo->extra = NULL;
						free(me.indexpath);
						free(me.fieldpath);
					}

					// compute (x,y,z) center, scale, rotation.
					hitlist_healpix_compute_vector(mo);
					// add the match...
					hitlist_healpix_add_hit(hl, mo);
				}

				fprintf(stderr, "File %s: read %i matches.\n", inputfiles[i], nr);
			}
			// flush after each field (and clean up leftovers)
			flush_solved_fields(leftovers, agree, FALSE, TRUE);
			if (agreefid)
				fdatasync(fileno(agreefid));
		}
		fprintf(stderr, "\nRead %i matches.\n", nread);
		fflush(stderr);
	} else {
		for (i=0; i<ninputfiles; i++) {
			FILE* infile = NULL;
			char* fname;
			int nread;

			if (fromstdin) {
				fname = "stdin";
			} else {
				fname = inputfiles[i];
			}

			if ((strcmp(fname, "FLUSH") == 0) ||
				(flushinterval && ((i-1) % (flushinterval) == 0))) {
				printf("# flushing after file %i\n", i);
				fprintf(stderr, "Flushing solved fields...\n");
				flush_solved_fields(FALSE, agree, FALSE, FALSE);
				if (strcmp(fname, "FLUSH") == 0)
					continue;
			}

			if (fromstdin) {
				infile = stdin;
			} else {
				fopenin(fname, infile);
			}

			fprintf(stderr, "Reading from %s...\n", fname);
			fflush(stderr);
			nread = 0;
			for (;;) {
				MatchObj* mo;
				matchfile_entry me;
				matchfile_entry* mecopy;
				hitlist* hl;
				int c;
				int fieldnum;

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
				nread++;
				fieldnum = me.fieldnum;

				if (nread % 10000 == 9999) {
					fprintf(stderr, ".");
					fflush(stderr);
				}

				if (flushfieldinterval &&
					((nread % flushfieldinterval) == 0)) {
					fprintf(stderr, "\nRead %i matches; flushing solved fields.\n", nread);
					flush_solved_fields(FALSE, agree, FALSE, FALSE);
				}

				if ((fieldnum < firstfield) || (fieldnum > lastfield)) {
					free_MatchObj(mo);
					free(me.indexpath);
					free(me.fieldpath);
					continue;
				}

				// get the existing hitlist for this field...
				if (fieldnum < pl_size(hitlists)) {
					hl = (hitlist*)pl_get(hitlists, fieldnum);
					if (!hl) {
						// check if it's NULL because it's been flushed.
						if (il_contains(flushed, fieldnum)) {
							//fprintf(stderr, "Warning: field %i has already been flushed.\n", fieldnum);
							free_MatchObj(mo);
							free(me.indexpath);
							free(me.fieldpath);
							continue;
						}
						hl = hitlist_healpix_new(agreetolarcsec);
						pl_set(hitlists, fieldnum, hl);
					}
				} else {
					int k;
					for (k=pl_size(hitlists); k<fieldnum; k++)
						pl_append(hitlists, NULL);
					hl = hitlist_healpix_new(agreetolarcsec);
					pl_append(hitlists, hl);
				}

				if (leftovers || agree) {
					mecopy = (matchfile_entry*)malloc(sizeof(matchfile_entry));
					memcpy(mecopy, &me, sizeof(matchfile_entry));
					mo->extra = mecopy;
				} else {
					mo->extra = NULL;
					free(me.indexpath);
					free(me.fieldpath);
				}

				// compute (x,y,z) center, scale, rotation.
				hitlist_healpix_compute_vector(mo);

				// add the match...
				hitlist_healpix_add_hit(hl, mo);
			}
			fprintf(stderr, "\nRead %i matches.\n", nread);
			fflush(stderr);

			if (!fromstdin)
				fclose(infile);
		}
	}

	flush_solved_fields(leftovers, agree, TRUE, TRUE);

	pl_free(hitlists);
	il_free(flushed);
	il_free(solved);
	il_free(unsolved);

	// finish up HITS file...
	hits_write_tailer(hitfid);
	if (hitfname)
		fclose(hitfid);

	if (leftoverfid)
		fclose(leftoverfid);

	if (agreefid)
		fclose(agreefid);

	return 0;
}

void free_extra(MatchObj* mo) {
	matchfile_entry* me;
	if (!mo->extra) return;
	me = (matchfile_entry*)mo->extra;
	free(me->indexpath);
	free(me->fieldpath);
	free(me);
	mo->extra = NULL;
}

void flush_solved_fields(bool doleftovers,
						 bool doagree,
						 bool addunsolved,
						 bool cleanup) {
	int k;
	il* flushsolved = il_new(256);

	for (k=0; k<pl_size(hitlists); k++) {
		pl* best;
		hits_field fieldhdr;
		int nbest;
		int fieldnum = k;
		int j;
		uint* starids;
		uint* fieldids;
		int correspond_ok = 1;
		int Ncorrespond;

		hitlist* hl = (hitlist*)pl_get(hitlists, fieldnum);
		if (!hl) continue;
		nbest = hitlist_healpix_count_best(hl);
		if (nbest < min_matches_to_agree) {
			if (addunsolved) {
				il_append(unsolved, fieldnum);
			}
			if (doleftovers) {
				int j;
				int NA;
				pl* all = hitlist_healpix_get_all(hl);
				NA = pl_size(all);
				fprintf(stderr, "Field %i: writing %i leftovers...\n", fieldnum, NA);
				// write the leftovers...
				for (j=0; j<NA; j++) {
					matchfile_entry* me;
					MatchObj* mo = (MatchObj*)pl_get(all, j);
					me = (matchfile_entry*)mo->extra;
					if (matchfile_write_match(leftoverfid, mo, me)) {
						fprintf(stderr, "Error writing a match to %s: %s\n", leftoverfname, strerror(errno));
						break;
					}
				}
				fflush(leftoverfid);
				pl_free(all);
			}
			if (cleanup) {
				hitlist_healpix_free_extra(hl, free_extra);
				// this frees the MatchObjs.
				hitlist_healpix_clear(hl);
				hitlist_healpix_free(hl);
				pl_set(hitlists, fieldnum, NULL);
			}
			continue;
		}
		fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);

		best = hitlist_healpix_get_best(hl);
		//best = hitlist_healpix_get_all_best(hl);
		//best = hitlist_get_all_above_size(hl, min_matches_to_agree);

		il_append(flushsolved, fieldnum);
		il_append(solved, fieldnum);
		il_append(flushed, fieldnum);

		hits_field_init(&fieldhdr);
		fieldhdr.field = fieldnum;
		fieldhdr.nmatches = hitlist_healpix_count_all(hl);
		fieldhdr.nagree = nbest;

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
				if (matchfile_write_match(agreefid, mo, me)) {
					fprintf(stderr, "Error writing a match to %s: %s\n", agreefname, strerror(errno));
				}
			}
		}
		hits_end_hits_list(hitfid);

		starids  = (uint*)malloc(nbest * 4 * sizeof(uint));
		fieldids = (uint*)malloc(nbest * 4 * sizeof(uint));
		Ncorrespond = find_correspondences(best, starids, fieldids, &correspond_ok);
		hits_write_correspondences(hitfid, starids, fieldids, Ncorrespond, correspond_ok);
		free(starids);
		free(fieldids);
		hits_write_field_tailer(hitfid);
		fflush(hitfid);
		pl_free(best);

		// we now dump this hitlist.
		hitlist_healpix_free_extra(hl, free_extra);
		hitlist_healpix_clear(hl);
		hitlist_healpix_free(hl);
		pl_set(hitlists, fieldnum, NULL);
	}

	/*
	  printf("# nsolved = %i\n", il_size(flushsolved));
	  printf("solved=array([");
	  for (k=0; k<il_size(flushsolved); k++) {
	  printf("%i,", il_get(flushsolved, k));
	  }
	  printf("]);\n");		

	  // DEBUG - matlab
	  printf("# solved=[");
	  for (k=0; k<il_size(flushsolved); k++) {
	  printf("%i,", il_get(flushsolved, k));
	  }
	  printf("];\n");
	  fflush(stdout);
	  fprintf(stderr, "Flushed %i fields.\n", il_size(flushsolved));
	*/
	fprintf(stderr, "So far, %i fields have been solved.\n", il_size(solved));

	il_free(flushsolved);

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
		add_correspondence(starids, fieldids, mo->iA, mo->fA, &M, &ok);
		add_correspondence(starids, fieldids, mo->iB, mo->fB, &M, &ok);
		add_correspondence(starids, fieldids, mo->iC, mo->fC, &M, &ok);
		add_correspondence(starids, fieldids, mo->iD, mo->fD, &M, &ok);
	}
	if (p_ok && !ok) *p_ok = 0;
	return M;
}
