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

/**
 *   Solve fields (with slavish devotion)
 *
 * Inputs: .ckdt .quad .skdt
 * Output: .match
 */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <sched.h>
#include <pthread.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "solver.h"
#include "solver_callbacks.h"
#include "matchobj.h"
#include "matchfile.h"
#include "hitlist_healpix.h"
#include "tic.h"
#include "quadfile.h"
#include "idfile.h"
#include "intmap.h"
#include "verify.h"
#include "solvedclient.h"
#include "solvedfile.h"
#include "ioutils.h"
#include "starkd.h"
#include "codekd.h"
#include "boilerplate.h"
#include "svd.h"
#include "fitsioutils.h"
#include "qfits_error.h"

static void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n", progname);
}

static void solve_fields();

static int read_parameters();

static void reset_next_field();

#define DEFAULT_CODE_TOL .01
#define DEFAULT_PARITY_FLIP FALSE

// params:
char *fieldfname, *treefname, *quadfname, *startreefname;
char *idfname, *matchfname, *donefname, *solvedfname, *solvedserver;
char* xcolname, *ycolname;
char* wcs_template;
bool parity;
double codetol;
int startdepth;
int enddepth;
double funits_lower;
double funits_upper;
double index_scale;
double index_scale_lower;
int fieldid;
int indexid;
int healpix;
double agreetol;
bool do_verify;
int nagree_toverify;
double verify_dist2;
double overlap_tosolve;
double overlap_tokeep;
int min_ninfield;
int do_correspond;
int threads;
double cxdx_margin;
int maxquads;

bool quiet;
bool silent;

int firstfield, lastfield;

il* fieldlist;
pthread_mutex_t fieldlist_mutex;

matchfile* mf;
pthread_mutex_t matchfile_mutex;

idfile* id;
quadfile* quads;
xylist* xyls;
startree* starkd;
codetree *codekd;

bool circle;

int nverified;

