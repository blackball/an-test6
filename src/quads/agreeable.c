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
#include "hitsfile.h"
#include "hitlist_healpix.h"
#include "matchfile.h"
#include "solvedclient.h"
#include "solvedfile.h"

char* OPTIONS = "hH:n:A:B:L:M:m:o:f:bFs:I:J:RS:at";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]\n"
			"   [-B last-field]\n"
			"   [-I first-field-filenum]\n"
			"   [-J last-field-filenum]\n"
			"   [-H hits-file]\n"
			"   [-L write-leftover-matches-file]\n"
			"   [-M write-successful-matches-file]\n"
			"   [-m agreement-tolerance-in-arcsec]\n"
 			"   [-n matches_needed_to_agree]\n"
			"   [-o overlap_needed_to_solve]\n"
			"   [-f minimum-field-objects-needed-to-solve]\n"
			"   [-b]: best-overlap mode\n"
			"   [-F]: first-solved mode (smallest number of field objs)\n"
			"   [-a]: agreement mode (waits until enough agreeing matches have been found,\n"
			"                         prints the last one if it is above the overlap thresh,\n"
			"                         otherwise prints the last one but steals the 'overlap'\n"
			"                         from the largest-overlap match)\n"
			"   [-t]: print agreement distances.\n"
			"   [-s <solved-server-address>]\n"
			"   [-S <solved-file-template>]\n"
			"   [-R]: read all at once\n",
			progname);
}

int find_correspondences(pl* hits, uint* starids, uint* fieldids, int* p_ok);

void write_field(hitlist* hl,
				 pl* overlaps,
				 int fieldfile,
				 int fieldnum,
				 bool doleftovers,
				 bool doagree,
				 bool unsolvedstubs);

#define DEFAULT_MIN_MATCHES_TO_AGREE 3
#define DEFAULT_AGREE_TOL 10.0

unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;

FILE *hitfid = NULL;
char *hitfname = NULL;
char* leftoverfname = NULL;
matchfile* leftovermf = NULL;
char* agreefname = NULL;
matchfile* agreemf = NULL;
il* solved;
il* unsolved;

char* solvedserver = NULL;
char* solvedfile = NULL;

double overlap_needed = 0.0;
int min_ninfield = 0;

bool do_agree_overlap = FALSE;

bool print_agree = FALSE;
dl* agreedistlist;

