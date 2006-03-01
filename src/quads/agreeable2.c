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

char* OPTIONS = "hH:n:A:B:F:L:M:f:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]\n"
			"   [-B last-field]\n"
			"   [-H hits-file]\n"
			"   [-F flush-interval]\n"
			"   [-f flush-field-interval]\n"
			"   [-L write-leftover-matches-file]\n"
			"   [-M write-successful-matches-file]\n"
 			"   [-n matches_needed_to_agree]\n"
			"%s"
			"\nIf filename FLUSH is specified, agreeing matches will"
			" be written out.\n"
			"\nIf no match files are given, will read from stdin.\n",
			progname, hitlist_get_parameter_help());
}

int find_correspondences(blocklist* hits, sidx* starids, sidx* fieldids, int* p_ok);

void flush_solved_fields(bool doleftovers, bool doagree, bool addunsolved, bool cleanup);

#define DEFAULT_MIN_MATCHES_TO_AGREE 3

unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;

FILE *hitfid = NULL;
char *hitfname = NULL;
char* leftoverfname = NULL;
FILE* leftoverfid = NULL;
char* agreefname = NULL;
FILE* agreefid = NULL;
blocklist* hitlists;
blocklist *solved;
blocklist *unsolved;
blocklist* flushed;

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
	int firstfield=-1, lastfield=INT_MAX;
	int flushinterval = 0;
	int flushfieldinterval = 0;
	bool leftovers = FALSE;
	bool agree = FALSE;
	double ramin, ramax, decmin, decmax;

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

	hitlists = blocklist_pointer_new(256);
	flushed = blocklist_int_new(256);
	solved = blocklist_int_new(256);
	unsolved = blocklist_int_new(256);

	// write HITS header.
	hits_header_init(&hitshdr);
	hitshdr.nfields = 0;
	hitshdr.min_matches_to_agree = min_matches_to_agree;
	hits_write_header(hitfid, &hitshdr);

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
			double x1,y1,z1;
			double x2,y2,z2;
			double xc,yc,zc;
			double dx, dy, dz;
			double xn, yn, zn;
			double xe, ye, ze;
			double pn, pe;
			double rac, decc, arc;
			double r;
			double rotation;

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
					// check if it's NULL because it's been flushed.
					if (blocklist_int_contains(flushed, fieldnum)) {
						//fprintf(stderr, "Warning: field %i has already been flushed.\n", fieldnum);
						free_star(mo->sMin);
						free_star(mo->sMax);
						free_MatchObj(mo);
						free(me.indexpath);
						free(me.fieldpath);
						continue;
					}
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

			x1 = star_ref(mo->sMin, 0);
			y1 = star_ref(mo->sMin, 1);
			z1 = star_ref(mo->sMin, 2);
			// normalize.
			r = sqrt(square(x1) + square(y1) + square(z1));
			x1 /= r;
			y1 /= r;
			z1 /= r;

			x2 = star_ref(mo->sMax, 0);
			y2 = star_ref(mo->sMax, 1);
			z2 = star_ref(mo->sMax, 2);
			r = sqrt(square(x2) + square(y2) + square(z2));
			x2 /= r;
			y2 /= r;
			z2 /= r;

			xc = (x1 + x2) / 2.0;
			yc = (y1 + y2) / 2.0;
			zc = (z1 + z2) / 2.0;
			r = sqrt(square(xc) + square(yc) + square(zc));
			xc /= r;
			yc /= r;
			zc /= r;

			/*
			  radius2 = square(xc - x1) + square(yc - y1) + square(zc - z1);
			*/
			arc  = 60.0 * rad2deg(distsq2arc(square(x2-x1)+square(y2-y1)+square(z2-z1)));

			// we will characterise the rotation of the field with
			// respect to the "North" vector - the vector tangent
			// to the field pointing in the direction of increasing
			// Dec.  We take the derivatives of (x,y,z) wrt Dec,
			// at the center of the field.

			// {x,y,z}(dec) = {cos(ra)cos(dec), sin(ra)cos(dec), sin(dec)}
			// d{x,y,z}/ddec = {-cos(ra)sin(dec), -sin(ra)sin(dec), cos(dec)}

			// center of the field...
			rac  = xy2ra(xc, yc);
			decc = z2dec(zc);

			// North
			xn = -cos(rac) * sin(decc);
			yn = -sin(rac) * sin(decc);
			zn =             cos(decc);

			// "East" (maybe West - direction of increasing RA, whatever that is.)
			xe = -sin(rac) * cos(decc);
			ye =  cos(rac) * cos(decc);
			ze = 0.0;

			// Now we look at the vector from the center to sMin
			dx = x1 - xc;
			dy = y1 - yc;
			dz = z1 - zc;

			// Project that onto N, E: dot products
			pn = (dx * xn) + (dy * yn) + (dz * zn);
			pe = (dx * xe) + (dy * ye) + (dz * ze);

			rotation = atan2(pn, pe);

			r = sqrt(square(pn) + square(pe));
			pn /= r;
			pe /= r;

			mo->vector[0] = xc;
			mo->vector[1] = yc;
			mo->vector[2] = zc;
			mo->vector[3] = arc;
			mo->vector[4] = pn;
			mo->vector[5] = pe;
			/*
			  mo->vector[4] = rotation;
			  mo->vector[5] = 0.0;
			*/

			/*
			  fprintf(stderr, "Pos (%6.2f, %6.2f), Scale %5.2f, Rot %6.2f\n",
			  rad2deg(rac), rad2deg(decc), arc,
			  rad2deg(rotation + ((rotation<0.0)? 2.0*M_PI : 0.0)));
			*/


			// add the match...
			hitlist_add_hit(hl, mo);
		}
		fprintf(stderr, "\nRead %i matches.\n", nread);
		fflush(stderr);

		if (!fromstdin)
			fclose(infile);
	}

	flush_solved_fields(leftovers, agree, TRUE, TRUE);

	blocklist_free(hitlists);
	blocklist_free(flushed);
	blocklist_free(solved);
	blocklist_free(unsolved);

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
	blocklist* flushsolved = blocklist_int_new(256);

	for (k=0; k<blocklist_count(hitlists); k++) {
		blocklist* best;
		hits_field fieldhdr;
		int nbest;
		int fieldnum = k;
		int j;
		sidx* starids;
		sidx* fieldids;
		int correspond_ok = 1;
		int Ncorrespond;

		hitlist* hl = (hitlist*)blocklist_pointer_access(hitlists, fieldnum);
		if (!hl) continue;
		nbest = hitlist_count_best(hl);
		if (nbest < min_matches_to_agree) {
			if (addunsolved) {
				blocklist_int_append(unsolved, fieldnum);
			}
			if (doleftovers) {
				int j;
				int NA;
				blocklist* all = hitlist_get_all(hl);
				NA = blocklist_count(all);
				fprintf(stderr, "Field %i: writing %i leftovers...\n", fieldnum, NA);
				// write the leftovers...
				for (j=0; j<NA; j++) {
					matchfile_entry* me;
					MatchObj* mo = (MatchObj*)blocklist_pointer_access(all, j);
					me = (matchfile_entry*)mo->extra;
					if (matchfile_write_match(leftoverfid, mo, me)) {
						fprintf(stderr, "Error writing a match to %s: %s\n", leftoverfname, strerror(errno));
						break;
					}
				}
				fflush(leftoverfid);
				blocklist_free(all);
			}
			if (cleanup) {
				hitlist_free_extra(hl, free_extra);
				// this frees the MatchObjs.
				hitlist_clear(hl);
				hitlist_free(hl);
				blocklist_pointer_set(hitlists, fieldnum, NULL);
			}
			continue;
		}
		fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);

		best = hitlist_get_best(hl);
		/*
		  best = hitlist_get_all_above_size(hl, min_matches_to_agree);
		  nbest = blocklist_count(best);
		*/

		blocklist_int_append(flushsolved, fieldnum);
		blocklist_int_append(solved, fieldnum);
		blocklist_int_append(flushed, fieldnum);

		hits_field_init(&fieldhdr);
		fieldhdr.field = fieldnum;
		fieldhdr.nmatches = hitlist_count_all(hl);
		fieldhdr.nagree = nbest;

		//hits_write_field_header(hitfid, &fieldhdr);
		//hits_start_hits_list(hitfid);

		for (j=0; j<nbest; j++) {
			matchfile_entry* me;
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(best, j);
			me = (matchfile_entry*)mo->extra;

			if (j == 0) {
				if (me)
					fieldhdr.fieldpath = me->fieldpath;
				hits_write_field_header(hitfid, &fieldhdr);
				hits_start_hits_list(hitfid);
			}

			hits_write_hit(hitfid, mo, me);

			if (doagree) {
				matchfile_entry* me;
				me = (matchfile_entry*)mo->extra;
				if (matchfile_write_match(agreefid, mo, me)) {
					fprintf(stderr, "Error writing a match to %s: %s\n", agreefname, strerror(errno));
				}
			}
		}
		hits_end_hits_list(hitfid);

		starids  = (sidx*)malloc(nbest * 4 * sizeof(sidx));
		fieldids = (sidx*)malloc(nbest * 4 * sizeof(sidx));
		Ncorrespond = find_correspondences(best, starids, fieldids, &correspond_ok);
		hits_write_correspondences(hitfid, starids, fieldids, Ncorrespond, correspond_ok);
		free(starids);
		free(fieldids);
		hits_write_field_tailer(hitfid);
		fflush(hitfid);
		blocklist_free(best);

		// we now dump this hitlist.
		hitlist_free_extra(hl, free_extra);
		hitlist_clear(hl);
		hitlist_free(hl);
		blocklist_pointer_set(hitlists, fieldnum, NULL);
	}
	printf("# nsolved = %i\n", blocklist_count(flushsolved));
	printf("solved=array([");
	for (k=0; k<blocklist_count(flushsolved); k++) {
		printf("%i,", blocklist_int_access(flushsolved, k));
	}
	printf("]);\n");		

	// DEBUG - matlab
	printf("# solved=[");
	for (k=0; k<blocklist_count(flushsolved); k++) {
		printf("%i,", blocklist_int_access(flushsolved, k));
	}
	printf("];\n");
	fflush(stdout);

	fprintf(stderr, "Flushed %i fields.\n", blocklist_count(flushsolved));
	fprintf(stderr, "So far, %i fields have been solved.\n", blocklist_count(solved));

	blocklist_free(flushsolved);

	fflush(hitfid);
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
		add_correspondence(starids, fieldids, mo->iA, mo->fA, &M, &ok);
		add_correspondence(starids, fieldids, mo->iB, mo->fB, &M, &ok);
		add_correspondence(starids, fieldids, mo->iC, mo->fC, &M, &ok);
		add_correspondence(starids, fieldids, mo->iD, mo->fD, &M, &ok);
	}
	if (p_ok && !ok) *p_ok = 0;
	return M;
}