int main(int argc, char *argv[]) {
    uint numfields;
	char* progname = argv[0];
	int i;
	int err;

	if (argc == 2 && strcmp(argv[1],"-s") == 0) {
		silent = TRUE;
		fprintf(stderr, "premptive silence\n");
	}

	if (argc != 1 && !silent) {
		printHelp(progname);
		exit(-1);
	}

	if (!silent) {
		printf("Running on host:\n");
		fflush(stdout);
		system("hostname");
		printf("\n");
		fflush(stdout);
	}

	fieldlist = il_new(256);

	if ((err = pthread_mutex_init(&fieldlist_mutex, NULL)) ||
		(err = pthread_mutex_init(&matchfile_mutex, NULL))) {
		fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(errno));
		exit(-1);
	}

	qfits_err_statset(1);

	for (;;) {

		tic();

		fieldfname = NULL;
		treefname = NULL;
		quadfname = NULL;
		startreefname = NULL;
		idfname = NULL;
		matchfname = NULL;
		donefname = NULL;
		solvedfname = NULL;
		solvedserver = NULL;
		wcs_template = NULL;
		xcolname = strdup("X");
		ycolname = strdup("Y");
		parity = DEFAULT_PARITY_FLIP;
		codetol = DEFAULT_CODE_TOL;
		firstfield = lastfield = -1;
		startdepth = 0;
		enddepth = 0;
		funits_lower = 0.0;
		funits_upper = 0.0;
		index_scale = 0.0;
		fieldid = 0;
		indexid = 0;
		healpix = -1;
		agreetol = 0.0;
		nagree_toverify = 0;
		verify_dist2 = 0.0;
		overlap_tosolve = 0.0;
		overlap_tokeep = 0.0;
		min_ninfield = 0;
		do_correspond = 1;
		threads = 1;
		cxdx_margin = 0.0;
		quiet = FALSE;

		il_remove_all(fieldlist);

		quads = NULL;
		starkd = NULL;
		nverified = 0;

		if (read_parameters())
			break;

		if (!silent) {
			fprintf(stderr, "%s params:\n", progname);
			fprintf(stderr, "fields ");
			for (i=0; i<il_size(fieldlist); i++)
				fprintf(stderr, "%i ", il_get(fieldlist, i));
			fprintf(stderr, "\n");
			fprintf(stderr, "fieldfname %s\n", fieldfname);
			fprintf(stderr, "fieldid %i\n", fieldid);
			fprintf(stderr, "treefname %s\n", treefname);
			fprintf(stderr, "startreefname %s\n", startreefname);
			fprintf(stderr, "quadfname %s\n", quadfname);
			fprintf(stderr, "idfname %s\n", idfname);
			fprintf(stderr, "matchfname %s\n", matchfname);
			fprintf(stderr, "donefname %s\n", donefname);
			fprintf(stderr, "solvedfname %s\n", solvedfname);
			fprintf(stderr, "solvedserver %s\n", solvedserver);
			fprintf(stderr, "wcs %s\n", wcs_template);
			fprintf(stderr, "parity %i\n", parity);
			fprintf(stderr, "codetol %g\n", codetol);
			fprintf(stderr, "startdepth %i\n", startdepth);
			fprintf(stderr, "enddepth %i\n", enddepth);
			fprintf(stderr, "fieldunits_lower %g\n", funits_lower);
			fprintf(stderr, "fieldunits_upper %g\n", funits_upper);
			fprintf(stderr, "agreetol %g\n", agreetol);
			fprintf(stderr, "verify_dist %g\n", distsq2arcsec(verify_dist2));
			fprintf(stderr, "nagree_toverify %i\n", nagree_toverify);
			fprintf(stderr, "overlap_tosolve %f\n", overlap_tosolve);
			fprintf(stderr, "overlap_tokeep %f\n", overlap_tokeep);
			fprintf(stderr, "min_ninfield %i\n", min_ninfield);
			fprintf(stderr, "xcolname %s\n", xcolname);
			fprintf(stderr, "ycolname %s\n", ycolname);
			fprintf(stderr, "do_correspond %i\n", do_correspond);
			fprintf(stderr, "cxdx_margin %g\n", cxdx_margin);
			fprintf(stderr, "maxquads %i\n", maxquads);
			fprintf(stderr, "quiet %i\n", quiet);
			fprintf(stderr, "threads %i\n", threads);
		}

		if (!treefname || !fieldfname || (codetol < 0.0) || !matchfname) {
			fprintf(stderr, "Invalid params... this message is useless.\n");
			exit(-1);
		}

		// make agreetol be RMS.
		agreetol *= sqrt(2.0);

		reset_next_field();

		mf = matchfile_open_for_writing(matchfname);
		if (!mf) {
			fprintf(stderr, "Failed to open file %s to write match file.\n", matchfname);
			exit(-1);
		}
		boilerplate_add_fits_headers(mf->header);
		qfits_header_add(mf->header, "HISTORY", "This file was created by the program \"blind\".", NULL, NULL);
		if (matchfile_write_header(mf)) {
			fprintf(stderr, "Failed to write matchfile header.\n");
			exit(-1);
		}
		
		// Read .xyls file...
		if (!silent) {
			fprintf(stderr, "Reading fields file %s...", fieldfname);
			fflush(stderr);
		}
		xyls = xylist_open(fieldfname);
		if (!xyls) {
			fprintf(stderr, "Failed to read xylist.\n");
			exit(-1);
		}
		numfields = xyls->nfields;
		if (!silent)
			fprintf(stderr, "got %u fields.\n", numfields);
		
		if (parity) {
			if (!silent) 
				fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");
			xyls->parity = 1;
		}
		xyls->xname = xcolname;
		xyls->yname = ycolname;

		// Read .ckdt file...
		if (!silent) {
			fprintf(stderr, "Reading code KD tree from %s...\n", treefname);
			fflush(stderr);
		}
		codekd = codetree_open(treefname);
		if (!codekd)
			exit(-1);
		if (!silent) 
			fprintf(stderr, "  (%d quads, %d nodes, dim %d).\n",
					codetree_N(codekd), codetree_nodes(codekd), codetree_D(codekd));

		// Read .quad file...
		if (!silent) 
			fprintf(stderr, "Reading quads file %s...\n", quadfname);
		quads = quadfile_open(quadfname, 0);
		if (!quads) {
			fprintf(stderr, "Couldn't read quads file %s\n", quadfname);
			exit(-1);
		}
		index_scale = quadfile_get_index_scale_arcsec(quads);
		index_scale_lower = quadfile_get_index_scale_lower_arcsec(quads);
		indexid = quads->indexid;
		healpix = quads->healpix;

		if (!silent) 
			fprintf(stderr, "Index scale: %g arcmin, %g arcsec\n", index_scale/60.0, index_scale);

		// Read .skdt file...
		if (!silent) {
			fprintf(stderr, "Reading star KD tree from %s...\n", startreefname);
			fflush(stderr);
		}
		starkd = startree_open(startreefname);
		if (!starkd) {
			fprintf(stderr, "Failed to read star kdtree %s\n", startreefname);
			exit(-1);
		}

		if (!silent) 
			fprintf(stderr, "  (%d stars, %d nodes, dim %d).\n",
					startree_N(starkd), startree_nodes(starkd), startree_D(starkd));

		do_verify = (verify_dist2 > 0.0);
		{
			qfits_header* hdr = qfits_header_read(treefname);
			if (!hdr) {
				fprintf(stderr, "Failed to read FITS header from ckdt file %s\n", treefname);
				exit(-1);
			}
			if (cxdx_margin > 0.0) {
				// check for CXDX field in ckdt header...
				int cxdx = qfits_header_getboolean(hdr, "CXDX", 0);
				if (!cxdx) {
					fprintf(stderr, "Warning: you asked for a CXDX margin, but ckdt file %s does not have the CXDX FITS header.\n",
							treefname);
					//exit(-1);
				}
			}
			// check for CIRCLE field in ckdt header...
			circle = qfits_header_getboolean(hdr, "CIRCLE", 0);
			qfits_header_destroy(hdr);
		}
		if (!silent) 
			fprintf(stderr, "ckdt %s the CIRCLE header.\n",
					(circle ? "contains" : "does not contain"));

		if (solvedserver) {
			if (solvedclient_set_server(solvedserver)) {
				fprintf(stderr, "Error setting solvedserver.\n");
				exit(-1);
			}

			if ((il_size(fieldlist) == 0) && (firstfield != -1) && (lastfield != -1)) {
				int j;
				free(fieldlist);
				if (!silent) 
					printf("Contacting solvedserver to get field list...\n");
				fieldlist = solvedclient_get_fields(fieldid, firstfield, lastfield, 0);
				if (!fieldlist) {
					fprintf(stderr, "Failed to get field list from solvedserver.\n");
					exit(-1);
				}
				if (!silent) 
					printf("Got %i fields from solvedserver: ", il_size(fieldlist));
				if (!silent) {
					for (j=0; j<il_size(fieldlist); j++) {
						printf("%i ", il_get(fieldlist, j));
					}
					printf("\n");
				}
			}
		}

		id = idfile_open(idfname, 0);
		if (!id) {
			fprintf(stderr, "Couldn't open id file %s.\n", idfname);
			//exit(-1);
		}

		// Do it!
		solve_fields();

		if (donefname) {
			FILE* batchfid = NULL;
			if (!silent)
				fprintf(stderr, "Writing marker file %s...\n", donefname);
			batchfid = fopen(donefname, "wb");
			if (batchfid)
				fclose(batchfid);
			else
				fprintf(stderr, "Failed to write marker file %s: %s\n", donefname, strerror(errno));
		}

		if (solvedserver) {
			solvedclient_set_server(NULL);
		}

		xylist_close(xyls);
		if (matchfile_fix_header(mf) ||
			matchfile_close(mf)) {
			if (!silent)
				fprintf(stderr, "Error closing matchfile.\n");
		}
		codetree_close(codekd);
		startree_close(starkd);
		if (id)
			idfile_close(id);
		quadfile_close(quads);

		free(donefname);
		free(solvedfname);
		free(solvedserver);
		free(matchfname);
		free(xcolname);
		free(ycolname);
		free(wcs_template);
		free_fn(fieldfname);
		free_fn(treefname);
		free_fn(quadfname);
		free_fn(idfname);
		free_fn(startreefname);

		if (!silent)
			toc();
	}

	il_free(fieldlist);

	if ((err = pthread_mutex_destroy(&fieldlist_mutex)) ||
		(err = pthread_mutex_destroy(&matchfile_mutex))) {
		fprintf(stderr, "pthread_mutex_destroy failed: %s\n", strerror(err));
		exit(-1);
	}

	return 0;
}

