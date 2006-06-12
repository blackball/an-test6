/**
 *   Solve fields (with slavish devotion)
 *
 * Inputs: .ckdt .quad (.skdt or .objs)
 * Output: .match
 */
#include <sys/types.h>
#include <sys/stat.h>
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
#include "kdtree_fits_io.h"
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "solver.h"
#include "solver_callbacks.h"
#include "matchobj.h"
#include "matchfile.h"
#include "catalog.h"
#include "hitlist_healpix.h"
#include "tic.h"
#include "quadfile.h"
#include "idfile.h"
#include "intmap.h"

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n", progname);
}

void solve_fields();

int read_parameters();

#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP FALSE

// params:
char *fieldfname = NULL, *treefname = NULL;
char *quadfname = NULL, *catfname = NULL;
char* startreefname = NULL;
char *idfname = NULL;
char* matchfname = NULL;
char* donefname = NULL;
char* solvedfname = NULL;
char* xcolname;
char* ycolname;
bool parity = DEFAULT_PARITY_FLIP;
double codetol = DEFAULT_CODE_TOL;

int startdepth = 0;
int enddepth = 0;

double funits_lower = 0.0;
double funits_upper = 0.0;
double index_scale;
double index_scale_lower;
double index_scale_lower_factor = 0.0;

bool agreement = FALSE;
int nagree = 4;
int maxnagree = 0;
double agreetol = 0.0;

bool do_verify = FALSE;
int nagree_toverify = 0;
double verify_dist2 = 0.0;
int noverlap_tosolve = 0;
int noverlap_toconfirm = 0;

int threads = 1;
il* fieldlist = NULL;
pthread_mutex_t fieldlist_mutex;

matchfile* mf;
pthread_mutex_t matchfile_mutex;

catalog* cat;
idfile* id;
quadfile* quads;
kdtree_t *codetree;
xylist* xyls;
kdtree_t* startree;

int* inverse_perm = NULL;

// histogram of the size of agreement clusters.
int *agreesizehist;
int Nagreehist;


