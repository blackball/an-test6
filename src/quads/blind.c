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
 *   Solve fields blindly
 *
 * Inputs: .ckdt .quad .skdt
 * Output: .match .rdls .wcs
 */
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <ctype.h>

#include "qfits_error.h"
#include "qfits_cache.h"
#include "tweak_internal.h"

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "solver.h"
#include "solver_callbacks.h"
#include "matchobj.h"
#include "matchfile.h"
#include "tic.h"
#include "quadfile.h"
#include "idfile.h"
#include "solvedclient.h"
#include "solvedfile.h"
#include "starkd.h"
#include "codekd.h"
#include "boilerplate.h"
#include "fitsioutils.h"
#include "blind_wcs.h"
#include "rdlist.h"
#include "verify.h"

typedef __sighandler_t sighandler_t;

static void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n", progname);
}

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#define DEFAULT_CODE_TOL .01
#define DEFAULT_PARITY PARITY_BOTH
#define DEFAULT_TWEAK_ABORDER 3
#define DEFAULT_TWEAK_ABPORDER 3
#define DEFAULT_INDEX_JITTER 1.0  // arcsec

struct blind_params {
	solver_params solver;

	int nindex_tokeep;
	int nindex_tosolve;

	int nverify;

	double logratio_tokeep;
	double logratio_tosolve;

	// number of times we've run verification
	int nverified;

	// best hit that surpasses the "keep" requirements.
	bool have_bestmo;
	MatchObj bestmo;
	// does it also surpass the "solve" requirements?
	bool bestmo_solves;
	// best logodds encountered (even if we don't record bestmo)
	double bestlogodds;

	// Filenames
	char* indexname;
	char *fieldfname, *matchfname, *donefname, *startfname, *logfname;
	char *donescript;
	char* rdlsfname;
	char* indexrdlsfname;

	// filename template (sprintf format with %i for field number)
	char* wcs_template;

	char *solved_out;
	// Solvedserver ip:port
	char *solvedserver;
	// If using solvedserver, limits of fields to ask for
	int firstfield, lastfield;

	// Indexes to use (base filenames)
	pl* indexes;

	// Logfile
	FILE* logfd;
	int dup_stderr;
	int dup_stdout;
	bool silent;
	bool verbose;

	// Fields to try
	il* fieldlist;

	// xylist column names
	char *xcolname, *ycolname;
	// FITS keyword to copy from xylist to matchfile.
	char *fieldid_key;

	int indexid;
	int healpix;

	double verify_dist2;
	double verify_pix;

	// proportion of distractors (in [0,1])
	double distractors;

	double logratio_toprint;

	double logratio_tobail;

	matchfile* mf;
	idfile* id;
	quadfile* quads;
	xylist* xyls;
	startree* starkd;
	codetree* codekd;
	rdlist* rdls;
	rdlist* indexrdls;

	int cpulimit;
	int timelimit;

	int total_timelimit;
	bool hit_total_timelimit;

	bool do_tweak;
	int tweak_aborder;
	int tweak_abporder;
	bool tweak_skipshift;
};
typedef struct blind_params blind_params;

static void solve_fields(blind_params* bp);
static int read_parameters(blind_params* bp);
static void add_blind_params(blind_params* bp, qfits_header* hdr);
static int blind_handle_hit(solver_params* sp, MatchObj* mo);

static blind_params my_bp;

static void loglvl(int level, const blind_params* bp, const char* format, va_list va) {
	// 1=error
	if (bp->silent && level > 1)
		return;
	/*
	// 2=important
	if (bp->quiet && level > 2)
	return;
	*/
	// 3=normal
	if (!bp->verbose && level > 3)
		return;
	vfprintf(stderr, format, va);
	fflush(stderr);
}

static void
ATTRIB_FORMAT(printf,2,3)
logerr(const blind_params* bp, const char* format, ...) {
	va_list va;
	va_start(va, format);
	loglvl(1, bp, format, va);
	va_end(va);
}
static void
ATTRIB_FORMAT(printf,2,3)
logmsg(const blind_params* bp, const char* format, ...) {
	va_list va;
	va_start(va, format);
	loglvl(3, bp, format, va);
	va_end(va);
}
static void
ATTRIB_FORMAT(printf,2,3)
logverb(const blind_params* bp, const char* format, ...) {
	va_list va;
	va_start(va, format);
	loglvl(4, bp, format, va);
	va_end(va);
}

static void quit_now(char* msg) {
	logmsg(&my_bp, msg);
	my_bp.solver.quitNow = TRUE;
}

static void cpu_time_limit(int sig) {
	quit_now("CPU time limit reached!\n");
}

static void wall_time_limit(int sig) {
	quit_now("Wall-clock time limit reached!\n");
}

static void total_wall_time_limit(int sig) {
	quit_now("Total wall-clock time limit reached!\n");
	my_bp.hit_total_timelimit = TRUE;
}