static int read_parameters() {
	for (;;) {
		char buffer[10240];
		char* nextword;
//		if (!silent)
//			fprintf(stderr, "\nAwaiting your command:\n");
//		fflush(stderr);
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			return -1;
		}
		// strip off newline
		if (buffer[strlen(buffer) - 1] == '\n')
			buffer[strlen(buffer) - 1] = '\0';

		if (!silent) {
			fprintf(stderr, "Command: %s\n", buffer);
			fflush(stderr);
		}

		if (is_word(buffer, "help", &nextword)) {
			fprintf(stderr, "Commands:\n"
					"    index <index-file-name>\n"
					"    match <match-file-name>\n"
					"    done <done-file-name>\n"
					"    field <field-file-name>\n"
					"    solved <solved-filename>\n"
					"    fields [<field-number> or <start range>/<end range>...]\n"
					"    sdepth <start-field-object>\n"
					"    depth <end-field-object>\n"
					"    parity <0 || 1>\n"
					"    tol <code-tolerance>\n"
					"    fieldunits_lower <arcsec-per-pixel>\n"
					"    fieldunits_upper <arcsec-per-pixel>\n"
					"    index_lower <index-size-lower-bound-fraction>\n"
					"    agreetol <agreement-tolerance (arcsec)>\n"
					"    verify_dist <early-verification-dist (arcsec)>\n"
					"    nagree_toverify <nagree>\n"
					"    overlap_tosolve <overlap-fraction>\n"
					"    overlap_tokeep <overlap-fraction>\n"
					"    quiet    (Print less)\n"
					"    silent   (Don't print anything)\n"
					"    run\n"
					"    help\n"
					"    quit\n");
		} else if (is_word(buffer, "silent", &nextword)) {
			silent = TRUE;
		} else if (is_word(buffer, "quiet", &nextword)) {
			quiet = TRUE;
		} else if (is_word(buffer, "wcs ", &nextword)) {
			wcs_template = strdup(nextword);
		} else if (is_word(buffer, "maxquads ", &nextword)) {
			maxquads = atoi(nextword);
		} else if (is_word(buffer, "cxdx_margin ", &nextword)) {
			cxdx_margin = atof(nextword);
		} else if (is_word(buffer, "do_correspond ", &nextword)) {
			do_correspond = atoi(nextword);
		} else if (is_word(buffer, "xcol ", &nextword)) {
			free(xcolname);
			xcolname = strdup(nextword);
		} else if (is_word(buffer, "ycol ", &nextword)) {
			free(ycolname);
			ycolname = strdup(nextword);
		} else if (is_word(buffer, "threads ", &nextword)) {
			threads = atoi(nextword);
		} else if (is_word(buffer, "agreetol ", &nextword)) {
			agreetol = atof(nextword);
		} else if (is_word(buffer, "index ", &nextword)) {
			char* fname = nextword;
			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);
		} else if (is_word(buffer, "verify_dist ", &nextword)) {
			verify_dist2 = arcsec2distsq(atof(nextword));
		} else if (is_word(buffer, "nagree_toverify ", &nextword)) {
			nagree_toverify = atoi(nextword);
		} else if (is_word(buffer, "overlap_tosolve ", &nextword)) {
			overlap_tosolve = atof(nextword);
		} else if (is_word(buffer, "overlap_tokeep ", &nextword)) {
			overlap_tokeep = atof(nextword);
		} else if (is_word(buffer, "min_ninfield ", &nextword)) {
			min_ninfield = atoi(nextword);
		} else if (is_word(buffer, "field ", &nextword)) {
			fieldfname = mk_fieldfn(nextword);
		} else if (is_word(buffer, "fieldid ", &nextword)) {
			fieldid = atoi(nextword);
		} else if (is_word(buffer, "match ", &nextword)) {
			matchfname = strdup(nextword);
		} else if (is_word(buffer, "done ", &nextword)) {
			donefname = strdup(nextword);
		} else if (is_word(buffer, "solved ", &nextword)) {
			solvedfname = strdup(nextword);
		} else if (is_word(buffer, "solvedserver ", &nextword)) {
			solvedserver = strdup(nextword);
		} else if (is_word(buffer, "sdepth ", &nextword)) {
			startdepth = atoi(nextword);
		} else if (is_word(buffer, "depth ", &nextword)) {
			enddepth = atoi(nextword);
		} else if (is_word(buffer, "tol ", &nextword)) {
			codetol = atof(nextword);
		} else if (is_word(buffer, "parity ", &nextword)) {
			parity = (atoi(nextword) ? TRUE : FALSE);
		} else if (is_word(buffer, "fieldunits_lower ", &nextword)) {
			funits_lower = atof(nextword);
		} else if (is_word(buffer, "fieldunits_upper ", &nextword)) {
			funits_upper = atof(nextword);
		} else if (is_word(buffer, "firstfield ", &nextword)) {
			firstfield = atoi(nextword);
		} else if (is_word(buffer, "lastfield ", &nextword)) {
			lastfield = atoi(nextword);
		} else if (is_word(buffer, "fields ", &nextword)) {
			char* str = nextword;
			char* endp;
			int i, firstfld = -1;
			for (;;) {
				int fld = strtol(str, &endp, 10);
				if (str == endp) {
					// non-numeric value
					fprintf(stderr, "Couldn't parse: %.20s [etc]\n", str);
					break;
				}
				if (firstfld == -1) {
					il_insert_unique_ascending(fieldlist, fld);
				} else {
					if (firstfld > fld) {
						fprintf(stderr, "Ranges must be specified as <start>/<end>: %i/%i\n", firstfld, fld);
					} else {
						for (i=firstfld+1; i<=fld; i++) {
							il_insert_unique_ascending(fieldlist, i);
						}
					}
					firstfld = -1;
				}
				if (*endp == '/')
					firstfld = fld;
				if (*endp == '\0')
					// end of string
					break;
				str = endp + 1;
			}
		} else if (is_word(buffer, "run", &nextword)) {
			return 0;
		} else if (is_word(buffer, "quit", &nextword)) {
			return 1;
		} else {
			fprintf(stderr, "I didn't understand that command.\n");
			fflush(stderr);
		}
	}
}

struct solvethread_args {
	int winning_listind;
	int threadnum;
	bool running;
	hitlist* hits;
	pl* verified;
};
typedef struct solvethread_args threadargs;

struct cached_hits {
	int fieldnum;
	pl* matches;
};
typedef struct cached_hits cached_hits;

static int cached_hits_compare(const void* v1, const void* v2) {
	const cached_hits* ch1 = v1;
	const cached_hits* ch2 = v2;
	if (ch1->fieldnum > ch2->fieldnum)
		return 1;
	if (ch1->fieldnum < ch2->fieldnum)
		return -1;
	return 0;
}