static int compare_overlaps(const void* v1, const void* v2) {
	const MatchObj* mo1 = v1;
	const MatchObj* mo2 = v2;
	double diff = mo2->overlap - mo1->overlap;
	if (diff > 0.0)
		return 1;
	if (diff == 0.0)
		return 0;
	return -1;
}

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

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	hits_header hitshdr;
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	int firstfield=0, lastfield=INT_MAX-1;
	int firstfieldfile=1, lastfieldfile=INT_MAX-1;
	bool leftovers = FALSE;
	bool agree = FALSE;
	//double ramin, ramax, decmin, decmax;
	double agreetolarcsec = DEFAULT_AGREE_TOL;
	matchfile** mfs;
	MatchObj** mos;
	bool* eofs;
	bool* eofieldfile;
	int nread = 0;
	int f;
	hitlist* hl = NULL;
	pl* overlaps = NULL;
	bool do_best_overlap = FALSE;
	bool do_first_overlap = FALSE;
	//bool do_agree_overlap = FALSE;
	int fieldfile;
	int totalsolved, totalunsolved;
	bool read_all = FALSE;
	bl** lists;
	int* inds;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'S':
			solvedfile = optarg;
			break;
		case 'R':
			read_all = TRUE;
			break;
		case 's':
			solvedserver = optarg;
			break;
		case 'b':
			do_best_overlap = TRUE;
			break;
		case 'F':
			do_first_overlap = TRUE;
			break;
		case 'a':
			do_agree_overlap = TRUE;
			break;
		case 't':
			print_agree = TRUE;
			break;
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
		case 'H':
			hitfname = optarg;
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

	if (do_first_overlap && do_best_overlap) {
		fprintf(stderr, "Can't select both -b and -F.\n");
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

	if (hitfname)
		fopenout(hitfname, &hitfid);

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

	if (do_best_overlap || do_first_overlap)
		overlaps = pl_new(32);
	else
		hl = hitlist_healpix_new(agreetolarcsec);

	solved = il_new(256);
	unsolved = il_new(256);

	if (hitfid) {
		// write HITS header.
		hits_header_init(&hitshdr);
		hitshdr.min_matches_to_agree = min_matches_to_agree;
		hitshdr.overlap_needed = overlap_needed;
		hitshdr.field_objs_needed = min_ninfield;
		hitshdr.agreetol = agreetolarcsec;
		hits_write_header(hitfid, &hitshdr);
	}

	totalsolved = totalunsolved = 0;

	mos =  calloc(ninputfiles, sizeof(MatchObj*));
	eofs = calloc(ninputfiles, sizeof(bool));
	eofieldfile = malloc(ninputfiles * sizeof(bool));

	if (read_all) {
		mfs = NULL;
		lists = malloc(ninputfiles * sizeof(bl*));
		inds = calloc(ninputfiles, sizeof(int));

		for (i=0; i<ninputfiles; i++) {
			matchfile* mf;
			lists[i] = bl_new(1024, sizeof(MatchObj));
			fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
			mf = matchfile_open(inputfiles[i]);
			if (!mf) {
				fprintf(stderr, "Failed to open matchfile %s.\n", inputfiles[i]);
				exit(-1);
			}
			for (;;) {
				MatchObj* mo = matchfile_buffered_read_match(mf);
				if (!mo)
					break;
				bl_append(lists[i], mo);
			}
			fprintf(stderr, "Read %i matches.\n", bl_size(lists[i]));
			matchfile_close(mf);
		}

	} else {
		mfs = malloc(ninputfiles * sizeof(matchfile*));
		lists = NULL;
		inds = NULL;

		for (i=0; i<ninputfiles; i++) {
			fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
			mfs[i] = matchfile_open(inputfiles[i]);
			if (!mfs[i]) {
				fprintf(stderr, "Failed to open matchfile %s.\n", inputfiles[i]);
				exit(-1);
			}
		}
	}




	// we assume the matchfiles are sorted by field id and number.
	for (fieldfile=firstfieldfile; fieldfile<=lastfieldfile; fieldfile++) {

		memset(eofieldfile, 0, ninputfiles * sizeof(bool));

		bool alldone = TRUE;
		for (f=firstfield; f<=lastfield; f++) {
			int fieldnum = f;
			bool donefieldfile;
			alldone = TRUE;
			for (i=0; i<ninputfiles; i++)
				if (!eofs[i]) {
					alldone = FALSE;
					break;
				}
			if (alldone)
				break;

			donefieldfile = TRUE;
			for (i=0; i<ninputfiles; i++)
				if (!eofieldfile[i] && !eofs[i]) {
					//fprintf(stderr, "input file %s still has hits for fieldfile %i.\n", inputfiles[i], fieldfile);
					//if (mos[i])
					//fprintf(stderr, "(field %i.)\n", mos[i]->fieldnum);
					donefieldfile = FALSE;
					break;
				}
			if (donefieldfile)
				break;

			fprintf(stderr, "File %i, Field %i.\n", fieldfile, f);

			for (i=0; i<ninputfiles; i++) {
				int nr = 0;
				int ns = 0;

				while (1) {
					if (eofs[i])
						break;
					if (!mos[i]) {
						if (read_all) {
							if (unlikely(inds[i] == bl_size(lists[i])))
								mos[i] = NULL;
							else {
								mos[i] = bl_access(lists[i], inds[i]);
								inds[i]++;
							}
						} else
							mos[i] = matchfile_buffered_read_match(mfs[i]);
					}
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
					nr++;
					if (nread % 10000 == 9999) {
						fprintf(stderr, ".");
						fflush(stderr);
					}

					if (!read_all) {
						MatchObj* copy = malloc(sizeof(MatchObj));
						memcpy(copy, mos[i], sizeof(MatchObj));
						mos[i] = copy;
					}

					if (do_best_overlap) {
						pl_insert_sorted(overlaps, mos[i], compare_overlaps);
					} else if (do_first_overlap) {
						pl_insert_sorted(overlaps, mos[i], compare_objs_used);
					} else {
						// compute (x,y,z) center, scale, rotation.
						hitlist_healpix_compute_vector(mos[i]);
						// add the match...
						hitlist_healpix_add_hit(hl, mos[i], NULL);
					}
					mos[i] = NULL;
				}
				if (nr || ns)
					fprintf(stderr, "File %s: read %i matches, skipped %i matches.\n", inputfiles[i], nr, ns);
			}

			write_field(hl, overlaps, fieldfile, fieldnum, leftovers, agree, TRUE);

			if (read_all) {
				if (hl)
					hitlist_healpix_remove_all(hl);
				if (overlaps)
					pl_remove_all(overlaps);
			} else {
				if (hl)
					hitlist_healpix_clear(hl);
				if (overlaps) {
					int i;
					for (i=0; i<pl_size(overlaps); i++)
						free_MatchObj(pl_get(overlaps, i));
					pl_remove_all(overlaps);
				}
			}

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

	if (read_all) {
		for (i=0; i<ninputfiles; i++)
			bl_free(lists[i]);
		free(lists);
		free(inds);
	} else {
		for (i=0; i<ninputfiles; i++)
			matchfile_close(mfs[i]);
		free(mfs);
	}
	free(mos);
	free(eofs);


	fprintf(stderr, "\nRead %i matches.\n", nread);
	fflush(stderr);

	if (hl)
		hitlist_healpix_free(hl);
	if (overlaps)
		pl_free(overlaps);

	il_free(solved);
	il_free(unsolved);

	// finish up HITS file...
	if (hitfid) {
		hits_write_tailer(hitfid);
		fclose(hitfid);
	}

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

void write_field(hitlist* hl,
				 pl* overlaps,
				 int fieldfile,
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
	int nall;

	assert(hl || overlaps);

	if (hl) {
		nbest = hitlist_healpix_count_best(hl);
		nall = hitlist_healpix_count_all(hl);
	} else {
		nall = pl_size(overlaps);
		nbest = (nall ? 1 : 0);
	}

	hits_field_init(&fieldhdr);
	fieldhdr.field = fieldnum;

	if (overlap_needed > 0.0) {
		pl* lists = pl_new(32);
		if (hl) {
			int nlists = hitlist_healpix_count_lists(hl);
			for (j=0; j<nlists; j++) {
				pl* list = hitlist_healpix_copy_list(hl, j);
				if (!list)
					continue;
				pl_append(lists, list);
			}
		} else {
			pl* copy = pl_dup(overlaps);
			pl_append(lists, copy);
		}

		for (j=0; j<pl_size(lists); j++) {
			int i;
			bool gotit = FALSE;
			pl* list = pl_get(lists, j);
			MatchObj* mo;

			MatchObj* mbest = NULL;
			double bestoverlap = 0.0;

			for (i=0; i<pl_size(list); i++) {
				mo = pl_get(list, i);

				if (i && hl && print_agree) {
					double bestd2 = 1e300;
					int j;
					for (j=0; j<i; j++) {
						double d2 = -1.0;
						MatchObj* mo2 = pl_get(list, j);
						hitlist_hits_agree(mo, mo2, hl->agreedist2, &d2);
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
						(mo->ninfield >= min_ninfield)) {
						mbest = mo;
						bestoverlap = mo->overlap;
					}
					if ((i >= (min_matches_to_agree-1)) &&
						(bestoverlap >= overlap_needed)) {
						gotit = TRUE;
						break;
					}
					
				} else {
					if ((mo->overlap >= overlap_needed) &&
						(mo->ninfield >= min_ninfield)) {
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
		if (hl) {
			if (nbest < min_matches_to_agree) {
				issolved = FALSE;
			} else {
				issolved = TRUE;
				fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);
				best = hitlist_healpix_get_best(hl);
				//best = hitlist_healpix_get_all_best(hl);
				//best = hitlist_get_all_above_size(hl, min_matches_to_agree);
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
		if (hitfid && unsolvedstubs) {
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
			if (hl) {
				all = hitlist_healpix_get_all(hl);
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
			if (hl)
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

	if (hitfid) {
		hits_write_field_header(hitfid, &fieldhdr);
		hits_start_hits_list(hitfid);
	}

	for (j=0; j<pl_size(best); j++) {
		MatchObj* mo = pl_get(best, j);
		if (hitfid)
			hits_write_hit(hitfid, mo);
		if (doagree) {
			if (matchfile_write_match(agreemf, mo)) {
				fprintf(stderr, "Error writing an agreeing match.");
			}
		}
		if (overlap_needed > 0.0)
			fprintf(stderr, "Field %i: Overlap %f\n", fieldnum, mo->overlap);
	}

	if (hitfid) {
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
	}
	pl_free(best);
}

Inline void add_correspondence(uint* starids, uint* fieldids,
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
