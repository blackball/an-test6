/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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
 * Figure out why a field didn't solve.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "qfits_error.h"
#include "qfits_cache.h"

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
#include "starkd.h"
#include "codekd.h"
#include "boilerplate.h"
#include "fitsioutils.h"
#include "blind_wcs.h"
#include "rdlist.h"
#include "verify.h"
#include "qidxfile.h"
#include "intmap.h"
#include "noise.h"

static void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n", progname);
}

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#define DEFAULT_CODE_TOL .01
#define DEFAULT_PARITY PARITY_BOTH
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

    char* truerdlsfname;

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

	idfile* id;
	quadfile* quads;
	xylist* xyls;
	startree* starkd;
	codetree* codekd;

    qidxfile* qidx;
    rdlist* truerdls;

	rdlist* indexrdls;

    double* truerd;

};
typedef struct blind_params blind_params;

static void solve_fields(blind_params* bp);
static int read_parameters(blind_params* bp);

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
		sp->field_minx = sp->field_maxx = 0.0;
		sp->field_miny = sp->field_maxy = 0.0;
		sp->parity = DEFAULT_PARITY;
		sp->codetol = DEFAULT_CODE_TOL;

		sp->handlehit = NULL;

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

		if (bp->truerdlsfname) {
			bp->truerdls = rdlist_open(bp->truerdlsfname);
			if (!bp->truerdls) {
				logerr(bp, "Failed to open ground-truth RDLS file %s.\n",
					   bp->truerdlsfname);
			}
		}

		if (bp->indexrdlsfname) {
			bp->indexrdls = rdlist_open_for_writing(bp->indexrdlsfname);
			if (bp->indexrdls) {
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

		for (I=0; I<pl_size(bp->indexes); I++) {
			char *idfname, *treefname, *quadfname, *startreefname;
			char* fname = pl_get(bp->indexes, I);
			double scalefudge = 0.0; // in pixels

			char* qidxfname;

			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);

			qidxfname = mk_qidxfn(fname);

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

			bp->qidx = qidxfile_open(qidxfname, 0);
			if (!bp->qidx) {
				logerr(bp, "Failed to read qidxfile %s.\n", qidxfname);
				exit(-1);
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

			// Do it!
			solve_fields(bp);

			// Clean up this index...
			qidxfile_close(bp->qidx);
			bp->qidx = NULL;
			free_fn(qidxfname);

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

		xylist_close(bp->xyls);

		if (bp->truerdls) {
			rdlist_close(bp->truerdls);
		}

		if (bp->indexrdls) {
			if (rdlist_fix_header(bp->indexrdls) ||
				rdlist_close(bp->indexrdls)) {
				logerr(bp, "Failed to close index RDLS file.\n");
			}
			bp->indexrdls = NULL;
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
		} else if (is_word(line, "truerdls ", &nextword)) {
			bp->truerdlsfname = strdup(nextword);
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

static void solve_fields(blind_params* bp) {
	solver_params* sp = &(bp->solver);
	int nfields;
	int fi;

	nfields = bp->xyls->nfields;
	sp->field = NULL;

	for (fi=0; fi<il_size(bp->fieldlist); fi++) {
		int fieldnum;

		double truecenter[3];
		double truer2;
		double* truexyz = NULL;
		kdtree_qres_t* res;
		double* indxyz;
		int nind;
		kdtree_t* itree;
		int* fieldtoind = NULL;
		int i;
		int Nleaf = 10;
		il* quadids = NULL;
		int NS;
		il* goodquads = NULL;
		il* goodstars = NULL;
		il* goodfields = NULL;
		il* starids = NULL;
		il* goodmaxfields = NULL;
		intmap* mapif = NULL;
		int uq;
		il* quadorder = NULL;
		tan_t wcs;

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


		// Get the corresponding True RDLS field
		bp->truerd = malloc(2 * sp->nfield * sizeof(double));
		if (rdlist_read_entries(bp->truerdls, fieldnum, 0, sp->nfield, bp->truerd)) {
			logerr(bp, "Failed to read true RDLS field.\n");
			goto cleanup;
		}

		// Find the approximate center and radius of the true field.
		truexyz = malloc(3 * sp->nfield * sizeof(double));
		truecenter[0] = truecenter[1] = truecenter[2] = 0.0;
		for (i=0; i<sp->nfield; i++) {
			radecdeg2xyzarr(bp->truerd[i*2], bp->truerd[i*2+1], truexyz + i*3);
			truecenter[0] += truexyz[i*3+0];
			truecenter[1] += truexyz[i*3+1];
			truecenter[2] += truexyz[i*3+2];
		}
		normalize_3(truecenter);
		truer2 = 0.0;
		for (i=0; i<sp->nfield; i++) {
			double r2;
			r2 = distsq(truecenter, truexyz + i*3, 3);
			if (r2 > truer2)
				truer2 = r2;
		}

		// Find all index stars in range.
		res = kdtree_rangesearch_options(bp->starkd->tree, truecenter,
										 truer2 * 1.05,
										 KD_OPTIONS_SMALL_RADIUS |
										 KD_OPTIONS_RETURN_POINTS);
		if (!res || !res->nres) {
			logerr(bp, "No index stars found.\n");
			goto cleanup;
		}

		indxyz = res->results.d;
		nind = res->nres;

		logmsg(bp, "Found %i index stars in range.\n", nind);

		if (bp->indexrdls) {
			double* indradec = malloc(2 * nind * sizeof(double));
			for (i=0; i<nind; i++) {
				xyzarr2radec(indxyz + i*3, indradec+i*2+0, indradec+i*2+1);
				indradec[2*i+0] = rad2deg(indradec[2*i+0]);
				indradec[2*i+1] = rad2deg(indradec[2*i+1]);
			}
			if (rdlist_write_entries(bp->indexrdls, indradec, nind)) {
				logerr(bp, "Failed to write index RDLS entry.\n");
			} else {
				logmsg(bp, "Wrote %i index RDLS entries.", nind);
			}
			free(indradec);
		}

		itree = kdtree_build(NULL, indxyz, nind, 3, Nleaf, KDTT_DOUBLE, KD_BUILD_BBOX);

		fieldtoind = malloc(sp->nfield * sizeof(int));

		// For each obj in the RDLS, see if there is a corresponding
		// index star.
		for (i=0; i<sp->nfield; i++) {
			kdtree_qres_t* q;
			int ind;
			fieldtoind[i] = -1;
			q = kdtree_rangesearch_options(itree, truexyz + i*3,
										   arcsec2distsq(sp->index_jitter),
										   KD_OPTIONS_SMALL_RADIUS |
										   KD_OPTIONS_RETURN_POINTS |
										   KD_OPTIONS_SORT_DISTS);
			if (!q)
				continue;

			if (q->nres > 1) {
				logmsg(bp, "%i search results.\n", q->nres);
			}

			if (q->nres >= 1) {
				ind = res->inds[q->inds[0]];
				fieldtoind[i] = ind;
			}
			kdtree_free_query(q);

			if (fieldtoind[i] != -1) {
				double indxyz[3];
				getstarcoord(fieldtoind[i], indxyz);
				logmsg(bp, "Field obj %i is %g arcsec from index star %i\n",
					   i, distsq2arcsec(distsq(indxyz, truexyz+i*3, 3)), fieldtoind[i]);
			}
		}

		kdtree_free(itree);
		kdtree_free_query(res);

		mapif = intmap_new(INTMAP_ONE_TO_ONE);

		NS = 0;
		for (i=0; i<sp->nfield; i++) {
			if (fieldtoind[i] != -1) {
				NS++;
				intmap_add(mapif, fieldtoind[i], i);
			}
		}
		logmsg(bp, "%i index stars are within index jitter of a field obj.\n", NS);


		// Compute WCS.
		{
			double* starxyz;
			double* fieldxy;
			int k;

			starxyz = malloc(NS * 3 * sizeof(double));
			fieldxy = malloc(NS * 2 * sizeof(double));
			k=0;
			for (i=0; i<sp->nfield; i++) {
				if (fieldtoind[i] == -1)
					continue;
				getstarcoord(fieldtoind[i], starxyz + k*3);
				fieldxy[k*2] = sp->field[i*2];
				fieldxy[k*2+1] = sp->field[i*2+1];
				k++;
			}

			blind_wcs_compute_2(starxyz, fieldxy, k, &wcs, NULL);

			free(starxyz);
			free(fieldxy);

			if (bp->wcs_template) {
				char wcs_fn[1024];
				FILE* fout;
				qfits_header* hdr;
				char* tm;
				char val[32];

				snprintf(wcs_fn, sizeof(wcs_fn), bp->wcs_template, fieldnum);
				fout = fopen(wcs_fn, "wb");
				if (!fout) {
					logerr(bp, "Failed to open WCS output file %s: %s\n", wcs_fn, strerror(errno));
					exit(-1);
				}
				hdr = blind_wcs_get_header(&wcs);
				sprintf(val, "%g", sp->field_maxx);
				qfits_header_add(hdr, "IMAGEW", val, "Width of the image used to solve this WCS.", NULL);
				sprintf(val, "%g", sp->field_maxy);
				qfits_header_add(hdr, "IMAGEH", val, "Height of the image used to solve this WCS.", NULL);
				boilerplate_add_fits_headers(hdr);
				qfits_header_add(hdr, "HISTORY", "This WCS header was created by the program \"whynot2\".", NULL, NULL);
				tm = qfits_get_datetime_iso8601();
				qfits_header_add(hdr, "DATE", tm, "Date this file was created.", NULL);
				if (qfits_header_dump(hdr, fout)) {
					logerr(bp, "Failed to write FITS WCS header.\n");
					exit(-1);
				}
				fits_pad_file(fout);
				qfits_header_destroy(hdr);
				fclose(fout);
			}
		}

		quadids = il_new(256);
		starids = il_new(256);

		for (i=0; i<sp->nfield; i++) {
			uint* quads;
			uint nquads;
			int k;
			if (fieldtoind[i] == -1)
				continue;
			qidxfile_get_quads(bp->qidx, fieldtoind[i], &quads, &nquads);
			logmsg(bp, "Field obj %i / Index star %i is part of %i quads.\n",
				   i, fieldtoind[i], nquads);
			for (k=0; k<nquads; k++) {
				//logmsg(bp, "star %i, quad %i\n", fieldtoind[i], quads[k]);
				int ind = il_insert_ascending(quadids, quads[k]);
				il_insert(starids, ind, fieldtoind[i]);
			}
		}

		goodquads = il_new(16);
		goodstars = il_new(16);

		uq = 0;
		for (i=0; i<il_size(quadids);) {
			int k;
			int quad = il_get(quadids, i);
			int nstars;
			uq++;
			//logmsg(bp, "quad %i, star %i\n", quad, il_get(starids, i));
			for (k=i+1; k<il_size(quadids); k++) {
				if (il_get(quadids, k) != quad)
					break;
				//logmsg(bp, "quad %i, star %i\n", quad, il_get(starids, k));
			}
			nstars = k - i;
			logmsg(bp, "Quad %i has %i matched stars in the field.\n",
				   quad, nstars);
			if (nstars == 4) {
				il_append(goodquads, quad);
				il_append(goodstars, il_get(starids, i));
				il_append(goodstars, il_get(starids, i+1));
				il_append(goodstars, il_get(starids, i+2));
				il_append(goodstars, il_get(starids, i+3));
			}
			i = k;
		}
		logmsg(bp, "%i quads at least partially contained in the field.\n", uq);
		logmsg(bp, "%i quads completely contained in the field.\n", il_size(goodquads));

		// Use the WCS we computed to project the quad into field coords.
		for (i=0; i<il_size(quadids);) {
			int k;
			int quad = il_get(quadids, i);
			uint starabcd[4];
			int m;
			for (k=i+1; k<il_size(quadids); k++) {
				if (il_get(quadids, k) != quad)
					break;
			}
			
			getquadids(quad, starabcd, starabcd+1, starabcd+2, starabcd+3);
			logmsg(bp, "Quad FieldXY ");
			for (m=0; m<4; m++) {
				double xyz[3];
				double px, py;
				getstarcoord(starabcd[m], xyz);
				tan_xyzarr2pixelxy(&wcs, xyz, &px, &py);
				logmsg(bp, "%g %g ", px, py);
			}
			logmsg(bp, "\n");
			i = k;
		}

		goodfields = il_new(16);
		goodmaxfields = il_new(16);

		for (i=0; i<il_size(goodquads); i++) {
			int quad = il_get(goodquads, i);
			int k;
			int maxfld = -1;
			for (k=0; k<4; k++) {
				int star = il_get(goodstars, i*4 + k);
				int fld = intmap_get(mapif, star, -1);
				if (fld == -1) {
					logerr(bp, "No field obj found for star %i\n", star);
					exit(-1);
				}
				il_append(goodfields, fld);
				if (fld > maxfld)
					maxfld = fld;
			}
			il_append(goodmaxfields, maxfld);
			logmsg(bp, "Quad %i contains field stars %i, %i, %i, %i (max %i)\n",
				   quad,
				   il_get(goodfields, i*4),
				   il_get(goodfields, i*4+1),
				   il_get(goodfields, i*4+2),
				   il_get(goodfields, i*4+3),
				   maxfld);
		}

		// sort-of sort them by order they will be found
		// (just sorted by max field obj, not fully sorted)
		quadorder = il_new(16);
		for (i=0; i<sp->nfield; i++) {
			int k;
			for (k=0; k<il_size(goodmaxfields); k++) {
				if (il_get(goodmaxfields, k) == i)
					il_append(quadorder, k);
			}
		}

		for (i=0; i<il_size(quadorder); i++) {
			int ind = il_get(quadorder, i);
			int quad = il_get(goodquads, ind);
			int starids[4];
			uint starabcd[4];
			int fieldabcd[4];
			double starcode[4];
			double fieldcode[4];
			double fieldscale;
			double starscale;

			//double fieldxy[8];
			double starxyz[12];

			double codeerr;

			int k;
			for (k=0; k<4; k++)
				starids[k] = il_get(goodstars, ind*4 + k);
			logmsg(bp, "Quad %i contains index stars %i, %i, %i, %i\n",
				   quad, starids[0], starids[1], starids[2], starids[3]);
			getquadids(quad, starabcd, starabcd+1, starabcd+2, starabcd+3);
			logmsg(bp, "ABCD %i, %i, %i, %i\n",
				   starabcd[0], starabcd[1], starabcd[2], starabcd[3]);
			for (k=0; k<4; k++)
				fieldabcd[k] = intmap_get(mapif, starabcd[k], -1);
			logmsg(bp, "Field ABCD %i, %i, %i, %i\n",
				   fieldabcd[0], fieldabcd[1], fieldabcd[2], fieldabcd[3]);

			compute_field_code(sp->field + fieldabcd[0]*2,
							   sp->field + fieldabcd[1]*2,
							   sp->field + fieldabcd[2]*2,
							   sp->field + fieldabcd[3]*2,
							   fieldcode, &fieldscale);
			fieldscale = sqrt(fieldscale);

			for (k=0; k<4; k++)
				getstarcoord(starabcd[k], starxyz + k*3);

			compute_star_code(starxyz,
							  starxyz+3,
							  starxyz+6,
							  starxyz+9,
							  starcode);

			starscale = distsq2arcsec(distsq(starxyz, starxyz+3, 3));

			logmsg(bp, "Field code: [%6.4f, %6.4f, %6.4f, %6.4f]\n",
				   fieldcode[0], fieldcode[1], fieldcode[2], fieldcode[3]);

			logmsg(bp, "Index code: [%6.4f, %6.4f, %6.4f, %6.4f]\n",
				   starcode[0], starcode[1], starcode[2], starcode[3]);

			codeerr = sqrt(distsq(fieldcode, starcode, 4));

			logmsg(bp, "Code error: %g\n", codeerr);
			logmsg(bp, "Field scale: %g pixels\n", fieldscale);
			logmsg(bp, "Index scale: %g arcsec\n", starscale);
			logmsg(bp, "Scale: %g arcsec/pix\n", starscale/fieldscale);

		}

	cleanup:
		if (bp->indexrdls) {
			if (rdlist_fix_field(bp->indexrdls)) {
				logerr(bp, "Failed to fix index RDLS field header.\n");
			}
		}

		free(bp->truerd);
		bp->truerd = NULL;
		free(truexyz);
		free(fieldtoind);

		intmap_free(mapif);
		if (quadids)
			il_free(quadids);
		if (starids)
			il_free(starids);
		if (goodquads)
			il_free(goodquads);
		if (goodstars)
			il_free(goodstars);
		if (goodfields)
			il_free(goodfields);
		if (goodmaxfields)
			il_free(goodmaxfields);
		if (quadorder)
			il_free(quadorder);
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