// if "matches" are NULL, marks "fieldnum" as having been done.
static void write_hits(int fieldnum, pl* matches) {
	static int index = 0;
	static bl* cached = NULL;
	int k, nextfld;

	if (pthread_mutex_lock(&matchfile_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	if (!cached)
		cached = bl_new(16, sizeof(cached_hits));

	// DEBUG - ensure cache is empty.
	if (fieldnum == -1) {
		cached_hits* cache;
		if (!bl_size(cached))
			goto done_cacheflush;
		fprintf(stderr, "Warning: cache was not empty at the end of the run.\n");
		fprintf(stderr, "Cache: [ ");
		for (k=0; k<bl_size(cached); k++) {
			cached_hits* ch = bl_access(cached, k);
			fprintf(stderr, "%i ", ch->fieldnum);
		}
		fprintf(stderr, "]\n");
		nextfld = il_get(fieldlist, index);
		fprintf(stderr, "nextfld=%i\n", nextfld);

		// write it!
		for (k=0; k<bl_size(cached); k++) {
			cache = bl_access(cached, k);
			if (cache->matches) {
				for (k=0; k<pl_size(cache->matches); k++) {
					MatchObj* mo = pl_get(cache->matches, k);
					if (matchfile_write_match(mf, mo))
						fprintf(stderr, "Error writing a match.\n");
				}
				for (k=0; k<pl_size(cache->matches); k++) {
					MatchObj* mo = pl_get(cache->matches, k);
					free(mo);
				}
				pl_free(cache->matches);
			}
		}
		if (matchfile_fix_header(mf)) {
			fprintf(stderr, "Failed to fix matchfile header.\n");
		}
	done_cacheflush:
		bl_free(cached);
		cached = NULL;
		index = 0;
		goto bailout;
	}

	nextfld = il_get(fieldlist, index);

	if (nextfld == fieldnum) {
		cached_hits ch;
		cached_hits* cache;
		bool freeit = FALSE;

		ch.fieldnum = fieldnum;
		ch.matches = matches;
		cache = &ch;

		for (;;) {
			// write it!
			if (cache->matches) {
				for (k=0; k<pl_size(cache->matches); k++) {
					MatchObj* mo = pl_get(cache->matches, k);
					if (matchfile_write_match(mf, mo))
						fprintf(stderr, "Error writing a match.\n");
				}
			}
			index++;

			if (freeit) {
				if (cache->matches) {
					for (k=0; k<pl_size(cache->matches); k++) {
						MatchObj* mo = pl_get(cache->matches, k);
						free(mo);
					}
					pl_free(cache->matches);
				}
				bl_remove_index(cached, 0);
			}
			freeit = TRUE;
			if (bl_size(cached) == 0)
				break;
			nextfld = il_get(fieldlist, index);
			cache = bl_access(cached, 0);

			if (cache->fieldnum != nextfld)
				break;
		}
		if (matchfile_fix_header(mf)) {
			fprintf(stderr, "Failed to fix matchfile header.\n");
			goto bailout;
		}
	} else {
		// cache it!
		cached_hits cache;
		// deep copy
		cache.fieldnum = fieldnum;
		if (matches) {
			cache.matches = pl_new(32);
			for (k=0; k<pl_size(matches); k++) {
				MatchObj* mo = pl_get(matches, k);
				MatchObj* copy = malloc(sizeof(MatchObj));
				memcpy(copy, mo, sizeof(MatchObj));
				pl_append(cache.matches, copy);
			}
		} else
			cache.matches = NULL;

		bl_insert_sorted(cached, &cache, cached_hits_compare);
	}

 bailout:
	if (pthread_mutex_unlock(&matchfile_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	return;
}

void verify(MatchObj* mo, solver_params* params, double* field, int nfield, int fieldnum, int nagree,
			int* correspondences) {
	int matches, unmatches, conflicts;

	verify_hit(starkd->tree, mo, field, nfield, verify_dist2,
			   &matches, &unmatches, &conflicts, NULL, NULL, correspondences);
	if (!quiet && !silent)
		fprintf(stderr, "    field %i (%i agree): overlap %4.1f%%: %i in field (%im/%iu/%ic)\n",
				fieldnum, nagree, 100.0 * mo->overlap, mo->ninfield, matches, unmatches, conflicts);
 	fflush(stderr);
	mo->nverified = nverified;
	nverified++;
}

static qfits_header* compute_wcs(MatchObj* mo, solver_params* params) {
	double star[12];
	double field[8];
	double cmass[3];
	double fieldcmass[2];
	double proj[8];
	double pcm[2];
	double cov[4];
	double U[4], V[4], S[2], R[4];
	double scale;
	int i, j, k;

	qfits_header* wcs = NULL;

	// compute a simple WCS transformation:
	// -get field & star positions of the matching quad.
	for (i=0; i<4; i++) {
		getstarcoord(mo->star[i], star + i*3);
		field[i*2 + 0] = params->field[mo->field[i] * 2 + 0];
		field[i*2 + 1] = params->field[mo->field[i] * 2 + 1];
	}
	// -set the tangent point to be the center of mass of the matching quad.
	cmass[0] = (star[0] + star[3] + star[6] + star[9] ) / 4.0;
	cmass[1] = (star[1] + star[4] + star[7] + star[10]) / 4.0;
	cmass[2] = (star[2] + star[5] + star[8] + star[11]) / 4.0;
	normalize_3(cmass);
	fieldcmass[0] = (field[0] + field[2] + field[4] + field[6]) / 4.0;
	fieldcmass[1] = (field[1] + field[3] + field[5] + field[7]) / 4.0;
	// -project the matching stars around this center
	star_coords(star + 0, cmass, proj + 0, proj + 1);
	star_coords(star + 3, cmass, proj + 2, proj + 3);
	star_coords(star + 6, cmass, proj + 4, proj + 5);
	star_coords(star + 9, cmass, proj + 6, proj + 7);
	// -subtract out the projected center of mass.
	pcm[0] = (proj[0] + proj[2] + proj[4] + proj[6]) / 4.0;
	pcm[1] = (proj[1] + proj[3] + proj[5] + proj[7]) / 4.0;
	for (i=0; i<4; i++)
		for (j=0; j<2; j++)
			proj[i*2 + j] -= pcm[j];
	// -compute the covariance between field positions and projected
	//  positions of the stars that form the quad.
	for (i=0; i<4; i++)
		cov[i] = 0.0;
	for (i=0; i<4; i++)
		for (j=0; j<2; j++)
			for (k=0; k<2; k++)
				//cov[k*2 + j] += proj[i*2 + k] * (field[i*2 + j] - fieldcmass[j]);
				cov[j*2 + k] += proj[i*2 + k] * (field[i*2 + j] - fieldcmass[j]);
	// set up svd params
	{
		double* pcov[] = { cov, cov+2 };
		double* pU[]   = { U,   U  +2 };
		double* pV[]   = { V,   V  +2 };
		double eps, tol;
		eps = 1e-30;
		tol = 1e-30;
		svd(2, 2, 1, 1, eps, tol, pcov, S, pU, pV);
	}
	// rotation matrix R = V U'
	for (i=0; i<4; i++)
		R[i] = 0.0;
	for (i=0; i<2; i++)
		for (j=0; j<2; j++)
			for (k=0; k<2; k++)
				R[i*2 + j] += V[i*2 + k] * U[j*2 + k];
	// -compute scale: proj' * R * field / (field' * field)
	/*{
	  double numer, denom;
	  numer = denom = 0.0;
	  for (i=0; i<4; i++) {
	  double f0 = field[i*2+0] - fieldcmass[0];
	  double f1 = field[i*2+1] - fieldcmass[1];
	  double Rf0 = R[0] * f0 + R[1] * f1;
	  double Rf1 = R[2] * f0 + R[3] * f1;
	  numer += (Rf0 * proj[i*2 + 0]) + (Rf1 * proj[i*2 + 1]);
	  denom += (f0 * f0) + (f1 * f1);
	  }
	  scale = numer / denom;
	  }
	*/

	// -compute scale: make the variances equal.
	{
		double pvar, fvar;
		pvar = fvar = 0.0;
		for (i=0; i<4; i++)
			for (j=0; j<2; j++) {
				pvar += square(proj[i*2 + j]);
				fvar += square(field[i*2 + j] - fieldcmass[j]);
			}
		scale = sqrt(pvar / fvar);
	}

	/*
	  for (i=0; i<4; i++)
	  printf("relfield%i = [%g, %g];\n", i, field[2*i]-fieldcmass[0],
	  field[2*i+1]-fieldcmass[1]);
	  for (i=0; i<4; i++)
	  printf("proj%i = [%g, %g];\n", i, proj[2*i], proj[2*i+1]);
	  for (i=0; i<4; i++)
	  printf("field%i = [%g, %g];\n", i, field[2*i], field[2*i+1]);
	  for (i=0; i<4; i++) {
	  double ra, dec;
	  xyz2radec(star[i*3], star[i*3 + 1], star[i*3 + 2], &ra, &dec);
	  printf("star%i = [%g, %g];\n", i, rad2deg(ra), rad2deg(dec));
	  }
	  printf("starra=[");
	  for (i=0; i<4; i++) {
	  double ra, dec;
	  xyz2radec(star[i*3], star[i*3 + 1], star[i*3 + 2], &ra, &dec);
	  printf("%g,", rad2deg(ra));
	  }
	  printf("];\n");
	  printf("stardec=[");
	  for (i=0; i<4; i++) {
	  double ra, dec;
	  xyz2radec(star[i*3], star[i*3 + 1], star[i*3 + 2], &ra, &dec);
	  printf("%g,", rad2deg(dec));
	  }
	  printf("];\n");
	  printf("scale=%g;\n", scale);
	  printf("R=[");
	  for (i=0; i<4; i++) printf("%g, ", R[i]);
	  printf("];\n");
	  {
	  double x,y;
	  x = R[0] * (field[0] - fieldcmass[0]) +
	  R[1] * (field[1] - fieldcmass[1]);
	  y = R[2] * (field[0] - fieldcmass[0]) +
	  R[3] * (field[1] - fieldcmass[1]);
	  x *= scale;
	  y *= scale;
	  printf("x,y=(%g,%g), proj=(%g,%g)\n",
	  x, y, proj[0], proj[1]);
	  }
	*/

	{
		qfits_header* hdr;
		char val[64];
		double ra, dec;

		xyz2radec(cmass[0], cmass[1], cmass[2], &ra, &dec);
		hdr = qfits_header_default();

		qfits_header_add(hdr, "BITPIX", "8", " ", NULL);
		qfits_header_add(hdr, "NAXIS", "0", "No image", NULL);
		qfits_header_add(hdr, "EXTEND", "T", "FITS extensions may follow", NULL);

		qfits_header_add(hdr, "CTYPE1 ", "RA---TAN", "TAN (gnomic) projection", NULL);
		qfits_header_add(hdr, "CTYPE2 ", "DEC--TAN", "TAN (gnomic) projection", NULL);
		sprintf(val, "%.12g", rad2deg(ra));
		qfits_header_add(hdr, "CRVAL1 ", val, "RA  of reference point", NULL);
		sprintf(val, "%.12g", rad2deg(dec));
		qfits_header_add(hdr, "CRVAL2 ", val, "DEC of reference point", NULL);

		sprintf(val, "%.12g", fieldcmass[0]);
		qfits_header_add(hdr, "CRPIX1 ", val, "X reference pixel", NULL);
		sprintf(val, "%.12g", fieldcmass[1]);
		qfits_header_add(hdr, "CRPIX2 ", val, "Y reference pixel", NULL);

		qfits_header_add(hdr, "CUNIT1 ", "deg", "X pixel scale units", NULL);
		qfits_header_add(hdr, "CUNIT2 ", "deg", "Y pixel scale units", NULL);
		scale = rad2deg(scale);
		/*
		  sprintf(val, "%.12g", R[0] * scale);
		  qfits_header_add(hdr, "CD1_1", val, "Transformation matrix", NULL);
		  sprintf(val, "%.12g", R[1] * scale);
		  qfits_header_add(hdr, "CD1_2", val, " ", NULL);
		  sprintf(val, "%.12g", R[2] * scale);
		  qfits_header_add(hdr, "CD2_1", val, " ", NULL);
		  sprintf(val, "%.12g", R[3] * scale);
		  qfits_header_add(hdr, "CD2_2", val, " ", NULL);
		*/
		// bizarrely, this only seems to work when I swap rows of R.
		sprintf(val, "%.12g", R[2] * scale);
		qfits_header_add(hdr, "CD1_1", val, "Transformation matrix", NULL);
		sprintf(val, "%.12g", R[3] * scale);
		qfits_header_add(hdr, "CD1_2", val, " ", NULL);
		sprintf(val, "%.12g", R[0] * scale);
		qfits_header_add(hdr, "CD2_1", val, " ", NULL);
		sprintf(val, "%.12g", R[1] * scale);
		qfits_header_add(hdr, "CD2_2", val, " ", NULL);

		wcs = hdr;
	}

	return wcs;
}

static qfits_header* compute_wcs_2(MatchObj* mo, solver_params* params,
								   int* corr) {
	double xyz[3];
	double starcmass[3];
	double fieldcmass[2];
	double cov[4];
	double U[4], V[4], S[2], R[4];
	double scale;
	int i, j, k;
	int Ncorr;
	// projected star coordinates
	double* p;
	// relative field coordinates
	double* f;
	double pcm[2];
	double fcm[2];

	qfits_header* wcs = NULL;

	// compute a simple WCS transformation:
	// -get field & star positions of the matching quad.
	for (j=0; j<3; j++)
		starcmass[j] = 0.0;
	for (j=0; j<2; j++)
		fieldcmass[j] = 0.0;
	for (i=0; i<4; i++) {
		getstarcoord(mo->star[i], xyz);
		for (j=0; j<3; j++)
			starcmass[j] += xyz[j];
		for (j=0; j<2; j++)
			fieldcmass[j] += params->field[mo->field[i] * 2 + j];
	}
	// -set the tangent point to be the center of mass of the matching quad.
	for (j=0; j<2; j++)
		fieldcmass[j] /= 4.0;
	normalize_3(starcmass);

	// -count how many corresponding stars there are.
	/*
	  Ncorr = 0;
	  for (i=0; i<params->nfield; i++)
	  if (corr[i] != -1)
	  Ncorr++;
	*/
	Ncorr = 4;

	// -allocate and fill "p" and "f" arrays.
	p = malloc(Ncorr * 2 * sizeof(double));
	f = malloc(Ncorr * 2 * sizeof(double));
	j = 0;
	/*
	  for (i=0; i<params->nfield; i++)
	  if (corr[i] != -1) {
	  // -project the stars around the quad center
	  getstarcoord(corr[i], xyz);
	  star_coords(xyz, starcmass, p + 2*j, p + 2*j + 1);
	  // -express the field coords relative to the quad center
	  f[2*j+0] = params->field[2*i+0];
	  f[2*j+1] = params->field[2*i+1];
	  j++;
	  }
	*/
	for (i=0; i<4; i++) {
		getstarcoord(mo->star[i], xyz);
		star_coords(xyz, starcmass, p + 2*i, p + 2*i + 1);
		f[2*i+0] = params->field[mo->field[i] * 2 + 0];
		f[2*i+1] = params->field[mo->field[i] * 2 + 1];
	}


	fprintf(stderr, "Ncorr=%i;\n", Ncorr);
	fprintf(stderr, "p=[");
	for (i=0; i<Ncorr; i++)
		fprintf(stderr, "%g,%g;", p[2*i], p[2*i+1]);
	fprintf(stderr, "];\n");

	fprintf(stderr, "f=[");
	for (i=0; i<Ncorr; i++)
		fprintf(stderr, "%g,%g;", f[2*i], f[2*i+1]);
	fprintf(stderr, "];\n");

	// -compute centers of mass
	pcm[0] = pcm[1] = fcm[0] = fcm[1] = 0.0;
	for (i=0; i<Ncorr; i++)
		for (j=0; j<2; j++) {
			pcm[j] += p[2*i+j];
			fcm[j] += f[2*i+j];
		}
	pcm[0] /= (double)Ncorr;
	pcm[1] /= (double)Ncorr;
	fcm[0] /= (double)Ncorr;
	fcm[1] /= (double)Ncorr;
	// -subtract out the centers of mass.
	for (i=0; i<Ncorr; i++)
		for (j=0; j<2; j++) {
			f[2*i + j] -= fcm[j];
			p[2*i + j] -= pcm[j];
		}
	fprintf(stderr, "pcm=[%g,%g];\n", pcm[0], pcm[1]);
	fprintf(stderr, "fcm=[%g,%g];\n", fcm[0], fcm[1]);

	fprintf(stderr, "p2=[");
	for (i=0; i<Ncorr; i++)
		fprintf(stderr, "%g,%g;", p[2*i], p[2*i+1]);
	fprintf(stderr, "];\n");

	fprintf(stderr, "f2=[");
	for (i=0; i<Ncorr; i++)
		fprintf(stderr, "%g,%g;", f[2*i], f[2*i+1]);
	fprintf(stderr, "];\n");

	// -compute the covariance between field positions and projected
	//  positions of the corresponding stars.
	for (i=0; i<4; i++)
		cov[i] = 0.0;
	for (i=0; i<Ncorr; i++)
		for (j=0; j<2; j++)
			for (k=0; k<2; k++)
				cov[j*2 + k] += p[i*2 + k] * f[i*2 + j];
	// -set up svd params
	{
		double* pcov[] = { cov, cov+2 };
		double* pU[]   = { U,   U  +2 };
		double* pV[]   = { V,   V  +2 };
		double eps, tol;
		eps = 1e-30;
		tol = 1e-30;
		svd(2, 2, 1, 1, eps, tol, pcov, S, pU, pV);
	}
	// -compute rotation matrix R = V U'
	for (i=0; i<4; i++)
		R[i] = 0.0;
	for (i=0; i<2; i++)
		for (j=0; j<2; j++)
			for (k=0; k<2; k++)
				R[i*2 + j] += V[i*2 + k] * U[j*2 + k];

	// -compute scale: make the variances equal.
	{
		double pvar, fvar;
		pvar = fvar = 0.0;
		for (i=0; i<Ncorr; i++)
			for (j=0; j<2; j++) {
				pvar += square(p[i*2 + j]);
				fvar += square(f[i*2 + j]);
			}
		//fprintf(stderr, "before scaling: variances %g, %g\n", pvar, fvar);
		scale = sqrt(pvar / fvar);
		fprintf(stderr, "scale=%g;\n", scale);
		/* check
		  {
		  double pscale = sqrt(pvar / (double)Ncorr);
		  double fscale = sqrt(fvar / (double)Ncorr);
		  fprintf(stderr, "pscale %g, fscale %g.\n", pscale, fscale);
		  for (i=0; i<Ncorr; i++)
		  for (j=0; j<2; j++) {
		  p[i*2 + j] /= pscale;
		  f[i*2 + j] /= fscale;
		  }
		  pvar = fvar = 0.0;
		  for (i=0; i<Ncorr; i++)
		  for (j=0; j<2; j++) {
		  pvar += square(p[i*2 + j]);
		  fvar += square(f[i*2 + j]);
		  }
		  pvar /= (double)Ncorr;
		  fvar /= (double)Ncorr;
		  fprintf(stderr, "after scaling: variances %g, %g\n", pvar, fvar);
		  }
		*/
	}

	{
		qfits_header* hdr;
		char val[64];
		double ra, dec;

		xyz2radec(starcmass[0], starcmass[1], starcmass[2], &ra, &dec);
		hdr = qfits_header_default();

		qfits_header_add(hdr, "BITPIX", "8", " ", NULL);
		qfits_header_add(hdr, "NAXIS", "0", "No image", NULL);
		qfits_header_add(hdr, "EXTEND", "T", "FITS extensions may follow", NULL);

		qfits_header_add(hdr, "CTYPE1 ", "RA---TAN", "TAN (gnomic) projection", NULL);
		qfits_header_add(hdr, "CTYPE2 ", "DEC--TAN", "TAN (gnomic) projection", NULL);
		sprintf(val, "%.12g", rad2deg(ra));
		qfits_header_add(hdr, "CRVAL1 ", val, "RA  of reference point", NULL);
		sprintf(val, "%.12g", rad2deg(dec));
		qfits_header_add(hdr, "CRVAL2 ", val, "DEC of reference point", NULL);

		sprintf(val, "%.12g", fieldcmass[0]);
		qfits_header_add(hdr, "CRPIX1 ", val, "X reference pixel", NULL);
		sprintf(val, "%.12g", fieldcmass[1]);
		qfits_header_add(hdr, "CRPIX2 ", val, "Y reference pixel", NULL);

		qfits_header_add(hdr, "CUNIT1 ", "deg", "X pixel scale units", NULL);
		qfits_header_add(hdr, "CUNIT2 ", "deg", "Y pixel scale units", NULL);

		scale = rad2deg(scale);

		// bizarrely, this only seems to work when I swap the rows of R.
		sprintf(val, "%.12g", R[2] * scale);
		qfits_header_add(hdr, "CD1_1", val, "Transformation matrix", NULL);
		sprintf(val, "%.12g", R[3] * scale);
		qfits_header_add(hdr, "CD1_2", val, " ", NULL);
		sprintf(val, "%.12g", R[0] * scale);
		qfits_header_add(hdr, "CD2_1", val, " ", NULL);
		sprintf(val, "%.12g", R[1] * scale);
		qfits_header_add(hdr, "CD2_2", val, " ", NULL);

		wcs = hdr;
	}

	free(p);
	free(f);

	return wcs;
}

int handlehit(solver_params* p, MatchObj* mo) {
	int listind;
	int n = 0;
	threadargs* my = p->userdata;
	int* corr = NULL;

	assert(mo->timeused >= 0.0);

	// compute (x,y,z) center, scale, rotation.
	hitlist_healpix_compute_vector(mo);
	n = hitlist_healpix_add_hit(my->hits, mo, &listind);

	if (!do_verify)
		return n;

	if (n < nagree_toverify)
		return n;

	if (p->wcs_filename) {
		int i;
		corr = malloc(p->nfield * sizeof(int));
		for (i=0; i<p->nfield; i++)
			corr[i] = -1;
	}

	verify(mo, p, p->field, p->nfield, p->fieldnum, n, corr);

	if (overlap_tosolve > 0.0) {
		bool solved = FALSE;
		if (n == nagree_toverify) {
			// run verification on the other matches
			int j;
			MatchObj* mo1 = NULL;
			pl* list = hitlist_healpix_copy_list(my->hits, listind);
			for (j=0; j<pl_size(list); j++) {
				mo1 = pl_get(list, j);
				if (mo1->overlap == 0.0) {
					verify(mo1, p, p->field, p->nfield, p->fieldnum, n, NULL);
					if (mo1->overlap >= overlap_tokeep)
						pl_append(my->verified, mo1);
				}
				if (mo1->overlap >= overlap_tosolve)
					solved = TRUE;
			}
			pl_free(list);
		}
		if (mo->overlap >= overlap_tokeep)
			pl_append(my->verified, mo);
		if (mo->overlap >= overlap_tosolve)
			solved = TRUE;

		if (solved && min_ninfield && (mo->ninfield < min_ninfield)) {
			fprintf(stderr, "    Match has only %i index stars in the field; %i required.\n",
					mo->ninfield, min_ninfield);
			solved = FALSE;
		}

		// we got enough overlaps to solve the field.
		if (solved) {
			if (!silent) {
				fprintf(stderr, "Found a match that produces %4.1f%% overlapping stars.\n", 100.0 * mo->overlap);
				fflush(stderr);
			}
			my->winning_listind = listind;
			p->quitNow = TRUE;

			if (p->wcs_filename) {
				FILE* fout;
				qfits_header* wcs = compute_wcs_2(mo, p, corr);
				fout = fopen(p->wcs_filename, "ab");
				if (!fout) {
					fprintf(stderr, "Failed to open WCS output file %s: %s\n", p->wcs_filename, strerror(errno));
					exit(-1);
				}
				if (qfits_header_dump(wcs, fout)) {
					fprintf(stderr, "Failed to write FITS WCS header.\n");
					exit(-1);
				}
				fits_pad_file(fout);
				qfits_header_destroy(wcs);

				// 4-point WCS.
				wcs = compute_wcs(mo, p);
				if (qfits_header_dump(wcs, fout)) {
					fprintf(stderr, "Failed to write FITS WCS header.\n");
					exit(-1);
				}
				fits_pad_file(fout);

				fclose(fout);
			}
		}
	}

	free(corr);

	return n;
}

static int next_field_index = 0;

static void reset_next_field() {
	next_field_index = 0;
}

static int next_field(xy** pfield) {
	int rtn;

	if (pthread_mutex_lock(&fieldlist_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	if (next_field_index >= il_size(fieldlist))
		rtn = -1;
	else {
		rtn = il_get(fieldlist, next_field_index);
		next_field_index++;
		if (pfield)
			*pfield = xylist_get_field(xyls, rtn);
	}

	if (pthread_mutex_unlock(&fieldlist_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	return rtn;
}

static void* solvethread_run(void* varg) {
	threadargs* my = varg;
	solver_params solver;
	double last_utime, last_stime;
	double utime, stime;
	struct timeval wtime, last_wtime;
	int nfields;
	double* field = NULL;

	if (!silent)
		fprintf(stderr, "Thread %i starting.\n", my->threadnum);

	get_resource_stats(&last_utime, &last_stime, NULL);
	gettimeofday(&last_wtime, NULL);

	solver_default_params(&solver);
	solver.codekd = codekd->tree;
	solver.endobj = enddepth;
	solver.maxtries = maxquads;
	solver.codetol = codetol;
	solver.handlehit = handlehit;
	solver.cxdx_margin = cxdx_margin;
	solver.quiet = quiet;

	if (do_verify)
		my->verified = pl_new(32);
	else
		my->verified = NULL;

	if (funits_upper != 0.0) {
		solver.arcsec_per_pixel_upper = funits_upper;
		solver.minAB = index_scale_lower / funits_upper;
		if (!silent)
			fprintf(stderr, "Set minAB to %g\n", solver.minAB);
	}
	if (funits_lower != 0.0) {
		solver.arcsec_per_pixel_lower = funits_lower;
		solver.maxAB = index_scale / funits_lower;
		if (!silent)
			fprintf(stderr, "Set maxAB to %g\n", solver.maxAB);
	}

	nfields = xyls->nfields;

	for (;;) {
		xy *thisfield = NULL;
		int fieldnum;
		MatchObj template;
		int nfield;
		char wcs_fn[1024];

		fieldnum = next_field(&thisfield);

		if (fieldnum == -1)
			break;
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			write_hits(fieldnum, NULL);
			continue;
		}
		if (!thisfield) {
			// HACK - why is this happening? QFITS + multithreading interaction bug?
			// or running out of address space?
			fprintf(stderr, "Couldn't get field %i\n", fieldnum);
			write_hits(fieldnum, NULL);
			continue;
		}

		if (solvedfname) {
			if (solvedfile_get(solvedfname, fieldnum)) {
				// file exists; field has already been solved.
				if (!silent)
					fprintf(stderr, "Field %i: solvedfile %s: field has been solved.\n", fieldnum, solvedfname);
				write_hits(fieldnum, NULL);
				free_xy(thisfield);
				continue;
			}
			solver.solvedfn = solvedfname;
		} else
			solver.solvedfn = NULL;

		if (solvedserver) {
			if (solvedclient_get(fieldid, fieldnum) == 1) {
				// field has already been solved.
				if (!silent)
					fprintf(stderr, "Field %i: field has already been solved.\n", fieldnum);
				write_hits(fieldnum, NULL);
				free_xy(thisfield);
				continue;
			}
		}
		solver.do_solvedserver = (solvedserver ? TRUE : FALSE);

		nfield = xy_size(thisfield);
		field = realloc(field, 2 * nfield * sizeof(double));
		dl_copy(thisfield, 0, 2 * nfield, field);
		free_xy(thisfield);

		memset(&template, 0, sizeof(MatchObj));
		template.fieldnum = fieldnum;
		template.parity = parity;
		template.fieldfile = fieldid;
		template.indexid = indexid;
		template.healpix = healpix;

		if (wcs_template) {
			sprintf(wcs_fn, wcs_template, fieldnum);
			solver.wcs_filename = wcs_fn;
		}

		solver.fieldid = fieldid;
		solver.fieldnum = fieldnum;
		solver.numtries = 0;
		solver.nummatches = 0;
		solver.numscaleok = 0;
		solver.mostagree = 0;
		solver.numcxdxskipped = 0;
		solver.startobj = startdepth;
		solver.field = field;
		solver.nfield = nfield;
		solver.quitNow = FALSE;
		solver.mo_template = &template;
		solver.circle = circle;
		solver.userdata = my;

		my->winning_listind = -1;
		my->hits = hitlist_healpix_new(agreetol);
		my->hits->do_correspond = do_correspond;

		// The real thing
		solve_field(&solver);

		if (!silent)
			fprintf(stderr, "field %i: tried %i quads, matched %i codes.\n",
					fieldnum, solver.numtries, solver.nummatches);

		if (maxquads && solver.numtries >= maxquads && !silent) {
			fprintf(stderr, "  exceeded the number of quads to try: %i >= %i.\n",
					solver.numtries, maxquads);
		}

		if (my->winning_listind == -1) {
			// didn't solve it...
			if (!silent)
				fprintf(stderr, "Field %i is unsolved.\n", fieldnum);
			// ... but write the matches for which verification was run
			// to collect good stats.
			if (do_verify) {
				// write 'em!
				write_hits(fieldnum, my->verified);
			} else {
				write_hits(fieldnum, NULL);
			}
		} else {
			double maxoverlap = 0;
			double sumoverlap = 0;
			pl* list = hitlist_healpix_copy_list(my->hits, my->winning_listind);
			if (do_verify) {
				int k;
				for (k=0; k<pl_size(list); k++) {
					MatchObj* mo = pl_get(list, k);
					// run verification on any of the matches that haven't
					// already been done.
					if (mo->overlap == 0.0) {
						verify(mo, &solver, solver.field, solver.nfield, solver.fieldnum, pl_size(list), NULL);
						if (do_verify)
							pl_append(my->verified, mo);
					}

					sumoverlap += mo->overlap;
					if (mo->overlap > maxoverlap)
						maxoverlap = mo->overlap;
				}
				if (!silent)
					fprintf(stderr, "Field %i: %i in agreement.  Overlap of winning cluster: max %f, avg %f\n",
							fieldnum, pl_size(list), maxoverlap, sumoverlap / (double)pl_size(list));
			} else if (!silent)
				fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, pl_size(list));
			
			// write 'em!
			if (do_verify)
				write_hits(fieldnum, my->verified);
			else
				write_hits(fieldnum, list);
			pl_free(list);

			if (solvedfname) {
				if (!silent)
					fprintf(stderr, "Field %i solved: writing to file %s to indicate this.\n", fieldnum, solvedfname);
				if (solvedfile_set(solvedfname, fieldnum)) {
					fprintf(stderr, "Failed to write to solvedfile %s.\n", solvedfname);
				}
			}
			if (solvedserver) {
				solvedclient_set(fieldid, fieldnum);
			}

		}
		hitlist_healpix_clear(my->hits);
		hitlist_healpix_free(my->hits);

		if (do_verify)
			pl_remove_all(my->verified);

		get_resource_stats(&utime, &stime, NULL);
		gettimeofday(&wtime, NULL);
		if (!silent)
			fprintf(stderr, "Spent %g s user, %g s system, %g s total, %g s wall time.\n\n\n",
					(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime),
					millis_between(&last_wtime, &wtime) * 0.001);
		last_utime = utime;
		last_stime = stime;
		last_wtime = wtime;
	}

	free(field);
	pl_free(my->verified);

	if (!silent)
		fprintf(stderr, "Thread %i finished.\n", my->threadnum);
	my->running = FALSE;
	return 0;
}

static void solve_fields() {
	int i;
	int STACKSIZE = 1024*1024;
	threadargs* allargs[threads];
	unsigned char* allstacks[threads];

	for (i=0; i<threads; i++) {
		threadargs* args;
		unsigned char* stack;
		args = calloc(1, sizeof(threadargs));
		stack = calloc(STACKSIZE, 1);
		allargs[i] = args;
		allstacks[i] = stack;
		args->threadnum = (i+1);
		args->running = TRUE;
		if (i == (threads - 1))
			solvethread_run(args);
		else {
			pthread_t thread;
			if (pthread_create(&thread, NULL, solvethread_run, args)) {
				fprintf(stderr, "Failed to create thread: %s\n", strerror(errno));
				break;
			}
		}
	}

	for (;;) {
		bool alldone = TRUE;
		for (i=0; i<(threads-1); i++) {
			if (allargs[i]->running) {
				alldone = FALSE;
				break;
			}
		}
		if (alldone)
			break;
		sleep(5);
	}

	// DEBUG
	write_hits(-1, NULL);

	for (i=0; i<threads; i++) {
		free(allargs[i]);
		free(allstacks[i]);
	}
}

void getquadids(uint thisquad, uint *iA, uint *iB, uint *iC, uint *iD) {
	uint sA, sB, sC, sD;
	quadfile_get_starids(quads, thisquad, &sA, &sB, &sC, &sD);
	*iA = sA;
	*iB = sB;
	*iC = sC;
	*iD = sD;
}

void getstarcoord(uint iA, double *sA) {
	startree_get(starkd, iA, sA);
}

uint64_t getstarid(uint iA) {
	if (id)
		return idfile_get_anid(id, iA);
	return 0;
}
