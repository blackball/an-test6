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

#include "kdtree.h"
#include "kdtree_access.h"
#include "kdtree_fits_io.h"
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

static void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n", progname);
}

static void solve_fields();

static int read_parameters();

#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP FALSE

// params:
char *fieldfname, *treefname, *quadfname, *startreefname;
char *idfname, *matchfname, *donefname, *solvedfname, *solvedserver;
char* xcolname, *ycolname;
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

il* fieldlist;
pthread_mutex_t fieldlist_mutex;

matchfile* mf;
pthread_mutex_t matchfile_mutex;

idfile* id;
quadfile* quads;
kdtree_t *codetree;
xylist* xyls;
kdtree_t* startree;

bool circle;

int* inverse_perm = NULL;

int nverified;

int main(int argc, char *argv[]) {
    uint numfields;
	char* progname = argv[0];
	int i;
	int err;

    if (argc != 1) {
		printHelp(progname);
		exit(-1);
    }

	printf("Running on host:\n");
	fflush(stdout);
	system("hostname");
	printf("\n");
	fflush(stdout);

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
		xcolname = strdup("ROWC");
		ycolname = strdup("COLC");
		parity = DEFAULT_PARITY_FLIP;
		codetol = DEFAULT_CODE_TOL;
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

		il_remove_all(fieldlist);

		quads = NULL;
		startree = NULL;
		nverified = 0;

		if (read_parameters())
			break;

		fprintf(stderr, "%s params:\n", progname);
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
		fprintf(stderr, "threads %i\n", threads);

		fprintf(stderr, "fields ");
		for (i=0; i<il_size(fieldlist); i++)
			fprintf(stderr, "%i ", il_get(fieldlist, i));
		fprintf(stderr, "\n");

		if (!treefname || !fieldfname || (codetol < 0.0) || !matchfname) {
			fprintf(stderr, "Invalid params... this message is useless.\n");
			exit(-1);
		}

		mf = matchfile_open_for_writing(matchfname);
		if (!mf) {
			fprintf(stderr, "Failed to open file %s to write match file.\n", matchfname);
			exit(-1);
		}
		if (matchfile_write_header(mf)) {
			fprintf(stderr, "Failed to write matchfile header.\n");
			exit(-1);
		}
		
		// Read .xyls file...
		fprintf(stderr, "Reading fields file %s...", fieldfname);
		fflush(stderr);
		xyls = xylist_open(fieldfname);
		if (!xyls) {
			fprintf(stderr, "Failed to read xylist.\n");
			exit(-1);
		}
		numfields = xyls->nfields;
		fprintf(stderr, "got %u fields.\n", numfields);
		if (parity) {
			fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");
			xyls->parity = 1;
		}
		xyls->xname = xcolname;
		xyls->yname = ycolname;

		// Read .ckdt file...
		fprintf(stderr, "Reading code KD tree from %s...\n", treefname);
		fflush(stderr);
		codetree = kdtree_fits_read_file(treefname);
		if (!codetree)
			exit(-1);
		fprintf(stderr, "  (%d quads, %d nodes, dim %d).\n",
				codetree->ndata, codetree->nnodes, codetree->ndim);

		// Read .quad file...
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

		fprintf(stderr, "Index scale: %g arcmin, %g arcsec\n", index_scale/60.0, index_scale);

		// Read .skdt file...
		fprintf(stderr, "Reading star KD tree from %s...\n", startreefname);
		fflush(stderr);
		startree = kdtree_fits_read_file(startreefname);
		if (!startree) {
			fprintf(stderr, "Failed to read star kdtree %s\n", startreefname);
			exit(-1);
		}
		fprintf(stderr, "  (%d stars, %d nodes, dim %d).\n",
				startree->ndata, startree->nnodes, startree->ndim);

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
		fprintf(stderr, "ckdt %s the CIRCLE header.\n",
				(circle ? "contains" : "does not contain"));

		if (solvedserver) {
			if (solvedclient_set_server(solvedserver)) {
				fprintf(stderr, "Error setting solvedserver.\n");
				exit(-1);
			}
		}

		id = idfile_open(idfname, 0);
		if (!id) {
			fprintf(stderr, "Couldn't open id file %s.\n", idfname);
			//exit(-1);
		}

		if (startree->perm) {
			fprintf(stderr, "Computing inverse permutation...\n");
			inverse_perm = malloc(startree->ndata * sizeof(int));
			kdtree_inverse_permutation(startree, inverse_perm);
		}

		// Do it!
		solve_fields();

		if (donefname) {
			FILE* batchfid = NULL;
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
			fprintf(stderr, "Error closing matchfile.\n");
		}
		kdtree_close(codetree);
		kdtree_close(startree);
		if (id)
			idfile_close(id);
		quadfile_close(quads);

		free(donefname);
		free(solvedfname);
		free(solvedserver);
		free(matchfname);
		free(xcolname);
		free(ycolname);
		free_fn(fieldfname);
		free_fn(treefname);
		free_fn(quadfname);
		free_fn(idfname);
		free_fn(startreefname);
		free(inverse_perm);

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

static int is_word(char* cmdline, char* keyword, char** cptr) {
	int len = strlen(keyword);
	if (strncmp(cmdline, keyword, len))
		return 0;
	*cptr = cmdline + len;
	return 1;
}

static int read_parameters() {
	for (;;) {
		char buffer[10240];
		char* nextword;
		fprintf(stderr, "\nAwaiting your command:\n");
		fflush(stderr);
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			return -1;
		}
		// strip off newline
		if (buffer[strlen(buffer) - 1] == '\n')
			buffer[strlen(buffer) - 1] = '\0';

		fprintf(stderr, "Command: %s\n", buffer);
		fflush(stderr);

		if (is_word(buffer, "help", &nextword)) {
			fprintf(stderr, "Commands:\n"
					"    index <index-file-name>\n"
					"    match <match-file-name>\n"
					"    done <done-file-name>\n"
					"    field <field-file-name>\n"
					"    solvedfname <solved-filename>\n"
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
					"    run\n"
					"    help\n"
					"    quit\n");
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
			double d = atof(nextword);
			verify_dist2 = arcsec2distsq(d);
		} else if (is_word(buffer, "nagree_toverify ", &nextword)) {
			nagree_toverify = atoi(nextword);
		} else if (is_word(buffer, "overlap_tosolve ", &nextword)) {
			overlap_tosolve = atof(nextword);
		} else if (is_word(buffer, "overlap_tokeep ", &nextword)) {
			overlap_tokeep = atof(nextword);
		} else if (is_word(buffer, "min_ninfield ", &nextword)) {
			min_ninfield = atoi(nextword);
		} else if (is_word(buffer, "field ", &nextword)) {
			char* fname = nextword;
			fieldfname = mk_fieldfn(fname);
		} else if (is_word(buffer, "fieldid ", &nextword)) {
			fieldid = atoi(nextword);
		} else if (is_word(buffer, "match ", &nextword)) {
			char* fname = nextword;
			matchfname = strdup(fname);
		} else if (is_word(buffer, "done ", &nextword)) {
			char* fname = nextword;
			donefname = strdup(fname);
		} else if (is_word(buffer, "solved ", &nextword)) {
			char* fname = nextword;
			solvedfname = strdup(fname);
		} else if (is_word(buffer, "solvedserver ", &nextword)) {
			solvedserver = strdup(nextword);
		} else if (is_word(buffer, "sdepth ", &nextword)) {
			int d = atoi(nextword);
			startdepth = d;
		} else if (is_word(buffer, "depth ", &nextword)) {
			int d = atoi(nextword);
			enddepth = d;
		} else if (is_word(buffer, "tol ", &nextword)) {
			double t = atof(nextword);
			codetol = t;
		} else if (is_word(buffer, "parity ", &nextword)) {
			int d = atoi(nextword);
			parity = (d ? TRUE : FALSE);
		} else if (is_word(buffer, "fieldunits_lower ", &nextword)) {
			double d = atof(nextword);
			funits_lower = d;
		} else if (is_word(buffer, "fieldunits_upper ", &nextword)) {
			double d = atof(nextword);
			funits_upper = d;
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
		fprintf(stderr, "Warning: cache was not empty at the end of the run.");
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

void verify(MatchObj* mo, double* field, int nfield, int fieldnum, int nagree) {
	int matches, unmatches, conflicts;
	verify_hit(startree, mo, field, nfield, verify_dist2,
			   &matches, &unmatches, &conflicts, NULL);
	fprintf(stderr, "    field %i (%i agree): overlap %4.1f%%: %i in field (%im/%iu/%ic)\n",
			fieldnum, nagree, 100.0 * mo->overlap, mo->ninfield, matches, unmatches, conflicts);
 	fflush(stderr);
	mo->nverified = nverified;
	nverified++;
}

int handlehit(solver_params* p, MatchObj* mo) {
	int listind;
	int n = 0;
	threadargs* my = p->userdata;

	assert(mo->timeused >= 0.0);

	// compute (x,y,z) center, scale, rotation.
	hitlist_healpix_compute_vector(mo);
	n = hitlist_healpix_add_hit(my->hits, mo, &listind);

	if (!do_verify)
		return n;

	if (n < nagree_toverify)
		return n;

	verify(mo, p->field, p->nfield, p->fieldnum, n);

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
					verify(mo1, p->field, p->nfield, p->fieldnum, n);
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
			fprintf(stderr, "Found a match that produces %4.1f%% overlapping stars.\n", 100.0 * mo->overlap);
			fflush(stderr);
			my->winning_listind = listind;
			p->quitNow = TRUE;
		}
	}

	return n;
}

static int next_field(xy** pfield) {
	static int index = 0;
	int rtn;

	if (pthread_mutex_lock(&fieldlist_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	if (index >= il_size(fieldlist))
		rtn = -1;
	else {
		rtn = il_get(fieldlist, index);
		index++;
		if (pfield)
			*pfield = xylist_get_field(xyls, rtn);
	}

	if (pthread_mutex_unlock(&fieldlist_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	return rtn;
}


void* solvethread_run(void* varg) {
	threadargs* my = varg;
	solver_params solver;
	double last_utime, last_stime;
	double utime, stime;
	int nfields;
	double* field = NULL;

	fprintf(stderr, "Thread %i starting.\n", my->threadnum);

	get_resource_stats(&last_utime, &last_stime, NULL);

	solver_default_params(&solver);
	solver.codekd = codetree;
	solver.endobj = enddepth;
	solver.maxtries = maxquads;
	solver.codetol = codetol;
	solver.handlehit = handlehit;
	solver.cxdx_margin = cxdx_margin;

	if (do_verify)
		my->verified = pl_new(32);
	else
		my->verified = NULL;

	if (funits_upper != 0.0) {
		solver.arcsec_per_pixel_upper = funits_upper;
		solver.minAB = index_scale_lower / funits_upper;
		fprintf(stderr, "Set minAB to %g\n", solver.minAB);
	}
	if (funits_lower != 0.0) {
		solver.arcsec_per_pixel_lower = funits_lower;
		solver.maxAB = index_scale / funits_lower;
		fprintf(stderr, "Set maxAB to %g\n", solver.maxAB);
	}

	nfields = xyls->nfields;

	for (;;) {
		xy *thisfield = NULL;
		int fieldnum;
		MatchObj template;
		int nfield;

		fieldnum = next_field(&thisfield);

		if (fieldnum == -1)
			break;
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
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

		fprintf(stderr, "    field %i: tried %i quads, matched %i codes.\n",
				fieldnum, solver.numtries, solver.nummatches);

		if (maxquads && solver.numtries >= maxquads) {
			fprintf(stderr, "    exceeded the number of quads to try: %i >= %i.\n",
					solver.numtries, maxquads);
		}

		if (my->winning_listind == -1) {
			// didn't solve it...
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
						verify(mo, solver.field, solver.nfield, solver.fieldnum, pl_size(list));
						if (do_verify)
							pl_append(my->verified, mo);
					}

					sumoverlap += mo->overlap;
					if (mo->overlap > maxoverlap)
						maxoverlap = mo->overlap;
				}
				fprintf(stderr, "Field %i: %i in agreement.  Overlap of winning cluster: max %f, avg %f\n",
						fieldnum, pl_size(list), maxoverlap, sumoverlap / (double)pl_size(list));
			} else
				fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, pl_size(list));
			
			// write 'em!
			if (do_verify)
				write_hits(fieldnum, my->verified);
			else
				write_hits(fieldnum, list);
			pl_free(list);

			if (solvedfname) {
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
		fprintf(stderr, "    spent %g s user, %g s system, %g s total.\n",
				(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime));
		last_utime = utime;
		last_stime = stime;
		fprintf(stderr, "\n\n");
	}

	free(field);
	pl_free(my->verified);

	fprintf(stderr, "Thread %i finished.\n", my->threadnum);
	my->running = FALSE;
	return 0;
}

void solve_fields() {
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
	if (inverse_perm)
		memcpy(sA, startree->data + inverse_perm[iA] * DIM_STARS,
			   DIM_STARS * sizeof(double));
	else
		memcpy(sA, startree->data + iA * DIM_STARS,
			   DIM_STARS * sizeof(double));
}

uint64_t getstarid(uint iA) {
	if (id)
		return idfile_get_anid(id, iA);
	return 0;
}
