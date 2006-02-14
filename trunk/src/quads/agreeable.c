#include <stdio.h>
#include <errno.h>
#include <string.h>

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "blocklist.h"
#include "matchobj.h"
#include "hitsfile.h"
#include "hitlist.h"
#include "matchfile.h"

char* OPTIONS = "hH:n:A:B:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]\n"
			"   [-B last-field]\n"
			"   [-H hits-file]\n"
			"   [-n matches_needed_to_agree]\n"
			"%s"
			"\nIf no match files are given, will read from stdin.\n",
			progname, hitlist_get_parameter_help());
}

int find_correspondences(blocklist* hits, sidx* starids, sidx* fieldids, int* p_ok);

#define DEFAULT_MIN_MATCHES_TO_AGREE 3

unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;

FILE *hitfid = NULL;
char *hitfname = NULL;

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char* hitlist_options;
	char alloptions[256];
	hits_header hitshdr;
	char** inputfiles = NULL;
	int ninputfiles = 0;
	bool fromstdin = FALSE;
	int i;
	blocklist* hitlists;
	blocklist *solved, *unsolved;
	int firstfield=-1, lastfield=INT_MAX;

	hitlist_set_default_parameters();
	hitlist_options = hitlist_get_parameter_options();
	sprintf(alloptions, "%s%s", OPTIONS, hitlist_options);

    while ((argchar = getopt (argc, argv, alloptions)) != -1) {
		char* ind = index(hitlist_options, argchar);
		if (ind) {
			if (hitlist_process_parameter(argchar, optarg)) {
				printHelp(progname);
				exit(-1);
			}
			continue;
		}
		switch (argchar) {
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

	hitlists = blocklist_pointer_new(256);

	for (i=0; i<ninputfiles; i++) {
		FILE* infile = NULL;
		char* fname;
		int nread;

		if (fromstdin) {
			infile = stdin;
			fname = "stdin";
		} else {
			fname = inputfiles[i];
			fopenin(fname, infile);
		}

		fprintf(stderr, "Reading from %s...\n", fname);
		fflush(stderr);
		nread = 0;
		for (;;) {
			MatchObj* mo;
			matchfile_entry me;
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

			if ((fieldnum < firstfield) || (fieldnum > lastfield)) {
				free_star(mo->sMin);
				free_star(mo->sMax);
				free_MatchObj(mo);
				free(me.indexpath);
				free(me.fieldpath);
				continue;
			}

			// get the existing hitlist for this field...
			if (fieldnum < blocklist_count(hitlists)) {
				hl = (hitlist*)blocklist_pointer_access(hitlists, fieldnum);
				if (!hl) {
					hl = hitlist_new();
					blocklist_pointer_set(hitlists, fieldnum, hl);
				}
			} else {
				int k;
				for (k=blocklist_count(hitlists); k<fieldnum; k++)
					blocklist_pointer_append(hitlists, NULL);
				hl = hitlist_new();
				blocklist_pointer_append(hitlists, hl);
			}

			// add the match...
			hitlist_add_hit(hl, mo);

			free(me.indexpath);
			free(me.fieldpath);
		}
		fprintf(stderr, "Read %i matches.\n", nread);
		fflush(stderr);

		if (!fromstdin)
			fclose(infile);
	}

	// write HITS header.
	hits_header_init(&hitshdr);
	/*
	  hitshdr.field_file_name = fieldfname;
	  hitshdr.tree_file_name = treefname;
	  hitshdr.ncodes = codekd->ndata;
	  hitshdr.nstars = numstars;
	  hitshdr.codetol = codetol;
	  //hitshdr.agreetol = AgreeArcSec;
	  hitshdr.parity = ParityFlip;
	  hitshdr.max_matches_needed = max_matches_needed;
	*/
	hitshdr.nfields = blocklist_count(hitlists);
	hitshdr.min_matches_to_agree = min_matches_to_agree;
	hits_write_header(hitfid, &hitshdr);

	solved = blocklist_int_new(256);
	unsolved = blocklist_int_new(256);

	for (i=0; i<blocklist_count(hitlists); i++) {
		blocklist* all;
		blocklist* best;
		hits_field fieldhdr;
		int nbest;
		int fieldnum = i;

		hitlist* hl = (hitlist*)blocklist_pointer_access(hitlists, i);
		if (!hl) continue;
		all  = hitlist_get_all(hl);
		best = hitlist_get_best(hl);
		if (best)
			nbest = blocklist_count(best);
		else
			nbest = 0;
		fprintf(stderr, "Field %i: %i hits total, %i in best agreement.\n",
				fieldnum, blocklist_count(all), nbest);

		hits_field_init(&fieldhdr);
		/*
		  fieldhdr.user_quit = 0;
		  fieldhdr.field = fieldnum;
		  fieldhdr.objects_in_field = 0;
		  fieldhdr.objects_examined = 0;
		  fieldhdr.field_corners = params.cornerpix;
		  fieldhdr.ntries = params.numtries;
		  fieldhdr.parity = ParityFlip;
		*/
 		fieldhdr.nmatches = blocklist_count(all);
		fieldhdr.nagree = nbest;
		hits_write_field_header(hitfid, &fieldhdr);

		if (nbest < min_matches_to_agree) {
			//hits_write_hit(hitfid, NULL);
			blocklist_int_append(unsolved, fieldnum);
		} else {
			int j;
			sidx* starids;
			sidx* fieldids;
			int correspond_ok = 1;
			int Ncorrespond;
			hits_start_hits_list(hitfid);
			blocklist_int_append(solved, fieldnum);
			for (j=0; j<nbest; j++) {
				MatchObj* mo = (MatchObj*)blocklist_pointer_access(best, j);
				hits_write_hit(hitfid, mo);
			}
			starids  = (sidx*)malloc(nbest * 4 * sizeof(sidx));
			fieldids = (sidx*)malloc(nbest * 4 * sizeof(sidx));
			Ncorrespond = find_correspondences(best, starids, fieldids, &correspond_ok);
			hits_write_correspondences(hitfid, starids, fieldids, Ncorrespond, correspond_ok);
			free(starids);
			free(fieldids);
			hits_end_hits_list(hitfid);
		}

		hits_write_field_tailer(hitfid);
		fflush(hitfid);

		blocklist_free(all);
		hitlist_clear(hl);
		hitlist_free(hl);
	}

	blocklist_free(hitlists);

	// print out Python literals of the solved and unsolved fields.
	printf("# nsolved = %i\n# nunsolved = %i\n", blocklist_count(solved), blocklist_count(unsolved));
	printf("solved=array([");
	for (i=0; i<blocklist_count(solved); i++) {
		printf("%i,", blocklist_int_access(solved, i));
	}
	printf("]);\n");
	printf("unsolved=array([");
	for (i=0; i<blocklist_count(unsolved); i++) {
		printf("%i,", blocklist_int_access(unsolved, i));
	}
	printf("]);\n");

	// DEBUG - matlab
	printf("# solved=[");
	for (i=0; i<blocklist_count(solved); i++) {
		printf("%i,", blocklist_int_access(solved, i));
	}
	printf("];\n");
	printf("# unsolved=[");
	for (i=0; i<blocklist_count(unsolved); i++) {
		printf("%i,", blocklist_int_access(unsolved, i));
	}
	printf("];\n");

	blocklist_free(solved);
	blocklist_free(unsolved);

	// finish up HITS file...
	hits_write_tailer(hitfid);
	if (hitfname)
		fclose(hitfid);

	return 0;
}

inline void add_correspondence(sidx* starids, sidx* fieldids,
							   sidx starid, sidx fieldid,
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

int find_correspondences(blocklist* hits, sidx* starids, sidx* fieldids,
						 int* p_ok) {
	int i, N;
	int M;
	int ok = 1;
	MatchObj* mo;
	if (!hits) return 0;
	N = blocklist_count(hits);
	M = 0;
	for (i=0; i<N; i++) {
		mo = (MatchObj*)blocklist_pointer_access(hits, i);
		add_correspondence(starids, fieldids, mo->fA, mo->iA, &M, &ok);
		add_correspondence(starids, fieldids, mo->fB, mo->iB, &M, &ok);
		add_correspondence(starids, fieldids, mo->fC, mo->iC, &M, &ok);
		add_correspondence(starids, fieldids, mo->fD, mo->iD, &M, &ok);
	}
	if (p_ok && !ok) *p_ok = 0;
	return M;
}