char* get_pathname(char* fname) {
	//char resolved[PATH_MAX];
	char* resolved = malloc(PATH_MAX);
	if (!realpath(fname, resolved)) {
		fprintf(stderr, "Couldn't resolve real path of %s: %s\n", fname, strerror(errno));
		return NULL;
	}
	resolved = realloc(resolved, strlen(resolved) + 1);
	return resolved;
	//return strdup(resolved);
}

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
		startreefname = NULL;
		quadfname = NULL;
		catfname = NULL;
		idfname = NULL;
		matchfname = NULL;
		donefname = NULL;
		solvedfname = NULL;
		parity = DEFAULT_PARITY_FLIP;
		codetol = DEFAULT_CODE_TOL;
		startdepth = 0;
		enddepth = 0;
		il_remove_all(fieldlist);
		funits_lower = 0.0;
		funits_upper = 0.0;
		index_scale = 0.0;
		index_scale_lower_factor = 0.0;
		agreement = FALSE;
		nagree = 4;
		maxnagree = 0;
		agreetol = 0.0;
		cat = NULL;
		quads = NULL;
		startree = NULL;
		xcolname = strdup("ROWC");
		ycolname = strdup("COLC");
		verify_dist2 = 0.0;
		nagree_toverify = 0;
		noverlap_toconfirm = 0;
		noverlap_tosolve = 0;

		if (read_parameters()) {
			break;
		}

		if (agreement && (agreetol <= 0.0)) {
			fprintf(stderr, "If you set 'agreement', you must set 'agreetol'.\n");
			exit(-1);
		}

		fprintf(stderr, "%s params:\n", progname);
		fprintf(stderr, "fieldfname %s\n", fieldfname);
		fprintf(stderr, "treefname %s\n", treefname);
		fprintf(stderr, "startreefname %s\n", startreefname);
		fprintf(stderr, "quadfname %s\n", quadfname);
		fprintf(stderr, "catfname %s\n", catfname);
		fprintf(stderr, "idfname %s\n", idfname);
		fprintf(stderr, "matchfname %s\n", matchfname);
		fprintf(stderr, "donefname %s\n", donefname);
		fprintf(stderr, "solvedfname %s\n", solvedfname);
		fprintf(stderr, "parity %i\n", parity);
		fprintf(stderr, "codetol %g\n", codetol);
		fprintf(stderr, "startdepth %i\n", startdepth);
		fprintf(stderr, "enddepth %i\n", enddepth);
		fprintf(stderr, "fieldunits_lower %g\n", funits_lower);
		fprintf(stderr, "fieldunits_upper %g\n", funits_upper);
		fprintf(stderr, "index_lower %g\n", index_scale_lower_factor);
		fprintf(stderr, "agreement %i\n", agreement);
		fprintf(stderr, "num-to-agree %i\n", nagree);
		fprintf(stderr, "max-num-to-agree %i\n", maxnagree);
		fprintf(stderr, "agreetol %g\n", agreetol);
		fprintf(stderr, "verify_dist %g\n", rad2arcsec(distsq2arc(verify_dist2)));
		fprintf(stderr, "nagree_toverify %i\n", nagree_toverify);
		fprintf(stderr, "noverlap_toconfirm %i\n", noverlap_toconfirm);
		fprintf(stderr, "noverlap_tosolve %i\n", noverlap_tosolve);
		fprintf(stderr, "xcolname %s\n", xcolname);
		fprintf(stderr, "ycolname %s\n", ycolname);

		fprintf(stderr, "fields ");
		for (i=0; i<il_size(fieldlist); i++)
			fprintf(stderr, "%i ", il_get(fieldlist, i));
		fprintf(stderr, "\n");

		if (!treefname || !fieldfname || (codetol < 0.0) || !matchfname) {
			fprintf(stderr, "Invalid params... this message is useless.\n");
			exit(-1);
		}

		if (!agreement && (threads > 1)) {
			fprintf(stderr, "Can't multi-thread when !agreement\n");
			threads = 1;
		}
		fprintf(stderr, "threads %i\n", threads);

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
		fprintf(stderr, "Reading code KD tree from %s...", treefname);
		fflush(stderr);
		codetree = kdtree_fits_read_file(treefname);
		if (!codetree)
			exit(-1);
		fprintf(stderr, "done\n    (%d quads, %d nodes, dim %d).\n",
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

        if ((index_scale_lower != 0.0) && (index_scale_lower_factor != 0.0) &&
            ((index_scale_lower_factor * index_scale) != index_scale_lower)) {
            fprintf(stderr, "You specified an index_scale, but the quad file contained a different index_scale_lower entry.\n");
			fprintf(stderr, "Quad file: %g.  Your spec: %g\n", index_scale_lower, index_scale_lower_factor * index_scale);
            fprintf(stderr, "Overriding your specification.\n");
        }
        if ((index_scale_lower == 0.0) && (index_scale_lower_factor != 0.0)) {
            index_scale_lower = index_scale * index_scale_lower_factor;
        }

		fprintf(stderr, "Index scale: %g arcmin, %g arcsec\n", index_scale/60.0, index_scale);

		// Read .skdt file...
		fprintf(stderr, "Reading star KD tree from %s...", startreefname);
		fflush(stderr);
		startree = kdtree_fits_read_file(startreefname);
		if (!startree)
			fprintf(stderr, "Star kdtree not found or failed to read.\n");
		else
			fprintf(stderr, "done\n");

		do_verify = startree && (verify_dist2 > 0.0);

		if (startree) {
			inverse_perm = malloc(startree->ndata * sizeof(int));
			for (i=0; i<startree->ndata; i++)
				inverse_perm[startree->perm[i]] = i;
			cat = NULL;
		} else {
			// Read .objs file...
			cat = catalog_open(catfname, 0);
			if (!cat) {
				fprintf(stderr, "Couldn't open catalog %s.\n", catfname);
				exit(-1);
			}
		}

		id = idfile_open(idfname, 0);
		if (!id) {
			fprintf(stderr, "Couldn't open id file %s.\n", idfname);
			//exit(-1);
		}

		Nagreehist = 100;
		agreesizehist = malloc(Nagreehist * sizeof(int));
		for (i=0; i<Nagreehist; i++)
			agreesizehist[i] = 0;

		// Do it!
		solve_fields();

		if (donefname) {
			FILE* batchfid = NULL;
			fprintf(stderr, "Writing marker file %s...\n", donefname);
			fopenout(donefname, batchfid);
			fclose(batchfid);
		}
		free(donefname);
		free(solvedfname);
		free(matchfname);

		free(xcolname);
		free(ycolname);

		xylist_close(xyls);

		matchfile_close(mf);

		free_fn(fieldfname);

		free_fn(treefname);
		free_fn(quadfname);
		free_fn(catfname);
		free_fn(idfname);
		free_fn(startreefname);

		kdtree_close(codetree);
		if (startree)
			kdtree_close(startree);
		if (cat)
			catalog_close(cat);
		if (inverse_perm)
			free(inverse_perm);
		if (id)
			idfile_close(id);
		quadfile_close(quads);

		{
			int maxagree = 0;
			for (i=0; i<Nagreehist; i++)
				if (agreesizehist[i])
					maxagree = i;
			fprintf(stderr, "Agreement cluster size histogram:\n");
			fprintf(stderr, "nagreehist_total=[");
			for (i=0; i<=maxagree; i++)
				fprintf(stderr, "%i,", agreesizehist[i]);
			fprintf(stderr, "];\n");

			free(agreesizehist);
		}

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

inline int is_word(char* cmdline, char* keyword, char** cptr) {
	int len = strlen(keyword);
	if (strncmp(cmdline, keyword, len))
		return 0;
	*cptr = cmdline + len;
	return 1;
}

int read_parameters() {
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
					"    solvedfname <solved-field-filename-template>\n"
					"    fields [<field-number> or <start range>/<end range>...]\n"
					"    sdepth <start-field-object>\n"
					"    depth <end-field-object>\n"
					"    parity <0 || 1>\n"
					"    tol <code-tolerance>\n"
					"    fieldunits_lower <arcsec-per-pixel>\n"
					"    fieldunits_upper <arcsec-per-pixel>\n"
					"    index_lower <index-size-lower-bound-fraction>\n"
					"    agreement\n"
					"    nagree <min-to-agree>\n"
					"    maxnagree <max-to-agree>\n"
					"    agreetol <agreement-tolerance (arcsec)>\n"
					"    verify_dist <early-verification-dist (arcsec)>\n"
					"    nagree_toverify <nagree>\n"
					"    noverlap_tosolve <noverlap>\n"
					"    noverlap_toconfirm <noverlap>\n"
					"    run\n"
					"    help\n"
					"    quit\n");
		} else if (is_word(buffer, "xcol ", &nextword)) {
			xcolname = strdup(nextword);
		} else if (is_word(buffer, "ycol ", &nextword)) {
			ycolname = strdup(nextword);
		} else if (is_word(buffer, "agreement", &nextword)) {
			agreement = TRUE;
		} else if (is_word(buffer, "threads ", &nextword)) {
			threads = atoi(nextword);
		} else if (is_word(buffer, "nagree ", &nextword)) {
			nagree = atoi(nextword);
		} else if (is_word(buffer, "maxnagree ", &nextword)) {
			maxnagree = atoi(nextword);
		} else if (is_word(buffer, "agreetol ", &nextword)) {
			agreetol = atof(nextword);
		} else if (is_word(buffer, "index ", &nextword)) {
			char* fname = nextword;
			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			catfname = mk_catfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);
		} else if (is_word(buffer, "verify_dist ", &nextword)) {
			double d = atof(nextword);
			verify_dist2 = arcsec2distsq(d);
		} else if (is_word(buffer, "nagree_toverify ", &nextword)) {
			nagree_toverify = atoi(nextword);
		} else if (is_word(buffer, "noverlap_tosolve ", &nextword)) {
			noverlap_tosolve = atoi(nextword);
		} else if (is_word(buffer, "noverlap_toconfirm ", &nextword)) {
			noverlap_toconfirm = atoi(nextword);
		} else if (is_word(buffer, "field ", &nextword)) {
			char* fname = nextword;
			fieldfname = mk_fieldfn(fname);
		} else if (is_word(buffer, "match ", &nextword)) {
			char* fname = nextword;
			matchfname = strdup(fname);
		} else if (is_word(buffer, "done ", &nextword)) {
			char* fname = nextword;
			donefname = strdup(fname);
		} else if (is_word(buffer, "solved ", &nextword)) {
			char* fname = nextword;
			solvedfname = strdup(fname);
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
		} else if (is_word(buffer, "index_lower ", &nextword)) {
			double d = atof(nextword);
			index_scale_lower_factor = d;
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

int verify_hit(MatchObj* mo, solver_params* p, int nagree) {
	xy* field = p->field;
	int i, NF;
	double* fieldstars;
	intmap* map;
	int matches;
	int unmatches;
	int conflicts;
	double avgmatch;
	double maxmatch;
	assert(mo->transform_valid);
	assert(startree);
	NF = xy_size(field);
	fieldstars = malloc(3 * NF * sizeof(double));
	for (i=0; i<NF; i++) {
		double u, v;
		u = xy_refx(field, i);
		v = xy_refy(field, i);
		image_to_xyz(u, v, fieldstars + 3*i, mo->transform);
	}

	matches = unmatches = conflicts = 0;
	maxmatch = avgmatch = 0.0;
	map = intmap_new(INTMAP_ONE_TO_ONE);
	for (i=0; i<NF; i++) {
		double bestd2;
		int ind = kdtree_nearest_neighbour(startree, fieldstars + 3*i, &bestd2);
		if (bestd2 <= verify_dist2) {
			if (intmap_add(map, ind, i) == -1)
				// a field object already selected star 'ind' as its nearest neighbour.
				conflicts++;
			else
				matches++;
			avgmatch += sqrt(bestd2);
			if (bestd2 > maxmatch)
				maxmatch = bestd2;
		} else
			unmatches++;
	}
	avgmatch /= (double)(conflicts + matches);
	
	fprintf(stderr, "    field %i (%i agree): verifying: %i matches, %i unmatches, %i conflicts.\n",
			p->fieldnum, nagree, matches, unmatches, conflicts);
	//Avg match dist: %g arcsec, max dist %g arcsec\n",
	//rad2arcsec(distsq2arc(square(avgmatch))),
	//rad2arcsec(distsq2arc(maxmatch)));
	fflush(stderr);

	mo->noverlap = matches - conflicts;

	/*
	  {
	  // estimate the center (as centroid) and radius of the field.
	  // count the index stars in that region.
	  double fieldcenter[3];
	  double fieldrad2 = 0.0;
	  double r;
	  int rc;
	  fieldcenter[0] = fieldcenter[1] = fieldcenter[2] = 0.0;
	  for (i=0; i<NF; i++) {
	  fieldcenter[0] += fieldstars[i*3 + 0];
	  fieldcenter[1] += fieldstars[i*3 + 1];
	  fieldcenter[2] += fieldstars[i*3 + 2];
	  }
	  // normalize
	  r = sqrt(square(fieldcenter[0]) + square(fieldcenter[1]) + square(fieldcenter[2]));
	  fieldcenter[0] /= r;
	  fieldcenter[1] /= r;
	  fieldcenter[2] /= r;
	  for (i=0; i<NF; i++) {
	  double d2 = distsq(fieldcenter, fieldstars + 3*i, 3);
	  if (d2 > fieldrad2)
	  fieldrad2 = d2;
	  }
	  rc = kdtree_rangecount(startree, fieldcenter, fieldrad2);
	  printf("Index has %i stars in the region.\n", rc);
	  }
	*/

	intmap_free(map);
	free(fieldstars);
	return 1;
}

struct solvethread_args {
	matchfile_entry me;
	int winning_listind;
	int threadnum;
	bool running;
	hitlist* hits;
};
typedef struct solvethread_args threadargs;

struct cached_hits {
	int fieldnum;
	matchfile_entry me;
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

// if "me" and "matches" are NULL, marks "fieldnum" as having been done.
static void write_hits(int fieldnum, matchfile_entry* me, pl* matches) {
	static int index = 0;
	static bl* cached = NULL;
	int k, nextfld;

	if (!cached)
		cached = bl_new(16, sizeof(cached_hits));

	if (pthread_mutex_lock(&matchfile_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	// DEBUG - ensure cache is empty.
	if (fieldnum == -1) {
		cached_hits* cache;
		if (!bl_size(cached))
			return;
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
				if (matchfile_start_table(mf, &cache->me) ||
					matchfile_write_table(mf)) {
					fprintf(stderr, "Error: Failed to write matchfile table.\n");
				}
				for (k=0; k<pl_size(cache->matches); k++) {
					MatchObj* mo = pl_get(cache->matches, k);
					if (matchfile_write_match(mf, mo))
						fprintf(stderr, "Error writing a match.\n");
				}
				if (matchfile_fix_table(mf)) {
					fprintf(stderr, "Error: Failed to fix matchfile table.\n");
				}
				for (k=0; k<pl_size(cache->matches); k++) {
					MatchObj* mo = pl_get(cache->matches, k);
					free(mo);
				}
				pl_free(cache->matches);
			}
			free(cache->me.indexpath);
			free(cache->me.fieldpath);
		}
		return;
	}

	nextfld = il_get(fieldlist, index);

	/*
	  fprintf(stderr, "Write_hits: fieldnum=%i.\n", fieldnum);
	  fprintf(stderr, "Cache: [ ");
	  for (k=0; k<bl_size(cached); k++) {
	  cached_hits* ch = bl_access(cached, k);
	  fprintf(stderr, "%i ", ch->fieldnum);
	  }
	  fprintf(stderr, "]\n");
	  fprintf(stderr, "nextfld=%i\n", nextfld);
	*/

	if (nextfld == fieldnum) {
		cached_hits ch;
		cached_hits* cache;
		bool freeit = FALSE;

		ch.fieldnum = fieldnum;
		if (me)
			memcpy(&ch.me, me, sizeof(matchfile_entry));
		else {
			ch.me.indexpath = NULL;
			ch.me.fieldpath = NULL;
		}
		ch.matches = matches;
		cache = &ch;

		for (;;) {
			// write it!

			/*
			  fprintf(stderr, "Writing field %i: matches=%p, m_e=(%s,%s).\n",
			  cache->fieldnum, cache->matches,
			  cache->me.indexpath, cache->me.fieldpath);
			*/

			if (cache->matches) {
				if (matchfile_start_table(mf, &cache->me) ||
					matchfile_write_table(mf)) {
					fprintf(stderr, "Error: Failed to write matchfile table.\n");
				}
				for (k=0; k<pl_size(cache->matches); k++) {
					MatchObj* mo = pl_get(cache->matches, k);
					if (matchfile_write_match(mf, mo))
						fprintf(stderr, "Error writing a match.\n");
				}
				if (matchfile_fix_table(mf)) {
					fprintf(stderr, "Error: Failed to fix matchfile table.\n");
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
				free(cache->me.indexpath);
				free(cache->me.fieldpath);
				bl_remove_index(cached, 0);
			}
			freeit = TRUE;
			if (bl_size(cached) == 0)
				break;
			nextfld = il_get(fieldlist, index);
			cache = bl_access(cached, 0);

			//fprintf(stderr, "Nextfld=%i, cached field=%i.\n", nextfld, cache->fieldnum);

			if (cache->fieldnum != nextfld)
				break;
		}
	} else {
		// cache it!
		cached_hits cache;
		// deep copy
		cache.fieldnum = fieldnum;
		if (me) {
			memcpy(&cache.me, me, sizeof(matchfile_entry));
			cache.me.indexpath = strdup(me->indexpath);
			cache.me.fieldpath = strdup(me->fieldpath);
		} else {
			memset(&cache.me, 0, sizeof(matchfile_entry));
		}
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

		/*
		  fprintf(stderr, "Caching field %i: matches=%p, m_e=(%s,%s).\n",
		  cache.fieldnum, cache.matches, 
		  cache.me.indexpath, cache.me.fieldpath);
		*/

		bl_insert_sorted(cached, &cache, cached_hits_compare);
	}

	/*
	  fprintf(stderr, "Cache: [ ");
	  for (k=0; k<bl_size(cached); k++) {
	  cached_hits* ch = bl_access(cached, k);
	  fprintf(stderr, "%i ", ch->fieldnum);
	  }
	  fprintf(stderr, "]\n");
	*/

	if (pthread_mutex_unlock(&matchfile_mutex)) {
		fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errno));
		exit(-1);
	}

	return;
}

int handlehit(solver_params* p, MatchObj* mo) {
	int listind;
	int n = 0;
	bool winner = FALSE;
	threadargs* my = p->userdata;

	if (!agreement) {
		if (matchfile_write_match(mf, mo)) {
			fprintf(stderr, "Failed to write matchfile entry.\n");
		}
		free_MatchObj(mo);
		return 1;
	}

	// share this struct between all the matches for this field...
	mo->extra = &(my->me);
	// compute (x,y,z) center, scale, rotation.
	hitlist_healpix_compute_vector(mo);
	n = hitlist_healpix_add_hit(my->hits, mo, &listind);

	// did this match just join a potentially winning set of agreeing matches?
	/*
	  if (n >= nagree) {
	  winning_listind = listind;
	  p->quitNow = TRUE;
	  winner = TRUE;
	  }
	*/

	if (!do_verify)
		return n;

	if (n < nagree_toverify)
		return n;

	verify_hit(mo, p, n);

	winner = (n >= nagree);

	// does this winning set of agreeing matches need confirmation?
	if (winner && noverlap_toconfirm) {
		if (mo->noverlap >= noverlap_toconfirm) {
			// enough stars overlap to confirm this set of agreeing matches.
			my->winning_listind = listind;
			p->quitNow = TRUE;
		} else {

			if (n == nagree_toverify) {
				// HACK - should check the other matches to see if one
				// of them could confirm.
			}

			// veto!
			fprintf(stderr, "Veto: found %i agreeing matches, but verification failed (%i overlaps < %i required).\n",
					n, mo->noverlap, noverlap_toconfirm);
			fflush(stderr);
			p->quitNow = FALSE;
			my->winning_listind = -1;
		}
		return n;
	}

	if (noverlap_tosolve && (mo->noverlap >= noverlap_tosolve)) {
		// this single hit causes enough overlaps to solve the field.
		fprintf(stderr, "Found a match that produces %i overlapping stars.\n", mo->noverlap);
		fflush(stderr);
		my->winning_listind = listind;
		p->quitNow = TRUE;
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
	char* path;

	fprintf(stderr, "Thread %i starting.\n", my->threadnum);

	get_resource_stats(&last_utime, &last_stime, NULL);

	solver_default_params(&solver);

	solver.codekd = codetree;
	solver.endobj = enddepth;
	solver.maxtries = 0;
	solver.max_matches_needed = maxnagree;
	solver.codetol = codetol;
	solver.cornerpix = mk_xy(2);
	solver.handlehit = handlehit;

	my->me.parity = parity;
	path = get_pathname(treefname);
	if (path)
		my->me.indexpath = path;
	else
		my->me.indexpath = treefname;
	path = get_pathname(fieldfname);
	if (path)
		my->me.fieldpath = path;
	else
		my->me.fieldpath = fieldfname;
	my->me.codetol = codetol;
	/*
	  my->me.fieldunits_lower = funits_lower;
	  my->me.fieldunits_upper = funits_upper;
	*/

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

		fieldnum = next_field(&thisfield);

		if (fieldnum == -1)
			break;
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			continue;
		}
		if (!thisfield) {
			// HACK - why is this happening? QFITS + multithreading interaction bug?
			fprintf(stderr, "Couldn't get field %i\n", fieldnum);
			write_hits(fieldnum, NULL, NULL);
			continue;
		}

		if (solvedfname) {
			char fn[256];
			struct stat st;
			sprintf(fn, solvedfname, fieldnum);
			if (stat(fn, &st) == 0) {
				// file exists; field has already been solved.
				fprintf(stderr, "Field %i: file %s exists; field has been solved.\n",
						fieldnum, fn);
				write_hits(fieldnum, NULL, NULL);
				continue;
			}
		}

		solver.fieldnum = fieldnum;
		solver.numtries = 0;
		solver.nummatches = 0;
		solver.mostagree = 0;
		solver.startobj = startdepth;
		solver.field = thisfield;
		solver.quitNow = FALSE;
		solver.userdata = my;

		my->me.fieldnum = fieldnum;
		my->winning_listind = -1;

		if (agreement)
			my->hits = hitlist_healpix_new(agreetol);
		else
			my->hits = NULL;

		if (!agreement) {
			if (matchfile_start_table(mf, &(my->me)) ||
				matchfile_write_table(mf)) {
				fprintf(stderr, "Error: Failed to write matchfile table.\n");
			}
		}

		// The real thing
		solve_field(&solver);

		fprintf(stderr, "    field %i: tried %i quads, matched %i codes.\n\n",
				fieldnum, solver.numtries, solver.nummatches);

		if (agreement) {
			int* thisagreehist;
			int maxagree = 0;
			int k;
			thisagreehist = calloc(Nagreehist, sizeof(int));
			hitlist_healpix_histogram_agreement_size(my->hits, thisagreehist, Nagreehist);
			for (k=0; k<Nagreehist; k++)
				if (thisagreehist[k]) {
					// global total...
					agreesizehist[k] += thisagreehist[k];
					maxagree = k;
				}
			fprintf(stderr, "Agreement cluster size histogram:\n");
			fprintf(stderr, "nagreehist=[");
			for (k=0; k<=maxagree; k++)
				fprintf(stderr, "%i,", thisagreehist[k]);
			fprintf(stderr, "];\n");
			free(thisagreehist);
			thisagreehist = NULL;


			if (my->winning_listind == -1) {
				// didn't solve it...
				fprintf(stderr, "Field %i is unsolved.\n", fieldnum);
				write_hits(fieldnum, NULL, NULL);
			} else {
				int maxoverlap = 0;
				int sumoverlap = 0;
				pl* list = hitlist_healpix_copy_list(my->hits, my->winning_listind);
				if (do_verify) {
					int k;
					// run verification on any of the matches that haven't
					// already been done.
					for (k=0; k<pl_size(list); k++) {
						MatchObj* mo = pl_get(list, k);
						if (!mo->noverlap)
							verify_hit(mo, &solver, pl_size(list));

						sumoverlap += mo->noverlap;
						if (mo->noverlap > maxoverlap)
							maxoverlap = mo->noverlap;
					}
				}
				if (do_verify)
					fprintf(stderr, "Field %i: %i in agreement.  Overlap of winning cluster: max %i, avg %g\n",
							fieldnum, pl_size(list), maxoverlap, sumoverlap / (double)pl_size(list));
				else
					fprintf(stderr, "Field %i: %i in agreement.\n",
							fieldnum, pl_size(list));

				// write 'em!
				write_hits(fieldnum, &my->me, list);
				pl_free(list);

				if (solvedfname) {
					// write a file to indicate that the field was solved.
					char fn[256];
					FILE* f;
					sprintf(fn, solvedfname, fieldnum);
					fprintf(stderr, "Field %i solved: writing file %s to indicate this.\n", fieldnum, fn);
					if (!(f = fopen(fn, "w")) ||
						fclose(f)) {
						fprintf(stderr, "Failed to write field-finished file %s.\n", fn);
					}
				}
			}

			hitlist_healpix_clear(my->hits);
			hitlist_healpix_free(my->hits);
		}

		free_xy(thisfield);

		if (!agreement) {
			if (matchfile_fix_table(mf)) {
				fprintf(stderr, "Error: Failed to fix matchfile table.\n");
			}
		}

		get_resource_stats(&utime, &stime, NULL);
		fprintf(stderr, "    spent %g s user, %g s system, %g s total.\n",
				(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime));
		last_utime = utime;
		last_stime = stime;
	}

	free(my->me.indexpath);
	free(my->me.fieldpath);
	free_xy(solver.cornerpix);

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
	write_hits(-1, NULL, NULL);

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
	if (cat)
		memcpy(sA, catalog_get_star(cat, iA), DIM_STARS * sizeof(double));
	else
		memcpy(sA, startree->data + inverse_perm[iA] * DIM_STARS,
			   DIM_STARS * sizeof(double));
}

uint64_t getstarid(uint iA) {
	if (id)
		return idfile_get_anid(id, iA);
	return 0;
}