int main(int argc, char *argv[]) {
	char* progname = argv[0];
    uint numfields;
	int i;
	blind_params* bp = &my_bp;
	solver_params* sp = &(bp->solver);

	if (argc == 2 && strcmp(argv[1],"-s") == 0) {
		bp->silent = TRUE;
		fprintf(stderr, "premptive silence\n");
	}

	if (argc != 1 && !bp->silent) {
		printHelp(progname);
		exit(-1);
	}

	qfits_err_statset(1);

	// Read input settings until "run" is encountered; repeat.
	for (;;) {
		int I;
		sighandler_t oldsigalarm_total = NULL;

		if (bp->hit_total_timelimit)
			break;

		tic();

		// Reset params.
		memset(bp, 0, sizeof(blind_params));
		solver_default_params(&(bp->solver));
		sp->userdata = bp;

		bp->nverify = 20;
		bp->logratio_tobail = -HUGE_VAL;
		bp->fieldlist = il_new(256);
		bp->indexes = pl_new(16);
		bp->fieldid_key = strdup("FIELDID");
		bp->xcolname = strdup("X");
		bp->ycolname = strdup("Y");
		bp->firstfield = -1;
		bp->lastfield = -1;
		bp->healpix = -1;
		bp->tweak_aborder  = DEFAULT_TWEAK_ABORDER;
		bp->tweak_abporder = DEFAULT_TWEAK_ABPORDER;
		sp->field_minx = sp->field_maxx = 0.0;
		sp->field_miny = sp->field_maxy = 0.0;
		sp->parity = DEFAULT_PARITY;
		sp->codetol = DEFAULT_CODE_TOL;

		sp->handlehit = blind_handle_hit;

		if (read_parameters(bp)) {
			il_free(bp->fieldlist);
			pl_free(bp->indexes);
			free(bp->xcolname);
			free(bp->ycolname);
			free(bp->fieldid_key);
			break;
		}

		if (!pl_size(bp->indexes)) {
			logerr(bp, "You must specify an index.\n");
			exit(-1);
		}
		if (!bp->fieldfname) {
			logerr(bp, "You must specify a field filename (xylist).\n");
			exit(-1);
		}
		if (sp->codetol < 0.0) {
			logerr(bp, "You must specify codetol > 0\n");
			exit(-1);
		}
		if ((((bp->verify_pix > 0.0)?1:0) +
			 ((bp->verify_dist2 > 0.0)?1:0)) != 1) {
			logerr(bp, "You must specify either verify_pix or verify_dist2.\n");
			exit(-1);
		}

		// If we're just solving one field, check to see if it's already
		// solved before doing a bunch of work and spewing tons of output.
		if ((il_size(bp->fieldlist) == 1) && (sp->solved_in)) {
			if (solvedfile_get(sp->solved_in, il_get(bp->fieldlist, 0))) {
				logmsg(bp, "Field %i is already solved.\n", il_get(bp->fieldlist, 0));
				il_free(bp->fieldlist);
				pl_free(bp->indexes);
				free(bp->xcolname);
				free(bp->ycolname);
				free(bp->fieldid_key);
				continue;
			}
		}

		// Early check to see if this job was cancelled.
		if (sp->cancelfname) {
			struct stat st;
			if (stat(sp->cancelfname, &st) == 0) {
				logmsg(bp, "Run cancelled.\n");
				il_free(bp->fieldlist);
				pl_free(bp->indexes);
				free(bp->xcolname);
				free(bp->ycolname);
				free(bp->fieldid_key);
				continue;
			}
		}

		logmsg(bp, "%s params:\n", progname);
		logmsg(bp, "fields ");
		for (i=0; i<il_size(bp->fieldlist); i++)
			logmsg(bp, "%i ", il_get(bp->fieldlist, i));
		logmsg(bp, "\n");
		logmsg(bp, "indexes:\n");
		for (i=0; i<pl_size(bp->indexes); i++)
			logmsg(bp, "  %s\n", (char*)pl_get(bp->indexes, i));
		logmsg(bp, "fieldfname %s\n", bp->fieldfname);
		logmsg(bp, "fieldid %i\n", sp->fieldid);
		logmsg(bp, "matchfname %s\n", bp->matchfname);
		logmsg(bp, "startfname %s\n", bp->startfname);
		logmsg(bp, "donefname %s\n", bp->donefname);
		logmsg(bp, "donescript %s\n", bp->donescript);
		logmsg(bp, "solved_in %s\n", sp->solved_in);
		logmsg(bp, "solved_out %s\n", bp->solved_out);
		logmsg(bp, "solvedserver %s\n", bp->solvedserver);
		logmsg(bp, "cancel %s\n", sp->cancelfname);
		logmsg(bp, "wcs %s\n", bp->wcs_template);
		logmsg(bp, "fieldid_key %s\n", bp->fieldid_key);
		logmsg(bp, "parity %i\n", sp->parity);
		logmsg(bp, "codetol %g\n", sp->codetol);
		logmsg(bp, "startdepth %i\n", sp->startobj);
		logmsg(bp, "enddepth %i\n", sp->endobj);
		logmsg(bp, "fieldunits_lower %g\n", sp->funits_lower);
		logmsg(bp, "fieldunits_upper %g\n", sp->funits_upper);
		logmsg(bp, "verify_dist %g\n", distsq2arcsec(bp->verify_dist2));
		logmsg(bp, "verify_pix %g\n", bp->verify_pix);
		logmsg(bp, "nindex_tokeep %i\n", bp->nindex_tokeep);
		logmsg(bp, "nindex_tosolve %i\n", bp->nindex_tosolve);
		logmsg(bp, "xcolname %s\n", bp->xcolname);
		logmsg(bp, "ycolname %s\n", bp->ycolname);
		logmsg(bp, "maxquads %i\n", sp->maxquads);
		logmsg(bp, "maxmatches %i\n", sp->maxmatches);
		logmsg(bp, "quiet %i\n", sp->quiet);
		logmsg(bp, "verbose %i\n", bp->verbose);
		logmsg(bp, "logfname %s\n", bp->logfname);
		logmsg(bp, "cpulimit %i\n", bp->cpulimit);
		logmsg(bp, "timelimit %i\n", bp->timelimit);
		logmsg(bp, "total_timelimit %i\n", bp->total_timelimit);
		logmsg(bp, "tweak %s\n", bp->do_tweak ? "on" : "off");
		if (bp->do_tweak) {
			logmsg(bp, "tweak_aborder %i\n", bp->tweak_aborder);
			logmsg(bp, "tweak_abporder %i\n", bp->tweak_abporder);
		}

		// Set total wall-clock time limit.
		// Note that we never cancel this alarm, it persists across "run"
		// boundaries.
		if (bp->total_timelimit) {
			alarm(bp->total_timelimit);
			oldsigalarm_total = signal(SIGALRM, total_wall_time_limit);
			if (oldsigalarm_total == SIG_ERR) {
				logerr(bp, "Failed to set total wall time limit signal handler: %s\n", strerror(errno));
				exit(-1);
			}
		}

		if (bp->matchfname) {
			bp->mf = matchfile_open_for_writing(bp->matchfname);
			if (!bp->mf) {
				logerr(bp, "Failed to open file %s to write match file.\n", bp->matchfname);
				exit(-1);
			}
			boilerplate_add_fits_headers(bp->mf->header);
			qfits_header_add(bp->mf->header, "HISTORY", "This file was created by the program \"blind\".", NULL, NULL);
			qfits_header_add(bp->mf->header, "DATE", qfits_get_datetime_iso8601(), "Date this file was created.", NULL);
			add_blind_params(bp, bp->mf->header);
			if (matchfile_write_header(bp->mf)) {
				logerr(bp, "Failed to write matchfile header.\n");
				exit(-1);
			}
		}

		// Read .xyls file...
		logmsg(bp, "Reading fields file %s...", bp->fieldfname);
		bp->xyls = xylist_open(bp->fieldfname);
		if (!bp->xyls) {
			logerr(bp, "Failed to read xylist.\n");
			exit(-1);
		}
		bp->xyls->xname = bp->xcolname;
		bp->xyls->yname = bp->ycolname;
		numfields = bp->xyls->nfields;
		logmsg(bp, "got %u fields.\n", numfields);

		if (bp->solvedserver) {
			if (solvedclient_set_server(bp->solvedserver)) {
				logerr(bp, "Error setting solvedserver.\n");
				exit(-1);
			}

			if ((il_size(bp->fieldlist) == 0) && (bp->firstfield != -1) && (bp->lastfield != -1)) {
				int j;
				il_free(bp->fieldlist);
				logmsg(bp, "Contacting solvedserver to get field list...\n");
				bp->fieldlist = solvedclient_get_fields(sp->fieldid, bp->firstfield, bp->lastfield, 0);
				if (!bp->fieldlist) {
					logerr(bp, "Failed to get field list from solvedserver.\n");
					exit(-1);
				}
				logmsg(bp, "Got %i fields from solvedserver: ", il_size(bp->fieldlist));
				for (j=0; j<il_size(bp->fieldlist); j++) {
					logmsg(bp, "%i ", il_get(bp->fieldlist, j));
				}
				logmsg(bp, "\n");
			}
		}

		if (bp->rdlsfname) {
			bp->rdls = rdlist_open_for_writing(bp->rdlsfname);
			if (bp->rdls) {
				boilerplate_add_fits_headers(bp->rdls->header);
				fits_add_long_history(bp->rdls->header, "This \"rdls\" file was created by the program \"blind\"."
									  "  It contains the RA/DEC of field objects after being transformed by the WCS.");
				qfits_header_add(bp->rdls->header, "DATE", qfits_get_datetime_iso8601(), "Date this file was created.", NULL);
				add_blind_params(bp, bp->rdls->header);
				if (rdlist_write_header(bp->rdls)) {
					logerr(bp, "Failed to write RDLS header.\n");
					rdlist_close(bp->rdls);
					bp->rdls = NULL;
				}
			} else {
				logerr(bp, "Failed to open RDLS file %s for writing.\n",
						bp->rdlsfname);
			}
		}
		if (bp->indexrdlsfname) {
			bp->indexrdls = rdlist_open_for_writing(bp->indexrdlsfname);
			if (bp->indexrdls) {
				boilerplate_add_fits_headers(bp->indexrdls->header);
				fits_add_long_history(bp->indexrdls->header, "This \"indexrdls\" file was created by the program \"blind\"."
									  "  It contains the RA/DEC of index objects that were found inside a solved field.");
				qfits_header_add(bp->indexrdls->header, "DATE", qfits_get_datetime_iso8601(), "Date this file was created.", NULL);
				add_blind_params(bp, bp->indexrdls->header);
				if (rdlist_write_header(bp->indexrdls)) {
					logerr(bp, "Failed to write index RDLS header.\n");
					rdlist_close(bp->indexrdls);
					bp->indexrdls = NULL;
				}
			} else {
				logerr(bp, "Failed to open index RDLS file %s for writing.\n",
						bp->indexrdlsfname);
			}
		}

		if (bp->startfname) {
			FILE* batchfid = NULL;
			logmsg(bp, "Writing marker file %s...\n", bp->startfname);
			batchfid = fopen(bp->startfname, "wb");
			if (batchfid)
				fclose(batchfid);
			else
				logerr(bp, "Failed to write marker file %s: %s\n", bp->startfname, strerror(errno));
		}

		for (I=0; I<pl_size(bp->indexes); I++) {
			char *idfname, *treefname, *quadfname, *startreefname;
			char* fname = pl_get(bp->indexes, I);
			sighandler_t oldsigcpu = NULL;
			sighandler_t oldsigalarm = NULL;
			double scalefudge = 0.0; // in pixels

			if (bp->hit_total_timelimit)
				break;

			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);
			bp->indexname = fname;

			// Read .ckdt file...
			logmsg(bp, "Reading code KD tree from %s...\n", treefname);
			bp->codekd = codetree_open(treefname);
			if (!bp->codekd)
				exit(-1);
			logmsg(bp, "  (%d quads, %d nodes, dim %d).\n",
				   codetree_N(bp->codekd), codetree_nodes(bp->codekd), codetree_D(bp->codekd));

			// Read .quad file...
			logmsg(bp, "Reading quads file %s...\n", quadfname);
			bp->quads = quadfile_open(quadfname, 0);
			if (!bp->quads) {
				logerr(bp, "Couldn't read quads file %s\n", quadfname);
				exit(-1);
			}
			sp->index_scale_upper = quadfile_get_index_scale_arcsec(bp->quads);
			sp->index_scale_lower = quadfile_get_index_scale_lower_arcsec(bp->quads);
			bp->indexid = bp->quads->indexid;
			bp->healpix = bp->quads->healpix;

			// See if index contains JITTER header... if so, set index_jitter to that value.
			{
				double ijitter = qfits_header_getdouble(bp->quads->header, "JITTER", 0.0);
				if (ijitter > 0.0) {
					sp->index_jitter = ijitter;
				} else {
					sp->index_jitter = DEFAULT_INDEX_JITTER;
				}
				logmsg(bp, "Setting index jitter to %g arcsec.\n", sp->index_jitter);
			}

			logmsg(bp, "Index scale: [%g, %g] arcmin, [%g, %g] arcsec\n",
				   sp->index_scale_lower/60.0, sp->index_scale_upper/60.0, sp->index_scale_lower, sp->index_scale_upper);

			// Read .skdt file...
			logmsg(bp, "Reading star KD tree from %s...\n", startreefname);
			bp->starkd = startree_open(startreefname);
			if (!bp->starkd) {
				logerr(bp, "Failed to read star kdtree %s\n", startreefname);
				exit(-1);
			}

			logmsg(bp, "  (%d stars, %d nodes, dim %d).\n",
				   startree_N(bp->starkd), startree_nodes(bp->starkd), startree_D(bp->starkd));

			// If the code kdtree has CXDX set, set cxdx_margin.
			if (qfits_header_getboolean(bp->codekd->header, "CXDX", 0))
				// 1.5 = sqrt(2) + fudge factor.
				sp->cxdx_margin = 1.5 * sp->codetol;

			// check for CIRCLE field in ckdt header...
			sp->circle = qfits_header_getboolean(bp->codekd->header, "CIRCLE", 0);

			logmsg(bp, "ckdt %s the CIRCLE header.\n",
				   (sp->circle ? "contains" : "does not contain"));

			// Read .id file...
			bp->id = idfile_open(idfname, 0);
			if (!bp->id) {
				logmsg(bp, "Couldn't open id file %s.\n", idfname);
				logmsg(bp, "(Note, this won't cause trouble; you just won't get star IDs for matching quads.)\n");
			}

			// Set index params
			sp->codekd = bp->codekd->tree;
			if (sp->funits_upper != 0.0) {
				sp->minAB = sp->index_scale_lower / sp->funits_upper;

				// compute fudge factor for quad scale: what are the extreme
				// ranges of quad scales that should be accepted, given the
				// code tolerance?

				// -what is the maximum number of pixels a C or D star can move
				//  to singlehandedly exceed the code tolerance?
				// -largest quad
				// -smallest arcsec-per-pixel scale

				// -index_scale_upper * 1/sqrt(2) is the side length of
				//  the unit-square of code space, in arcseconds.
				// -that times the code tolerance is how far a C/D star
				//  can move before exceeding the code tolerance, in arcsec.
				// -that divided by the smallest arcsec-per-pixel scale
				//  gives the largest motion in pixels.
				scalefudge = sp->index_scale_upper * M_SQRT1_2 *
					sp->codetol / sp->funits_upper;
				sp->minAB -= scalefudge;
				logmsg(bp, "Scale fudge: %g pixels.\n", scalefudge);
				logmsg(bp, "Set minAB to %g\n", sp->minAB);
			}
			if (sp->funits_lower != 0.0) {
				sp->maxAB = sp->index_scale_upper / sp->funits_lower;
				sp->maxAB += scalefudge;
				logmsg(bp, "Set maxAB to %g\n", sp->maxAB);
			}

			// Set CPU time limit.
			if (bp->cpulimit) {
				struct rusage r;
				struct rlimit rlim;
				int sofar;

				if (getrusage(RUSAGE_SELF, &r)) {
					logerr(bp, "Failed to get resource usage: %s\n", strerror(errno));
					exit(-1);
				}
				sofar = ceil((float)(r.ru_utime.tv_sec + r.ru_stime.tv_sec) +
							 (float)(1e-6 * r.ru_utime.tv_usec + r.ru_stime.tv_usec));

				if (getrlimit(RLIMIT_CPU, &rlim)) {
					logerr(bp, "Failed to get CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}
				rlim.rlim_cur = bp->cpulimit + sofar;

				if (setrlimit(RLIMIT_CPU, &rlim)) {
					logerr(bp, "Failed to set CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}

				oldsigcpu = signal(SIGXCPU, cpu_time_limit);
				if (oldsigcpu == SIG_ERR) {
					logerr(bp, "Failed to set CPU time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Set wall-clock time limit.
			if (bp->timelimit) {
				alarm(bp->timelimit);
				oldsigalarm = signal(SIGALRM, wall_time_limit);
				if (oldsigalarm == SIG_ERR) {
					logerr(bp, "Failed to set wall time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Do it!
			solve_fields(bp);

			// Cancel wall-clock time limit.
			if (bp->timelimit) {
				alarm(0);
				if (signal(SIGALRM, oldsigalarm) == SIG_ERR) {
					logerr(bp, "Failed to restore wall time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Restore CPU time limit.
			if (bp->cpulimit) {
				struct rlimit rlim;
				// Restore old CPU limit signal handler.
				if (signal(SIGXCPU, oldsigcpu) == SIG_ERR) {
					logerr(bp, "Failed to restore CPU time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
				// Remove CPU limit.
				if (getrlimit(RLIMIT_CPU, &rlim)) {
					logerr(bp, "Failed to get CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}
				rlim.rlim_cur = rlim.rlim_max;
				if (setrlimit(RLIMIT_CPU, &rlim)) {
					logerr(bp, "Failed to remove CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Clean up this index...
			codetree_close(bp->codekd);
			startree_close(bp->starkd);
			if (bp->id)
				idfile_close(bp->id);
			quadfile_close(bp->quads);

			bp->codekd = NULL;
			bp->starkd = NULL;
			bp->id = NULL;
			bp->quads = NULL;

			free_fn(treefname);
			free_fn(quadfname);
			free_fn(idfname);
			free_fn(startreefname);
		}

		if (bp->solvedserver) {
			solvedclient_set_server(NULL);
		}

		xylist_close(bp->xyls);

		if (bp->mf) {
			if (matchfile_fix_header(bp->mf) ||
				matchfile_close(bp->mf)) {
				logerr(bp, "Error closing matchfile.\n");
			}
		}

		if (bp->rdls) {
			if (rdlist_fix_header(bp->rdls) ||
				rdlist_close(bp->rdls)) {
				logerr(bp, "Failed to close RDLS file.\n");
			}
			bp->rdls = NULL;
		}
		if (bp->indexrdls) {
			if (rdlist_fix_header(bp->indexrdls) ||
				rdlist_close(bp->indexrdls)) {
				logerr(bp, "Failed to close index RDLS file.\n");
			}
			bp->indexrdls = NULL;
		}

		if (bp->donefname) {
			FILE* batchfid = NULL;
			logmsg(bp, "Writing marker file %s...\n", bp->donefname);
			batchfid = fopen(bp->donefname, "wb");
			if (batchfid)
				fclose(batchfid);
			else
				logerr(bp, "Failed to write marker file %s: %s\n", bp->donefname, strerror(errno));
		}

		if (bp->donescript) {
			int rtn = system(bp->donescript);
			if (rtn == -1) {
				logerr(bp, "Donescript failed.\n");
			} else {
				logmsg(bp, "Donescript returned %i.\n", rtn);
			}
		}

		if (!bp->silent)
			toc();

		// Put stdout and stderr back to the way they were!
		if (bp->logfname) {
			if (dup2(bp->dup_stdout, fileno(stdout)) == -1) {
				logerr(bp, "Failed to dup2() back to stdout.\n");
			}
			if (dup2(bp->dup_stderr, fileno(stderr)) == -1) {
				logerr(bp, "Failed to dup2() back to stderr.\n");
			}
			if (close(bp->dup_stdout) || close(bp->dup_stderr)) {
				logerr(bp, "Failed to close duplicate stdout/stderr: %s\n", strerror(errno));
			}
			if (fclose(bp->logfd)) {
				logerr(bp, "Failed to close log file: %s\n",
						strerror(errno));
			}
		}

		for (i=0; i<pl_size(bp->indexes); i++)
			free(pl_get(bp->indexes, i));

		pl_free(bp->indexes);
		il_free(bp->fieldlist);

		free(bp->logfname);
		free(bp->donefname);
		free(bp->donescript);
		free(bp->startfname);
		free(sp->solved_in);
		free(bp->solved_out);
		free(bp->solvedserver);
		free(sp->cancelfname);
		free(bp->matchfname);
		free(bp->rdlsfname);
		free(bp->indexrdlsfname);
		free(bp->xcolname);
		free(bp->ycolname);
		free(bp->wcs_template);
		free(bp->fieldid_key);
		free(bp->fieldfname);
	}

	qfits_cache_purge(); // for valgrind
	return 0;
}

static int read_parameters(blind_params* bp) {
	solver_params* sp = &(bp->solver);
	for (;;) {
		char buffer[10240];
		char* nextword;
		char* line;
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			return -1;
		}
		line = buffer;
		// strip off newline
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = '\0';

		// skip leading whitespace:
		while (*line && isspace(*line))
			line++;

		logverb(bp, "Command: %s\n", line);

		if (line[0] == '#') {
			//logmsg(bp, "Skipping comment.\n");
			continue;
		}
		// skip blank lines.
		if (line[0] == '\0') {
			continue;
		}
		if (is_word(line, "help", &nextword)) {
			logmsg(bp, "No help soup for you!\n  (use the source, Luke)\n");
		} else if (is_word(line, "cpulimit ", &nextword)) {
			bp->cpulimit = atoi(nextword);
		} else if (is_word(line, "timelimit ", &nextword)) {
			bp->timelimit = atoi(nextword);
		} else if (is_word(line, "total_timelimit ", &nextword)) {
			bp->total_timelimit = atoi(nextword);
		} else if (is_word(line, "verify_dist ", &nextword)) {
			bp->verify_dist2 = arcsec2distsq(atof(nextword));
		} else if (is_word(line, "verify_pix ", &nextword)) {
			bp->verify_pix = atof(nextword);
		} else if (is_word(line, "ratio_tosolve ", &nextword)) {
			bp->logratio_tosolve = log(atof(nextword));
		} else if (is_word(line, "ratio_tokeep ", &nextword)) {
			bp->logratio_tokeep = log(atof(nextword));
		} else if (is_word(line, "ratio_toprint ", &nextword)) {
			bp->logratio_toprint = log(atof(nextword));
		} else if (is_word(line, "ratio_tobail ", &nextword)) {
			bp->logratio_tobail = log(atof(nextword));
		} else if (is_word(line, "nindex_tokeep ", &nextword)) {
			bp->nindex_tokeep = atoi(nextword);
		} else if (is_word(line, "nverify ", &nextword)) {
			bp->nverify = atoi(nextword);
		} else if (is_word(line, "nindex_tosolve ", &nextword)) {
			bp->nindex_tosolve = atoi(nextword);
		} else if (is_word(line, "match ", &nextword)) {
			bp->matchfname = strdup(nextword);
		} else if (is_word(line, "rdls ", &nextword)) {
			bp->rdlsfname = strdup(nextword);
		} else if (is_word(line, "indexrdls ", &nextword)) {
			bp->indexrdlsfname = strdup(nextword);
		} else if (is_word(line, "solved ", &nextword)) {
			free(bp->solver.solved_in);
			free(bp->solved_out);
			bp->solver.solved_in = strdup(nextword);
			bp->solved_out = strdup(nextword);
		} else if (is_word(line, "solved_in ", &nextword)) {
			free(bp->solver.solved_in);
			bp->solver.solved_in = strdup(nextword);
		} else if (is_word(line, "cancel ", &nextword)) {
			free(sp->cancelfname);
			sp->cancelfname = strdup(nextword);
		} else if (is_word(line, "solved_out ", &nextword)) {
			free(bp->solved_out);
			bp->solved_out = strdup(nextword);
		} else if (is_word(line, "log ", &nextword)) {
			// Open the log file...
			bp->logfname = strdup(nextword);
			bp->logfd = fopen(bp->logfname, "a");
			if (!bp->logfd) {
				logerr(bp, "Failed to open log file %s: %s\n", bp->logfname, strerror(errno));
				goto bailout;
			}
			// Save old stdout/stderr...
			bp->dup_stdout = dup(fileno(stdout));
			if (bp->dup_stdout == -1) {
				logerr(bp, "Failed to dup stdout: %s\n", strerror(errno));
				goto bailout;
			}
			bp->dup_stderr = dup(fileno(stderr));
			if (bp->dup_stderr == -1) {
				logerr(bp, "Failed to dup stderr: %s\n", strerror(errno));
				goto bailout;
			}
			// Replace stdout/stderr with logfile...
			if (dup2(fileno(bp->logfd), fileno(stderr)) == -1) {
				logerr(bp, "Failed to dup2 log file: %s\n", strerror(errno));
				goto bailout;
			}
			if (dup2(fileno(bp->logfd), fileno(stdout)) == -1) {
				logerr(bp, "Failed to dup2 log file: %s\n", strerror(errno));
				goto bailout;
			}
			continue;
			bailout:
			if (bp->logfd) fclose(bp->logfd);
			free(bp->logfname);
			bp->logfname = NULL;
		} else if (is_word(line, "solvedserver ", &nextword)) {
			bp->solvedserver = strdup(nextword);
		} else if (is_word(line, "silent", &nextword)) {
			bp->silent = TRUE;
		} else if (is_word(line, "quiet", &nextword)) {
			sp->quiet = TRUE;
		} else if (is_word(line, "verbose", &nextword)) {
			bp->verbose = TRUE;
		} else if (is_word(line, "tweak_noshift", &nextword)) {
			bp->tweak_skipshift = TRUE;
		} else if (is_word(line, "tweak_aborder ", &nextword)) {
			bp->tweak_aborder = atoi(nextword);
		} else if (is_word(line, "tweak_abporder ", &nextword)) {
			bp->tweak_abporder = atoi(nextword);
		} else if (is_word(line, "tweak", &nextword)) {
			bp->do_tweak = TRUE;
		} else if (is_word(line, "wcs ", &nextword)) {
			bp->wcs_template = strdup(nextword);
		} else if (is_word(line, "fieldid_key ", &nextword)) {
			free(bp->fieldid_key);
			bp->fieldid_key = strdup(nextword);
		} else if (is_word(line, "maxquads ", &nextword)) {
			sp->maxquads = atoi(nextword);
		} else if (is_word(line, "maxmatches ", &nextword)) {
			sp->maxmatches = atoi(nextword);
		} else if (is_word(line, "xcol ", &nextword)) {
			free(bp->xcolname);
			bp->xcolname = strdup(nextword);
		} else if (is_word(line, "ycol ", &nextword)) {
			free(bp->ycolname);
			bp->ycolname = strdup(nextword);
		} else if (is_word(line, "index ", &nextword)) {
			pl_append(bp->indexes, strdup(nextword));
		} else if (is_word(line, "field ", &nextword)) {
			free(bp->fieldfname);
			bp->fieldfname = strdup(nextword);
		} else if (is_word(line, "fieldw ", &nextword)) {
			sp->field_maxx = atof(nextword);
		} else if (is_word(line, "fieldh ", &nextword)) {
			sp->field_maxy = atof(nextword);
		} else if (is_word(line, "distractors ", &nextword)) {
			bp->distractors = atof(nextword);
		} else if (is_word(line, "fieldid ", &nextword)) {
			sp->fieldid = atoi(nextword);
		} else if (is_word(line, "done ", &nextword)) {
			bp->donefname = strdup(nextword);
		} else if (is_word(line, "donescript ", &nextword)) {
			bp->donescript = strdup(nextword);
		} else if (is_word(line, "start ", &nextword)) {
			bp->startfname = strdup(nextword);
		} else if (is_word(line, "sdepth ", &nextword)) {
			sp->startobj = atoi(nextword);
		} else if (is_word(line, "depth ", &nextword)) {
			sp->endobj = atoi(nextword);
		} else if (is_word(line, "tol ", &nextword)) {
			sp->codetol = atof(nextword);
		} else if (is_word(line, "parity ", &nextword)) {
			sp->parity = atoi(nextword);
		} else if (is_word(line, "fieldunits_lower ", &nextword)) {
			sp->funits_lower = atof(nextword);
		} else if (is_word(line, "fieldunits_upper ", &nextword)) {
			sp->funits_upper = atof(nextword);
		} else if (is_word(line, "firstfield ", &nextword)) {
			bp->firstfield = atoi(nextword);
		} else if (is_word(line, "lastfield ", &nextword)) {
			bp->lastfield = atoi(nextword);
		} else if (is_word(line, "fields ", &nextword)) {
			char* str = nextword;
			char* endp;
			int i, firstfld = -1;
			for (;;) {
				int fld = strtol(str, &endp, 10);
				if (str == endp) {
					// non-numeric value
					logerr(bp, "Couldn't parse: %.20s [etc]\n", str);
					break;
				}
				if (firstfld == -1) {
					il_insert_unique_ascending(bp->fieldlist, fld);
				} else {
					if (firstfld > fld) {
						logerr(bp, "Ranges must be specified as <start>/<end>: %i/%i\n", firstfld, fld);
					} else {
						for (i=firstfld+1; i<=fld; i++) {
							il_insert_unique_ascending(bp->fieldlist, i);
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
		} else if (is_word(line, "run", &nextword)) {
			return 0;
		} else if (is_word(line, "quit", &nextword)) {
			return 1;
		} else {
			logmsg(bp, "I didn't understand this command:\n  \"%s\"", line);
		}
	}
}

static sip_t* tweak(blind_params* bp, MatchObj* mo, startree* starkd) {
	solver_params* sp = &(bp->solver);
	tweak_t* twee = NULL;
	double *imgx = NULL, *imgy = NULL;
	int i;
	double* starxyz;
	int nstars;
	kdtree_qres_t* res = NULL;
	double fieldcenter[3];
	double fieldr2;
	sip_t* sip = NULL;

	logmsg(bp, "Tweaking!\n");

	twee = tweak_new();
	if (bp->verify_dist2 > 0.0)
		twee->jitter = distsq2arcsec(bp->verify_dist2);
	else {
		twee->jitter = hypot(mo->scale * bp->verify_pix, sp->index_jitter);
		logmsg(bp, "Pixel scale implied by this quad: %g arcsec/pix.\n", mo->scale);
		logmsg(bp, "Star jitter: %g arcsec.\n", twee->jitter);
	}
	// Set tweak's jitter to 6 sigmas.
	twee->jitter *= 6.0;
	logmsg(bp, "Setting tweak jitter: %g arcsec.\n", twee->jitter);

	// pull out the field coordinates into separate X and Y arrays.
	imgx = malloc(sp->nfield * sizeof(double));
	imgy = malloc(sp->nfield * sizeof(double));
	for (i=0; i<sp->nfield; i++) {
		imgx[i] = sp->field[i*2 + 0];
		imgy[i] = sp->field[i*2 + 1];
	}
	logmsg(bp, "Pushing %i image coordinates.\n", sp->nfield);
	tweak_push_image_xy(twee, imgx, imgy, sp->nfield);

	// find all the index stars that are inside the circle that bounds
	// the field.
	star_midpoint(fieldcenter, mo->sMin, mo->sMax);
	fieldr2 = distsq(fieldcenter, mo->sMin, 3);
	// 1.05 is a little safety factor.
	res = kdtree_rangesearch_options(starkd->tree, fieldcenter,
									 fieldr2 * 1.05,
									 KD_OPTIONS_SMALL_RADIUS |
									 KD_OPTIONS_RETURN_POINTS);
	if (!res || !res->nres)
		goto bailout;
	starxyz = res->results.d;
	nstars = res->nres;
	logmsg(bp, "Pushing %i star coordinates.\n", nstars);
	tweak_push_ref_xyz(twee, starxyz, nstars);

	tweak_push_wcs_tan(twee, &(mo->wcstan));
	twee->sip->a_order  = twee->sip->b_order  = bp->tweak_aborder;
	twee->sip->ap_order = twee->sip->bp_order = bp->tweak_abporder;

	if (bp->tweak_skipshift) {
		logmsg(bp, "Skipping shift operation.\n");
		tweak_skip_shift(twee);
	}

	logmsg(bp, "Begin tweaking...\n");
	while (!(twee->state & TWEAK_HAS_LINEAR_CD)) {
		unsigned int r = tweak_advance_to(twee, TWEAK_HAS_LINEAR_CD);
		if (r == -1) {
			logerr(bp, "Tweak error!\n");
			goto bailout;
		}
	}

	logmsg(bp, "Done tweaking!\n");

	// Steal the resulting SIP structure
	sip = twee->sip;
	// Set it NULL so tweak_free() doesn't delete it.
	twee->sip = NULL;

 bailout:
	kdtree_free_query(res);
	free(imgx);
	free(imgy);
	tweak_free(twee);
	return sip;
}

static void print_match(blind_params* bp, MatchObj* mo) {
	int Nmin = min(mo->nindex, mo->nfield);
	int ndropout = Nmin - mo->noverlap - mo->nconflict;
	logmsg(bp, "logodds ratio %g (%g), %i match, %i conflict, %i dropout, %i index.\n",
		   mo->logodds, exp(mo->logodds), mo->noverlap, mo->nconflict, ndropout, mo->nindex);
}

static int blind_handle_hit(solver_params* sp, MatchObj* mo) {
	blind_params* bp = sp->userdata;
	double pixd2;

	// if verification was specified in pixel units, compute the verification
	// distance on the unit sphere...
	if (bp->verify_pix > 0.0) {
		pixd2 = square(bp->verify_pix) + square(sp->index_jitter / mo->scale);
		//d2 = arcsec2distsq(hypot(mo->scale * bp->verify_pix, sp->index_jitter));
	} else {
		pixd2 = (bp->verify_dist2 + square(sp->index_jitter)) / square(mo->scale);
		//d2 = bp->verify_dist2 + square(sp->index_jitter);
	}

	verify_hit(bp->starkd->tree, mo, sp->field, sp->nfield, pixd2,
			   bp->distractors, sp->field_maxx, sp->field_maxy,
			   bp->logratio_tobail, bp->nverify);
	// FIXME - this is the same as nmatches.
	mo->nverified = bp->nverified++;

	if (mo->logodds >= bp->logratio_toprint) {
		print_match(bp, mo);
	}

	if (mo->logodds >= bp->bestlogodds) {
		bp->bestlogodds = mo->logodds;
	}

	if ((mo->logodds < bp->logratio_tokeep) ||
		(mo->nindex < bp->nindex_tokeep)) {
		return FALSE;
	}

	if (bp->mf) {
		if (matchfile_write_match(bp->mf, mo)) {
			logmsg(bp, "Field %i: error writing a match.\n", mo->fieldnum);
		}
	}

	if (!bp->have_bestmo || (mo->logodds > bp->bestmo.logodds)) {
		logmsg(bp, "Got a new best match: logodds %g.\n", mo->logodds);
		//print_match(bp, mo);
		memcpy(&(bp->bestmo), mo, sizeof(MatchObj));
		bp->have_bestmo = TRUE;
	}

	if ((mo->logodds < bp->logratio_tosolve) ||
		(mo->nindex < bp->nindex_tosolve)) {
		return FALSE;
	}

	bp->bestmo_solves = TRUE;
	return TRUE;
}

static void add_blind_params(blind_params* bp, qfits_header* hdr) {
	solver_params* sp = &(bp->solver);
	int i;
	fits_add_long_comment(hdr, "-- blind solver parameters: --");
	fits_add_long_comment(hdr, "Index name: %s", bp->indexname);
	for (i=0; i<pl_size(bp->indexes); i++)
		fits_add_long_comment(hdr, "Index(%i): %s", i, (char*)pl_get(bp->indexes, i));
	fits_add_long_comment(hdr, "Index scale lower: %g arcsec", sp->index_scale_lower);
	fits_add_long_comment(hdr, "Index scale upper: %g arcsec", sp->index_scale_upper);
	fits_add_long_comment(hdr, "Index jitter: %g", sp->index_jitter);
	fits_add_long_comment(hdr, "Index id: %i", bp->indexid);
	fits_add_long_comment(hdr, "Index healpix: %i", bp->healpix);
	fits_add_long_comment(hdr, "Circle: %s", sp->circle ? "yes":"no");

	fits_add_long_comment(hdr, "Field name: %s", bp->fieldfname);
	fits_add_long_comment(hdr, "Field scale lower: %g arcsec/pixel", sp->funits_lower);
	fits_add_long_comment(hdr, "Field scale upper: %g arcsec/pixel", sp->funits_upper);
	fits_add_long_comment(hdr, "X col name: %s", bp->xcolname);
	fits_add_long_comment(hdr, "Y col name: %s", bp->ycolname);
	fits_add_long_comment(hdr, "Start obj: %i", sp->startobj);
	fits_add_long_comment(hdr, "End obj: %i", sp->endobj);

	fits_add_long_comment(hdr, "Solved_in: %s", bp->solver.solved_in);
	fits_add_long_comment(hdr, "Solved_out: %s", bp->solved_out);
	fits_add_long_comment(hdr, "Solvedserver: %s", bp->solvedserver);

	fits_add_long_comment(hdr, "Parity: %i", sp->parity);
	fits_add_long_comment(hdr, "Codetol: %g", sp->codetol);
	fits_add_long_comment(hdr, "Cxdx margin: %g", sp->cxdx_margin);
	fits_add_long_comment(hdr, "Verify distance: %g arcsec", distsq2arcsec(bp->verify_dist2));
	fits_add_long_comment(hdr, "Verify pixels: %g pix", bp->verify_pix);
	fits_add_long_comment(hdr, "N index in field to solve: %i", bp->nindex_tosolve);

	fits_add_long_comment(hdr, "Maxquads: %i", sp->maxquads);
	fits_add_long_comment(hdr, "Maxmatches: %i", sp->maxmatches);
	fits_add_long_comment(hdr, "Cpu limit: %i s", bp->cpulimit);
	fits_add_long_comment(hdr, "Time limit: %i s", bp->timelimit);
	fits_add_long_comment(hdr, "Total time limit: %i s", bp->total_timelimit);

	fits_add_long_comment(hdr, "Tweak: %s", (bp->do_tweak ? "yes" : "no"));
	if (bp->do_tweak) {
		fits_add_long_comment(hdr, "Tweak AB order: %i", bp->tweak_aborder);
		fits_add_long_comment(hdr, "Tweak ABP order: %i", bp->tweak_abporder);
	}
	fits_add_long_comment(hdr, "--");
}

static void solve_fields(blind_params* bp) {
	solver_params* sp = &(bp->solver);
	double last_utime, last_stime;
	double utime, stime;
	struct timeval wtime, last_wtime;
	int nfields;
	int fi;

	get_resource_stats(&last_utime, &last_stime, NULL);
	gettimeofday(&last_wtime, NULL);

	nfields = bp->xyls->nfields;
	sp->field = NULL;

	for (fi=0; fi<il_size(bp->fieldlist); fi++) {
		int fieldnum;
		MatchObj template;
		qfits_header* fieldhdr = NULL;

		if (bp->rdls) {
			if (rdlist_write_new_field(bp->rdls)) {
				logerr(bp, "Failed to write RDLS field header.\n");
			}
		}
		if (bp->indexrdls) {
			if (rdlist_write_new_field(bp->indexrdls)) {
				logerr(bp, "Failed to write index RDLS field header.\n");
			}
		}

		fieldnum = il_get(bp->fieldlist, fi);
		if (fieldnum >= nfields) {
			logerr(bp, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			goto cleanup;
		}

		// Has the field already been solved?
		if (sp->solved_in) {
			if (solvedfile_get(sp->solved_in, fieldnum) == 1) {
				// file exists; field has already been solved.
				logmsg(bp, "Field %i: solvedfile %s: field has been solved.\n", fieldnum, sp->solved_in);
				goto cleanup;
			}
		}

		if (bp->solvedserver) {
			if (solvedclient_get(sp->fieldid, fieldnum) == 1) {
				// field has already been solved.
				logmsg(bp, "Field %i: field has already been solved.\n", fieldnum);
				goto cleanup;
			}
		}
		sp->do_solvedserver = (bp->solvedserver ? TRUE : FALSE);

		// Get the field.
		sp->nfield = xylist_n_entries(bp->xyls, fieldnum);
		if (sp->nfield == -1) {
			logerr(bp, "Couldn't determine how many objects are in field %i.\n", fieldnum);
			goto cleanup;
		}
		sp->field = realloc(sp->field, 2 * sp->nfield * sizeof(double));
		if (xylist_read_entries(bp->xyls, fieldnum, 0, sp->nfield, sp->field)) {
			logerr(bp, "Failed to read field.\n");
			goto cleanup;
		}

		// FIXME - reset these to their original values when done!!
		if (sp->field_minx == sp->field_maxx) {
			int i;
			for (i=0; i<sp->nfield; i++) {
				sp->field_minx = min(sp->field_minx, sp->field[2*i+0]);
				sp->field_maxx = max(sp->field_maxx, sp->field[2*i+0]);
			}
		}
		if (sp->field_miny == sp->field_maxy) {
			int i;
			for (i=0; i<sp->nfield; i++) {
				sp->field_miny = min(sp->field_miny, sp->field[2*i+1]);
				sp->field_maxy = max(sp->field_maxy, sp->field[2*i+1]);
			}
		}
		sp->field_diag = hypot(sp->field_maxy - sp->field_miny,
							   sp->field_maxx - sp->field_minx);

		memset(&template, 0, sizeof(MatchObj));
		template.fieldnum = fieldnum;
		template.fieldfile = sp->fieldid;
		template.indexid = bp->indexid;
		template.healpix = bp->healpix;

		fieldhdr = xylist_get_field_header(bp->xyls, fieldnum);
		if (fieldhdr) {
			char* idstr = qfits_pretty_string(qfits_header_getstr(fieldhdr, bp->fieldid_key));
			if (idstr)
				strncpy(template.fieldname, idstr, sizeof(template.fieldname)-1);
			qfits_header_destroy(fieldhdr);
		}

		sp->numtries = 0;
		sp->nummatches = 0;
		sp->numscaleok = 0;
		sp->numcxdxskipped = 0;
		sp->quitNow = FALSE;
		sp->mo_template = &template;
		sp->fieldnum = fieldnum;

		bp->nverified = 0;
		bp->have_bestmo = FALSE;
		memset(&(bp->bestmo), 0, sizeof(MatchObj));
		bp->bestmo_solves = FALSE;
		bp->bestlogodds = -HUGE_VAL;

		logmsg(bp, "\nSolving field %i.\n", fieldnum);

		// The real thing
		solve_field(sp);

		logmsg(bp, "field %i: tried %i quads, matched %i codes.\n",
			   fieldnum, sp->numtries, sp->nummatches);

		if (sp->maxquads && sp->numtries >= sp->maxquads) {
			logmsg(bp, "  exceeded the number of quads to try: %i >= %i.\n",
					sp->numtries, sp->maxquads);
		}
		if (sp->maxmatches && sp->nummatches >= sp->maxmatches) {
			logmsg(bp, "  exceeded the number of quads to match: %i >= %i.\n",
					sp->nummatches, sp->maxmatches);
		}
		if (sp->cancelled) {
			logmsg(bp, "  cancelled at user request.\n");
		}

		// Fix the matchfile.
		if (matchfile_fix_header(bp->mf)) {
			logerr(bp, "Failed to fix the matchfile header for field %i.\n", fieldnum);
		}

		if (bp->have_bestmo && !bp->bestmo_solves) {
			MatchObj* bestmo = &(bp->bestmo);
			int Nmin = min(bestmo->nindex, bestmo->nfield);
			int ndropout = Nmin - bestmo->noverlap - bestmo->nconflict;
			logmsg(bp, "Field %i did not solved (best odds ratio %g (%i match, %i conflict, %i dropout, %i index)).\n",
				   fieldnum, exp(bestmo->logodds), bestmo->noverlap, bestmo->nconflict, ndropout, bestmo->nindex);
		}

		if (bp->have_bestmo && bp->bestmo_solves) {
			MatchObj* bestmo = &(bp->bestmo);
			sip_t* sip = NULL;
			// Field solved!
			logmsg(bp, "Field %i solved: ", fieldnum);
			print_match(bp, bestmo);

			// Tweak, if requested.
			if (bp->do_tweak) {
				sip = tweak(bp, bestmo, bp->starkd);
			}

			// Write WCS, if requested.
			if (bp->wcs_template) {
				char wcs_fn[1024];
				FILE* fout;
				qfits_header* hdr;
				char* tm;
				char val[32];

				snprintf(wcs_fn, sizeof(wcs_fn), bp->wcs_template, fieldnum);
				fout = fopen(wcs_fn, "ab");
				if (!fout) {
					logerr(bp, "Failed to open WCS output file %s: %s\n", wcs_fn, strerror(errno));
					exit(-1);
				}

				assert(bestmo->wcs_valid);

				if (sip)
					hdr = blind_wcs_get_sip_header(sip);
				else
					hdr = blind_wcs_get_header(&(bestmo->wcstan));

				sprintf(val, "%g", sp->field_maxx);
				qfits_header_add(hdr, "IMAGEW", val, "Width of the image used to solve this WCS.", NULL);
				sprintf(val, "%g", sp->field_maxy);
				qfits_header_add(hdr, "IMAGEH", val, "Height of the image used to solve this WCS.", NULL);

				boilerplate_add_fits_headers(hdr);
				qfits_header_add(hdr, "HISTORY", "This WCS header was created by the program \"blind\".", NULL, NULL);
				tm = qfits_get_datetime_iso8601();
				qfits_header_add(hdr, "DATE", tm, "Date this file was created.", NULL);

				add_blind_params(bp, hdr);

				fits_add_long_comment(hdr, "-- properties of the matching quad: --");
				fits_add_long_comment(hdr, "quadno: %i", bestmo->quadno);
				fits_add_long_comment(hdr, "stars: %i,%i,%i,%i", bestmo->star[0], bestmo->star[1], bestmo->star[2], bestmo->star[3]);
				fits_add_long_comment(hdr, "field: %i,%i,%i,%i", bestmo->field[0], bestmo->field[1], bestmo->field[2], bestmo->field[3]);
				fits_add_long_comment(hdr, "code error: %g", sqrt(bestmo->code_err));
				fits_add_long_comment(hdr, "noverlap: %i", bestmo->noverlap);
				fits_add_long_comment(hdr, "nconflict: %i", bestmo->nconflict);
				fits_add_long_comment(hdr, "nfield: %i", bestmo->nfield);
				fits_add_long_comment(hdr, "nindex: %i", bestmo->nindex);
				fits_add_long_comment(hdr, "scale: %g arcsec/pix", bestmo->scale);
				fits_add_long_comment(hdr, "parity: %i", (int)bestmo->parity);
				fits_add_long_comment(hdr, "quads tried: %i", bestmo->quads_tried);
				fits_add_long_comment(hdr, "quads matched: %i", bestmo->quads_matched);
				fits_add_long_comment(hdr, "quads verified: %i", bestmo->nverified);
				fits_add_long_comment(hdr, "objs tried: %i", bestmo->objs_tried);
				fits_add_long_comment(hdr, "cpu time: %g", bestmo->timeused);
				fits_add_long_comment(hdr, "--");
				
				if (sp->mo_template && sp->mo_template->fieldname[0])
					qfits_header_add(hdr, bp->fieldid_key, sp->mo_template->fieldname, "Field name (copied from input field)", NULL);
				if (qfits_header_dump(hdr, fout)) {
					logerr(bp, "Failed to write FITS WCS header.\n");
					exit(-1);
				}
				fits_pad_file(fout);
				qfits_header_destroy(hdr);
				fclose(fout);
			}

			if (bp->rdls) {
				double* radec = malloc(sp->nfield * 2 * sizeof(double));
				int i;
				if (sip) {
					for (i=0; i<sp->nfield; i++)
						sip_pixelxy2radec(sip,
										  sp->field[i*2], sp->field[i*2+1],
										  radec+i*2, radec+i*2+1);
				} else {
					for (i=0; i<sp->nfield; i++)
						tan_pixelxy2radec(&(bestmo->wcstan),
										  sp->field[i*2], sp->field[i*2+1],
										  radec+i*2, radec+i*2+1);
				}
				if (rdlist_write_entries(bp->rdls, radec, sp->nfield)) {
					logerr(bp, "Failed to write RDLS entry.\n");
				}
				free(radec);
			}

			if (bp->indexrdls) {
				kdtree_qres_t* res = NULL;
				double* starxyz;
				int nstars;
				double* radec;
				int i;
				double fieldcenter[3];
				double fieldr2;
				// find all the index stars that are inside the circle that bounds
				// the field.
				star_midpoint(fieldcenter, bestmo->sMin, bestmo->sMax);
				fieldr2 = distsq(fieldcenter, bestmo->sMin, 3);
				// 1.05 is a little safety factor.
				res = kdtree_rangesearch_options(bp->starkd->tree, fieldcenter,
												 fieldr2 * 1.05,
												 KD_OPTIONS_SMALL_RADIUS |
												 KD_OPTIONS_RETURN_POINTS);
				if (!res || !res->nres) {
					logmsg(bp, "No index stars found!\n");
				}
				starxyz = res->results.d;
				nstars = res->nres;

				radec = malloc(nstars * 2 * sizeof(double));
				for (i=0; i<nstars; i++)
					xyzarr2radec(starxyz + i*3, radec+i*2, radec+i*2+1);
				for (i=0; i<2*nstars; i++)
					radec[i] = rad2deg(radec[i]);

				if (rdlist_write_entries(bp->indexrdls, radec, nstars)) {
					logerr(bp, "Failed to write index RDLS entry.\n");
				}



				// DEBUG - write out some extra stuff for Sam.
				fprintf(stderr, "fieldquad = [ %g,%g; %g,%g; %g,%g; %g,%g ];\n",
						sp->field[bestmo->field[0] * 2], sp->field[bestmo->field[0] * 2 + 1],
						sp->field[bestmo->field[1] * 2], sp->field[bestmo->field[1] * 2 + 1],
						sp->field[bestmo->field[2] * 2], sp->field[bestmo->field[2] * 2 + 1],
						sp->field[bestmo->field[3] * 2], sp->field[bestmo->field[3] * 2 + 1]);
				{
					double xyz[3];
					double xyzcm[3];
					int k;
					double x,y;
					fprintf(stderr, "indexquad_xyz = [ ");
					xyzcm[0] = xyzcm[1] = xyzcm[2] = 0.0;
					for (k=0; k<4; k++) {
						getstarcoord(bestmo->star[k], xyz);
						fprintf(stderr, "%g,%g,%g; ", xyz[0], xyz[1], xyz[2]);
						xyzcm[0] += 0.25 * xyz[0];
						xyzcm[1] += 0.25 * xyz[1];
						xyzcm[2] += 0.25 * xyz[2];
					}
					fprintf(stderr, "];\n");
					// project around center of mass.
					fprintf(stderr, "indexquad_xy = [");
					for (k=0; k<4; k++) {
						getstarcoord(bestmo->star[k], xyz);
						star_coords(xyz, xyzcm, &x, &y);
						fprintf(stderr, "%g,%g; ", x, y);
					}
					fprintf(stderr, "];\n");

					fprintf(stderr, "index_all_xy = [");
					for (i=0; i<nstars; i++) {
						star_coords(starxyz + i*3, xyzcm, &x, &y);
						fprintf(stderr, "%g,%g; ", x, y);
					}
					fprintf(stderr, "];\n");

					fprintf(stderr, "field_all = [");
					for (i=0; i<sp->nfield; i++) {
						fprintf(stderr, "%g,%g; ", sp->field[2*i], sp->field[2*i+1]);
					}
					fprintf(stderr, "];\n");
					
				}



				free(radec);
				kdtree_free_query(res);
			}

			if (sip) {
				sip_free(sip);
			}
			// Record in solved file, or send to solved server.
			if (bp->solved_out) {
				logmsg(bp, "Field %i solved: writing to file %s to indicate this.\n", fieldnum, bp->solved_out);
				if (solvedfile_set(bp->solved_out, fieldnum)) {
					logerr(bp, "Failed to write to solvedfile %s.\n", bp->solved_out);
				}
			}
			if (bp->solvedserver) {
				solvedclient_set(sp->fieldid, fieldnum);
			}

		} else {
			// Field unsolved.
			logmsg(bp, "Field %i is unsolved.\n", fieldnum);
			if (bp->have_bestmo) {
				logmsg(bp, "Best match encountered: ");
				print_match(bp, &(bp->bestmo));
			} else {
				logmsg(bp, "Best odds encountered: %g\n", exp(bp->bestlogodds));
			}
		}

		get_resource_stats(&utime, &stime, NULL);
		gettimeofday(&wtime, NULL);
		logmsg(bp, "  Spent %g s user, %g s system, %g s total, %g s wall time.\n",
			   (utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime),
			   millis_between(&last_wtime, &wtime) * 0.001);

		last_utime = utime;
		last_stime = stime;
		last_wtime = wtime;

	cleanup:
		if (bp->rdls) {
			if (rdlist_fix_field(bp->rdls)) {
				logerr(bp, "Failed to fix RDLS field header.\n");
			}
		}
		if (bp->indexrdls) {
			if (rdlist_fix_field(bp->indexrdls)) {
				logerr(bp, "Failed to fix index RDLS field header.\n");
			}
		}
	}

	free(sp->field);
	sp->field = NULL;
}

void getquadids(uint thisquad, uint *iA, uint *iB, uint *iC, uint *iD) {
	quadfile_get_starids(my_bp.quads, thisquad, iA, iB, iC, iD);
}

void getstarcoord(uint iA, double *sA) {
	startree_get(my_bp.starkd, iA, sA);
}

uint64_t getstarid(uint iA) {
	if (my_bp.id)
		return idfile_get_anid(my_bp.id, iA);
	return 0;
}
