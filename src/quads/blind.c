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
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <ctype.h>

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
#include "ioutils.h"
#include "starkd.h"
#include "codekd.h"
#include "boilerplate.h"
#include "fitsioutils.h"
#include "handlehits.h"
#include "blind_wcs.h"
#include "qfits_error.h"
#include "qfits_cache.h"
#include "tweak_internal.h"
#include "rdlist.h"

static void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n", progname);
}

static void solve_fields();
static int read_parameters();

#define DEFAULT_CODE_TOL .01
#define DEFAULT_PARITY PARITY_BOTH
#define DEFAULT_TWEAK_ABORDER 3
#define DEFAULT_TWEAK_ABPORDER 3
#define DEFAULT_INDEX_JITTER 1.0  // arcsec

struct blind_params {
	// Jitter in the index, in arcseconds.
	double index_jitter;
	// Filenames
	char* indexname;
	char *fieldfname, *matchfname, *donefname, *startfname, *logfname;
	char *donescript;
	char *solved_in, *solved_out;
	char* cancelfname;
	char* rdlsfname;
	char* indexrdlsfname;

	// filename template (sprintf format with %i for field number)
	char* wcs_template;
	// Solvedserver ip:port
	char *solvedserver;
	// Fields to solve
	pl* indexes;

	// FIXME - more sensible logging!
	// Logfile
	FILE* logfd;
	int dup_stderr;
	int dup_stdout;
	bool quiet;
	bool silent;
	bool verbose;

	// xylist column names
	char* xcolname, *ycolname;
	// FITS keyword to copy from xylist to matchfile.
	char* fieldid_key;

	int parity;
	double codetol;
	int startdepth;
	int enddepth;
	// arcsec/pixel bounds
	double funits_lower;
	double funits_upper;
	// quad AB distance (arcsec) bounds
	double index_scale_upper;
	double index_scale_lower;

	int fieldid;
	int indexid;
	int healpix;

	int nagree_toverify;
	double agreetol;

	double verify_dist2;
	double verify_pix;

	double overlap_toprint;
	double overlap_tokeep;
	double overlap_tosolve;

	int ninfield_tokeep;
	int ninfield_tosolve;
	int maxquads;
	int maxmatches;
	double cxdx_margin;

	int firstfield, lastfield;
	il* fieldlist;

	// Does index have CIRCLE (codes in the circle, not the box)?
	bool circle;

	int nverified;

	matchfile* mf;
	idfile* id;
	quadfile* quads;
	xylist* xyls;
	startree* starkd;
	codetree *codekd;
	rdlist* rdls;
	rdlist* indexrdls;

	handlehits* hits;

	int cpulimit;
	int timelimit;
	bool* p_quitNow;

	bool do_tweak;
	int tweak_aborder;
	int tweak_abporder;
};
typedef struct blind_params blind_params;

static blind_params bp;

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

static void logerr(const blind_params* bp, const char* format, ...) {
	va_list va;
	va_start(va, format);
	loglvl(1, bp, format, va);
	va_end(va);
}
static void logmsg(const blind_params* bp, const char* format, ...) {
	va_list va;
	va_start(va, format);
	loglvl(3, bp, format, va);
	va_end(va);
}
static void logverb(const blind_params* bp, const char* format, ...) {
	va_list va;
	va_start(va, format);
	loglvl(4, bp, format, va);
	va_end(va);
}

static void cpu_time_limit(int sig) {
	logmsg(&bp, "CPU time limit reached!\n");
	if (bp.p_quitNow) {
		*bp.p_quitNow = TRUE;
	}
}

static void wall_time_limit(int sig) {
	logmsg(&bp, "Wall-clock time limit reached!\n");
	if (bp.p_quitNow) {
		*bp.p_quitNow = TRUE;
	}
}

int main(int argc, char *argv[]) {
    uint numfields;
	char* progname = argv[0];
	int i;

	if (argc == 2 && strcmp(argv[1],"-s") == 0) {
		bp.silent = TRUE;
		fprintf(stderr, "premptive silence\n");
	}

	if (argc != 1 && !bp.silent) {
		printHelp(progname);
		exit(-1);
	}

	bp.fieldlist = il_new(256);
	bp.indexes = pl_new(16);
	qfits_err_statset(1);

	for (;;) {
		int I;
		tic();

		il_remove_all(bp.fieldlist);
		pl_remove_all(bp.indexes);

		memset(&bp, 0, sizeof(blind_params));

		bp.fieldid_key = strdup("FIELDID");
		bp.xcolname = strdup("X");
		bp.ycolname = strdup("Y");
		bp.parity = DEFAULT_PARITY;
		bp.codetol = DEFAULT_CODE_TOL;
		bp.firstfield = bp.lastfield = -1;
		bp.healpix = -1;

		if (read_parameters()) {
			free(bp.xcolname);
			free(bp.ycolname);
			free(bp.fieldid_key);
			break;
		}

		logmsg(&bp, "Params:\n");
		logmsg(&bp, "fields ");
		logmsg(&bp, "%s params:\n", progname);
		logmsg(&bp, "fields ");
		for (i=0; i<il_size(bp.fieldlist); i++)
			logmsg(&bp, "%i ", il_get(bp.fieldlist, i));
		logmsg(&bp, "\n");
		logmsg(&bp, "indexes:\n");
		for (i=0; i<pl_size(bp.indexes); i++)
			logmsg(&bp, "  %s\n", (char*)pl_get(bp.indexes, i));
		logmsg(&bp, "fieldfname %s\n", bp.fieldfname);
		logmsg(&bp, "fieldid %i\n", bp.fieldid);
		logmsg(&bp, "matchfname %s\n", bp.matchfname);
		logmsg(&bp, "startfname %s\n", bp.startfname);
		logmsg(&bp, "donefname %s\n", bp.donefname);
		logmsg(&bp, "donescript %s\n", bp.donescript);
		logmsg(&bp, "solved_in %s\n", bp.solved_in);
		logmsg(&bp, "solved_out %s\n", bp.solved_out);
		logmsg(&bp, "solvedserver %s\n", bp.solvedserver);
		logmsg(&bp, "cancel %s\n", bp.cancelfname);
		logmsg(&bp, "wcs %s\n", bp.wcs_template);
		logmsg(&bp, "fieldid_key %s\n", bp.fieldid_key);
		logmsg(&bp, "parity %i\n", bp.parity);
		logmsg(&bp, "codetol %g\n", bp.codetol);
		logmsg(&bp, "startdepth %i\n", bp.startdepth);
		logmsg(&bp, "enddepth %i\n", bp.enddepth);
		logmsg(&bp, "fieldunits_lower %g\n", bp.funits_lower);
		logmsg(&bp, "fieldunits_upper %g\n", bp.funits_upper);
		logmsg(&bp, "agreetol %g\n", bp.agreetol);
		logmsg(&bp, "verify_dist %g\n", distsq2arcsec(bp.verify_dist2));
		logmsg(&bp, "verify_pix %g\n", bp.verify_pix);
		logmsg(&bp, "nagree_toverify %i\n", bp.nagree_toverify);
		logmsg(&bp, "overlap_toprint %f\n", bp.overlap_toprint);
		logmsg(&bp, "overlap_tokeep %f\n", bp.overlap_tokeep);
		logmsg(&bp, "overlap_tosolve %f\n", bp.overlap_tosolve);
		logmsg(&bp, "ninfield_tokeep %i\n", bp.ninfield_tokeep);
		logmsg(&bp, "ninfield_tosolve %i\n", bp.ninfield_tosolve);
		logmsg(&bp, "xcolname %s\n", bp.xcolname);
		logmsg(&bp, "ycolname %s\n", bp.ycolname);
		logmsg(&bp, "maxquads %i\n", bp.maxquads);
		logmsg(&bp, "maxmatches %i\n", bp.maxmatches);
		logmsg(&bp, "quiet %i\n", bp.quiet);
		logmsg(&bp, "verbose %i\n", bp.verbose);
		logmsg(&bp, "logfname %s\n", bp.logfname);
		logmsg(&bp, "cpulimit %i\n", bp.cpulimit);
		logmsg(&bp, "timelimit %i\n", bp.timelimit);

		if (!pl_size(bp.indexes) || !bp.fieldfname || (bp.codetol < 0.0) || !bp.matchfname) {
			logerr(&bp, "Invalid params... this message is useless.\n");
			exit(-1);
		}

		if (bp.nagree_toverify && (bp.agreetol == 0.0)) {
			logerr(&bp, "If you specify 'nagree_toverify', you must also specify 'agreetol'.\n");
			exit(-1);
		}

		bp.mf = matchfile_open_for_writing(bp.matchfname);
		if (!bp.mf) {
			logerr(&bp, "Failed to open file %s to write match file.\n", bp.matchfname);
			exit(-1);
		}
		boilerplate_add_fits_headers(bp.mf->header);
		qfits_header_add(bp.mf->header, "HISTORY", "This file was created by the program \"blind\".", NULL, NULL);
		if (matchfile_write_header(bp.mf)) {
			logerr(&bp, "Failed to write matchfile header.\n");
			exit(-1);
		}
		
		// Read .xyls file...
		logmsg(&bp, "Reading fields file %s...", bp.fieldfname);
		bp.xyls = xylist_open(bp.fieldfname);
		if (!bp.xyls) {
			logerr(&bp, "Failed to read xylist.\n");
			exit(-1);
		}
		numfields = bp.xyls->nfields;
		logmsg(&bp, "got %u fields.\n", numfields);
		
		bp.xyls->xname = bp.xcolname;
		bp.xyls->yname = bp.ycolname;

		if (bp.solvedserver) {
			if (solvedclient_set_server(bp.solvedserver)) {
				logerr(&bp, "Error setting solvedserver.\n");
				exit(-1);
			}

			if ((il_size(bp.fieldlist) == 0) && (bp.firstfield != -1) && (bp.lastfield != -1)) {
				int j;
				il_free(bp.fieldlist);
				logmsg(&bp, "Contacting solvedserver to get field list...\n");
				bp.fieldlist = solvedclient_get_fields(bp.fieldid, bp.firstfield, bp.lastfield, 0);
				if (!bp.fieldlist) {
					logerr(&bp, "Failed to get field list from solvedserver.\n");
					exit(-1);
				}
				logmsg(&bp, "Got %i fields from solvedserver: ", il_size(bp.fieldlist));
				for (j=0; j<il_size(bp.fieldlist); j++) {
					logmsg(&bp, "%i ", il_get(bp.fieldlist, j));
				}
				logmsg(&bp, "\n");
			}
		}

		if (bp.rdlsfname) {
			bp.rdls = rdlist_open_for_writing(bp.rdlsfname);
			if (bp.rdls) {
				if (rdlist_write_header(bp.rdls)) {
					logerr(&bp, "Failed to write RDLS header.\n");
					rdlist_close(bp.rdls);
					bp.rdls = NULL;
				}
			} else {
				logerr(&bp, "Failed to open RDLS file %s for writing.\n",
						bp.rdlsfname);
			}
		}
		if (bp.indexrdlsfname) {
			bp.indexrdls = rdlist_open_for_writing(bp.indexrdlsfname);
			if (bp.indexrdls) {
				if (rdlist_write_header(bp.indexrdls)) {
					logerr(&bp, "Failed to write index RDLS header.\n");
					rdlist_close(bp.indexrdls);
					bp.indexrdls = NULL;
				}
			} else {
				logerr(&bp, "Failed to open index RDLS file %s for writing.\n",
						bp.indexrdlsfname);
			}
		}

		if (bp.startfname) {
			FILE* batchfid = NULL;
			logmsg(&bp, "Writing marker file %s...\n", bp.startfname);
			batchfid = fopen(bp.startfname, "wb");
			if (batchfid)
				fclose(batchfid);
			else
				logerr(&bp, "Failed to write marker file %s: %s\n", bp.startfname, strerror(errno));
		}

		for (I=0; I<pl_size(bp.indexes); I++) {
			char *idfname, *treefname, *quadfname, *startreefname;
			char* fname = pl_get(bp.indexes, I);
			sighandler_t oldsigcpu = NULL;
			sighandler_t oldsigalarm = NULL;

			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);
			bp.indexname = fname;

			// Read .ckdt file...
			logmsg(&bp, "Reading code KD tree from %s...\n", treefname);
			bp.codekd = codetree_open(treefname);
			if (!bp.codekd)
				exit(-1);
			logmsg(&bp, "  (%d quads, %d nodes, dim %d).\n",
				   codetree_N(bp.codekd), codetree_nodes(bp.codekd), codetree_D(bp.codekd));

			// Read .quad file...
			logmsg(&bp, "Reading quads file %s...\n", quadfname);
			bp.quads = quadfile_open(quadfname, 0);
			if (!bp.quads) {
				logerr(&bp, "Couldn't read quads file %s\n", quadfname);
				exit(-1);
			}
			bp.index_scale_upper = quadfile_get_index_scale_arcsec(bp.quads);
			bp.index_scale_lower = quadfile_get_index_scale_lower_arcsec(bp.quads);
			bp.indexid = bp.quads->indexid;
			bp.healpix = bp.quads->healpix;

			// See if index contains JITTER header... if so, set index_jitter to that value.
			{
				double ijitter = qfits_header_getdouble(bp.quads->header, "JITTER", 0.0);
				if (ijitter > 0.0) {
					bp.index_jitter = ijitter;
				} else {
					bp.index_jitter = DEFAULT_INDEX_JITTER;
				}
				logmsg(&bp, "Setting index jitter to %g arcsec.\n", bp.index_jitter);
			}

			logmsg(&bp, "Index scale: [%g, %g] arcmin, [%g, %g] arcsec\n",
				   bp.index_scale_lower/60.0, bp.index_scale_upper/60.0, bp.index_scale_lower, bp.index_scale_upper);

			// Read .skdt file...
			logmsg(&bp, "Reading star KD tree from %s...\n", startreefname);
			bp.starkd = startree_open(startreefname);
			if (!bp.starkd) {
				logerr(&bp, "Failed to read star kdtree %s\n", startreefname);
				exit(-1);
			}

			logmsg(&bp, "  (%d stars, %d nodes, dim %d).\n",
				   startree_N(bp.starkd), startree_nodes(bp.starkd), startree_D(bp.starkd));

			// If the code kdtree has CXDX set, set cxdx_margin.
			if (qfits_header_getboolean(bp.codekd->header, "CXDX", 0))
				// 1.5 = sqrt(2) + fudge factor.
				bp.cxdx_margin = 1.5 * bp.codetol;

			// check for CIRCLE field in ckdt header...
			bp.circle = qfits_header_getboolean(bp.codekd->header, "CIRCLE", 0);

			logmsg(&bp, "ckdt %s the CIRCLE header.\n",
				   (bp.circle ? "contains" : "does not contain"));

			// Read .id file...
			bp.id = idfile_open(idfname, 0);
			if (!bp.id) {
				logmsg(&bp, "Couldn't open id file %s.\n", idfname);
				logmsg(&bp, "(Note, this won't cause trouble; you just won't get star IDs for matching quads.)\n");
			}

			// Set CPU time limit.
			if (bp.cpulimit) {
				struct rusage r;
				struct rlimit rlim;
				int sofar;

				if (getrusage(RUSAGE_SELF, &r)) {
					logerr(&bp, "Failed to get resource usage: %s\n", strerror(errno));
					exit(-1);
				}
				sofar = ceil((float)(r.ru_utime.tv_sec + r.ru_stime.tv_sec) +
							 (float)(1e-6 * r.ru_utime.tv_usec + r.ru_stime.tv_usec));

				if (getrlimit(RLIMIT_CPU, &rlim)) {
					logerr(&bp, "Failed to get CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}
				rlim.rlim_cur = bp.cpulimit + sofar;

				if (setrlimit(RLIMIT_CPU, &rlim)) {
					logerr(&bp, "Failed to set CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}

				oldsigcpu = signal(SIGXCPU, cpu_time_limit);
				if (oldsigcpu == SIG_ERR) {
					logerr(&bp, "Failed to set CPU time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Set wall-clock time limit.
			if (bp.timelimit) {
				alarm(bp.timelimit);
				oldsigalarm = signal(SIGALRM, wall_time_limit);
				if (oldsigalarm == SIG_ERR) {
					logerr(&bp, "Failed to set wall time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Do it!
			solve_fields();

			// Cancel wall-clock time limit.
			if (bp.timelimit) {
				alarm(0);
				if (signal(SIGALRM, oldsigalarm) == SIG_ERR) {
					logerr(&bp, "Failed to restore wall time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Restore CPU time limit.
			if (bp.cpulimit) {
				struct rlimit rlim;
				// Restore old CPU limit signal handler.
				if (signal(SIGXCPU, oldsigcpu) == SIG_ERR) {
					logerr(&bp, "Failed to restore CPU time limit signal handler: %s\n", strerror(errno));
					exit(-1);
				}
				// Remove CPU limit.
				if (getrlimit(RLIMIT_CPU, &rlim)) {
					logerr(&bp, "Failed to get CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}
				rlim.rlim_cur = rlim.rlim_max;
				if (setrlimit(RLIMIT_CPU, &rlim)) {
					logerr(&bp, "Failed to remove CPU time limit: %s\n", strerror(errno));
					exit(-1);
				}
			}

			// Clean up this index...
			codetree_close(bp.codekd);
			startree_close(bp.starkd);
			if (bp.id)
				idfile_close(bp.id);
			quadfile_close(bp.quads);

			bp.codekd = NULL;
			bp.starkd = NULL;
			bp.id = NULL;
			bp.quads = NULL;

			free_fn(treefname);
			free_fn(quadfname);
			free_fn(idfname);
			free_fn(startreefname);
		}

		if (bp.solvedserver) {
			solvedclient_set_server(NULL);
		}

		xylist_close(bp.xyls);
		if (matchfile_fix_header(bp.mf) ||
			matchfile_close(bp.mf)) {
			logerr(&bp, "Error closing matchfile.\n");
		}

		if (bp.rdls) {
			if (rdlist_fix_header(bp.rdls) ||
				rdlist_close(bp.rdls)) {
				logerr(&bp, "Failed to close RDLS file.\n");
			}
			bp.rdls = NULL;
		}
		if (bp.indexrdls) {
			if (rdlist_fix_header(bp.indexrdls) ||
				rdlist_close(bp.indexrdls)) {
				logerr(&bp, "Failed to close index RDLS file.\n");
			}
			bp.indexrdls = NULL;
		}

		if (bp.donefname) {
			FILE* batchfid = NULL;
			logmsg(&bp, "Writing marker file %s...\n", bp.donefname);
			batchfid = fopen(bp.donefname, "wb");
			if (batchfid)
				fclose(batchfid);
			else
				logerr(&bp, "Failed to write marker file %s: %s\n", bp.donefname, strerror(errno));
		}

		if (bp.donescript) {
			int rtn = system(bp.donescript);
			if (rtn == -1) {
				logerr(&bp, "Donescript failed.\n");
			} else {
				logmsg(&bp, "Donescript returned %i.\n", rtn);
			}
		}

		if (!bp.silent)
			toc();

		// Put stdout and stderr back to the way they were!
		if (bp.logfname) {
			if (dup2(bp.dup_stdout, fileno(stdout)) == -1) {
				logerr(&bp, "Failed to dup2() back to stdout.\n");
			}
			if (dup2(bp.dup_stderr, fileno(stderr)) == -1) {
				logerr(&bp, "Failed to dup2() back to stderr.\n");
			}
			if (close(bp.dup_stdout) || close(bp.dup_stderr)) {
				logerr(&bp, "Failed to close duplicate stdout/stderr: %s\n", strerror(errno));
			}
			if (fclose(bp.logfd)) {
				logerr(&bp, "Failed to close log file: %s\n",
						strerror(errno));
			}
		}

		free(bp.logfname);
		free(bp.donefname);
		free(bp.donescript);
		free(bp.startfname);
		free(bp.solved_in);
		free(bp.solved_out);
		free(bp.solvedserver);
		free(bp.cancelfname);
		free(bp.matchfname);
		free(bp.rdlsfname);
		free(bp.indexrdlsfname);
		free(bp.xcolname);
		free(bp.ycolname);
		free(bp.wcs_template);
		free(bp.fieldid_key);
		free_fn(bp.fieldfname);
	}

	il_free(bp.fieldlist);
	pl_free(bp.indexes);
	qfits_cache_purge(); // for valgrind
	return 0;
}

static int read_parameters() {
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

		logmsg(&bp, "Command: %s\n", line);

		if (line[0] == '#') {
			logmsg(&bp, "Skipping comment.\n");
			continue;
		}
		// skip blank lines.
		if (line[0] == '\0') {
			continue;
		}
		if (is_word(line, "help", &nextword)) {
			logmsg(&bp, "No help soup for you!\n  (use the source, Luke)\n");
		} else if (is_word(line, "cpulimit ", &nextword)) {
			bp.cpulimit = atoi(nextword);
		} else if (is_word(line, "timelimit ", &nextword)) {
			bp.timelimit = atoi(nextword);
		} else if (is_word(line, "agreetol ", &nextword)) {
			bp.agreetol = atof(nextword);
		} else if (is_word(line, "verify_dist ", &nextword)) {
			bp.verify_dist2 = arcsec2distsq(atof(nextword));
		} else if (is_word(line, "verify_pix ", &nextword)) {
			bp.verify_pix = atof(nextword);
		} else if (is_word(line, "nagree_toverify ", &nextword)) {
			bp.nagree_toverify = atoi(nextword);
		} else if (is_word(line, "overlap_tosolve ", &nextword)) {
			bp.overlap_tosolve = atof(nextword);
		} else if (is_word(line, "overlap_tokeep ", &nextword)) {
			bp.overlap_tokeep = atof(nextword);
		} else if (is_word(line, "overlap_toprint ", &nextword)) {
			bp.overlap_toprint = atof(nextword);
		} else if (is_word(line, "min_ninfield ", &nextword)) {
			// LEGACY
			logmsg(&bp, "Warning, the \"min_ninfield\" command is deprecated."
					"Use \"ninfield_tokeep\" and \"ninfield_tosolve\" instead.\n");
			bp.ninfield_tokeep = bp.ninfield_tosolve = atoi(nextword);
		} else if (is_word(line, "ninfield_tokeep ", &nextword)) {
			bp.ninfield_tokeep = atoi(nextword);
		} else if (is_word(line, "ninfield_tosolve ", &nextword)) {
			bp.ninfield_tosolve = atoi(nextword);
		} else if (is_word(line, "match ", &nextword)) {
			bp.matchfname = strdup(nextword);
		} else if (is_word(line, "rdls ", &nextword)) {
			bp.rdlsfname = strdup(nextword);
		} else if (is_word(line, "indexrdls ", &nextword)) {
			bp.indexrdlsfname = strdup(nextword);
		} else if (is_word(line, "solved ", &nextword)) {
			free(bp.solved_in);
			free(bp.solved_out);
			bp.solved_in = strdup(nextword);
			bp.solved_out = strdup(nextword);
		} else if (is_word(line, "solved_in ", &nextword)) {
			free(bp.solved_in);
			bp.solved_in = strdup(nextword);
		} else if (is_word(line, "cancel ", &nextword)) {
			free(bp.cancelfname);
			bp.cancelfname = strdup(nextword);
		} else if (is_word(line, "solved_out ", &nextword)) {
			free(bp.solved_out);
			bp.solved_out = strdup(nextword);
		} else if (is_word(line, "log ", &nextword)) {
			// Open the log file...
			bp.logfname = strdup(nextword);
			bp.logfd = fopen(bp.logfname, "a");
			if (!bp.logfd) {
				logerr(&bp, "Failed to open log file %s: %s\n", bp.logfname, strerror(errno));
				goto bailout;
			}
			// Save old stdout/stderr...
			bp.dup_stdout = dup(fileno(stdout));
			if (bp.dup_stdout == -1) {
				logerr(&bp, "Failed to dup stdout: %s\n", strerror(errno));
				goto bailout;
			}
			bp.dup_stderr = dup(fileno(stderr));
			if (bp.dup_stderr == -1) {
				logerr(&bp, "Failed to dup stderr: %s\n", strerror(errno));
				goto bailout;
			}
			// Replace stdout/stderr with logfile...
			if (dup2(fileno(bp.logfd), fileno(stderr)) == -1) {
				logerr(&bp, "Failed to dup2 log file: %s\n", strerror(errno));
				goto bailout;
			}
			if (dup2(fileno(bp.logfd), fileno(stdout)) == -1) {
				logerr(&bp, "Failed to dup2 log file: %s\n", strerror(errno));
				goto bailout;
			}
			continue;
			bailout:
			if (bp.logfd) fclose(bp.logfd);
			free(bp.logfname);
			bp.logfname = NULL;
		} else if (is_word(line, "solvedserver ", &nextword)) {
			bp.solvedserver = strdup(nextword);
		} else if (is_word(line, "silent", &nextword)) {
			bp.silent = TRUE;
		} else if (is_word(line, "quiet", &nextword)) {
			bp.quiet = TRUE;
		} else if (is_word(line, "verbose", &nextword)) {
			bp.verbose = TRUE;
		} else if (is_word(line, "tweak", &nextword)) {
			bp.do_tweak = TRUE;
		} else if (is_word(line, "wcs ", &nextword)) {
			bp.wcs_template = strdup(nextword);
		} else if (is_word(line, "fieldid_key ", &nextword)) {
			free(bp.fieldid_key);
			bp.fieldid_key = strdup(nextword);
		} else if (is_word(line, "maxquads ", &nextword)) {
			bp.maxquads = atoi(nextword);
		} else if (is_word(line, "maxmatches ", &nextword)) {
			bp.maxmatches = atoi(nextword);
		} else if (is_word(line, "xcol ", &nextword)) {
			free(bp.xcolname);
			bp.xcolname = strdup(nextword);
		} else if (is_word(line, "ycol ", &nextword)) {
			free(bp.ycolname);
			bp.ycolname = strdup(nextword);
		} else if (is_word(line, "index ", &nextword)) {
			pl_append(bp.indexes, strdup(nextword));
		} else if (is_word(line, "field ", &nextword)) {
			bp.fieldfname = mk_fieldfn(nextword);
		} else if (is_word(line, "fieldid ", &nextword)) {
			bp.fieldid = atoi(nextword);
		} else if (is_word(line, "done ", &nextword)) {
			bp.donefname = strdup(nextword);
		} else if (is_word(line, "donescript ", &nextword)) {
			bp.donescript = strdup(nextword);
		} else if (is_word(line, "start ", &nextword)) {
			bp.startfname = strdup(nextword);
		} else if (is_word(line, "sdepth ", &nextword)) {
			bp.startdepth = atoi(nextword);
		} else if (is_word(line, "depth ", &nextword)) {
			bp.enddepth = atoi(nextword);
		} else if (is_word(line, "tol ", &nextword)) {
			bp.codetol = atof(nextword);
		} else if (is_word(line, "parity ", &nextword)) {
			bp.parity = atoi(nextword);
		} else if (is_word(line, "fieldunits_lower ", &nextword)) {
			bp.funits_lower = atof(nextword);
		} else if (is_word(line, "fieldunits_upper ", &nextword)) {
			bp.funits_upper = atof(nextword);
		} else if (is_word(line, "firstfield ", &nextword)) {
			bp.firstfield = atoi(nextword);
		} else if (is_word(line, "lastfield ", &nextword)) {
			bp.lastfield = atoi(nextword);
		} else if (is_word(line, "fields ", &nextword)) {
			char* str = nextword;
			char* endp;
			int i, firstfld = -1;
			for (;;) {
				int fld = strtol(str, &endp, 10);
				if (str == endp) {
					// non-numeric value
					logerr(&bp, "Couldn't parse: %.20s [etc]\n", str);
					break;
				}
				if (firstfld == -1) {
					il_insert_unique_ascending(bp.fieldlist, fld);
				} else {
					if (firstfld > fld) {
						logerr(&bp, "Ranges must be specified as <start>/<end>: %i/%i\n", firstfld, fld);
					} else {
						for (i=firstfld+1; i<=fld; i++) {
							il_insert_unique_ascending(bp.fieldlist, i);
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
			logmsg(&bp, "I didn't understand that command.\n");
		}
	}
}

static sip_t* tweak(MatchObj* mo, solver_params* p, startree* starkd) {
	tweak_t* twee = NULL;
	double *imgx = NULL, *imgy = NULL;
	int i;
	double* starxyz;
	int nstars;
	kdtree_qres_t* res = NULL;
	double fieldcenter[3];
	double fieldr2;
	sip_t* sip = NULL;

	fflush(NULL);
	logmsg(&bp, "Tweaking!\n");

	twee = tweak_new();
	if (bp.verify_dist2 > 0.0)
		twee->jitter = distsq2arcsec(bp.verify_dist2);
	else {
		twee->jitter = hypot(mo->scale * bp.verify_pix, bp.index_jitter);
		logmsg(&bp, "Pixel scale implied by this quad: %g arcsec/pix.\n", mo->scale);
		logmsg(&bp, "Star jitter: %g arcsec.\n", twee->jitter);
	}

	// pull out the field coordinates.
	imgx = malloc(p->nfield * sizeof(double));
	imgy = malloc(p->nfield * sizeof(double));
	for (i=0; i<p->nfield; i++) {
		imgx[i] = p->field[i*2 + 0];
		imgy[i] = p->field[i*2 + 1];
	}
	logmsg(&bp, "Pushing %i image coordinates.\n", p->nfield);
	tweak_push_image_xy(twee, imgx, imgy, p->nfield);

	// find all the index stars that are inside the circle that bounds
	// the field.
	for (i=0; i<3; i++)
		fieldcenter[i] = (mo->sMin[i] + mo->sMax[i]) / 2.0;
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
	logmsg(&bp, "Pushing %i star coordinates.\n", nstars);
	tweak_push_ref_xyz(twee, starxyz, nstars);

	tweak_push_wcs_tan(twee, &(mo->wcstan));
	twee->sip->a_order  = twee->sip->b_order  = bp.tweak_aborder;
	twee->sip->ap_order = twee->sip->bp_order = bp.tweak_abporder;

	logmsg(&bp, "Begin tweaking...\n");
	while (!(twee->state & TWEAK_HAS_LINEAR_CD)) {
		unsigned int r = tweak_advance_to(twee, TWEAK_HAS_LINEAR_CD);
		if (r == -1) {
			logerr(&bp, "Tweak error!\n");
			goto bailout;
		}
	}

	logmsg(&bp, "Done tweaking!\n");

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

static void verified(handlehits* hh, MatchObj* mo) {
	if (mo->overlap >= bp.overlap_toprint) {
		logverb(&bp, "    field %i", mo->fieldnum);
		if (hh->nagree_toverify)
			logverb(&bp, " (%i agree)", mo->nagree);
		logverb(&bp, ": overlap %4.1f%%: %i in field (%im/%iu/%ic)\n",
				100.0 * mo->overlap,
				mo->ninfield, mo->noverlap,
				(mo->ninfield - mo->noverlap - mo->nconflict), mo->nconflict);
	}
}

static int blind_handle_hit(solver_params* p, MatchObj* mo) {
	bool solved;

	// if verification was specified in pixel units, compute the verification
	// distance on the unit sphere...
	if (bp.verify_pix > 0.0) {
		bp.hits->verify_dist2 = arcsec2distsq(hypot(mo->scale * bp.verify_pix, bp.index_jitter));
	}

	solved = handlehits_add(bp.hits, mo);
	if (!solved)
		return 0;

	return 1;
}

static void solve_fields() {
	solver_params solver;
	double last_utime, last_stime;
	double utime, stime;
	struct timeval wtime, last_wtime;
	int nfields;
	double* field = NULL;
	int fi;
	double scalefudge = 0.0; // in pixels

	get_resource_stats(&last_utime, &last_stime, NULL);
	gettimeofday(&last_wtime, NULL);

	solver_default_params(&solver);
	solver.codekd = bp.codekd->tree;
	solver.endobj = bp.enddepth;
	solver.maxtries = bp.maxquads;
	solver.maxmatches = bp.maxquads;
	solver.codetol = bp.codetol;
	solver.handlehit = blind_handle_hit;
	solver.cxdx_margin = bp.cxdx_margin;
	solver.quiet = bp.quiet;

	if (bp.funits_upper != 0.0) {
		solver.arcsec_per_pixel_upper = bp.funits_upper;
		solver.minAB = bp.index_scale_lower / bp.funits_upper;

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
		scalefudge = bp.index_scale_upper * M_SQRT1_2 * bp.codetol / bp.funits_upper;

		solver.minAB -= scalefudge;

		logmsg(&bp, "Scale fudge: %g pixels.\n", scalefudge);
		logmsg(&bp, "Set minAB to %g\n", solver.minAB);
	}
	if (bp.funits_lower != 0.0) {
		solver.arcsec_per_pixel_lower = bp.funits_lower;
		solver.maxAB = bp.index_scale_upper / bp.funits_lower;
		solver.maxAB += scalefudge;
		logmsg(&bp, "Set maxAB to %g\n", solver.maxAB);
	}

	bp.hits = handlehits_new();
	bp.hits->agreetol = bp.agreetol;
	bp.hits->verify_dist2 = bp.verify_dist2;
	bp.hits->nagree_toverify = bp.nagree_toverify;
	bp.hits->overlap_tokeep  = bp.overlap_tokeep;
	bp.hits->overlap_tosolve = bp.overlap_tosolve;
	bp.hits->ninfield_tokeep  = bp.ninfield_tokeep;
	bp.hits->ninfield_tosolve = bp.ninfield_tosolve;
	bp.hits->startree = bp.starkd->tree;
	bp.hits->do_wcs = (bp.wcs_template ? 1 : 0);
	bp.hits->verified = verified;

	nfields = bp.xyls->nfields;

	for (fi=0; fi<il_size(bp.fieldlist); fi++) {
		int fieldnum;
		MatchObj template;
		int nfield;
		qfits_header* fieldhdr = NULL;

		if (bp.rdls) {
			if (rdlist_write_new_field(bp.rdls)) {
				logerr(&bp, "Failed to write RDLS field header.\n");
			}
		}
		if (bp.indexrdls) {
			if (rdlist_write_new_field(bp.indexrdls)) {
				logerr(&bp, "Failed to write index RDLS field header.\n");
			}
		}

		fieldnum = il_get(bp.fieldlist, fi);
		if (fieldnum >= nfields) {
			logerr(&bp, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			goto cleanup;
		}

		// Has the field already been solved?
		if (bp.solved_in) {
			if (solvedfile_get(bp.solved_in, fieldnum)) {
				// file exists; field has already been solved.
				logmsg(&bp, "Field %i: solvedfile %s: field has been solved.\n", fieldnum, bp.solved_in);
				goto cleanup;
			}
			solver.solvedfn = bp.solved_in;
		} else
			solver.solvedfn = NULL;

		if (bp.solvedserver) {
			if (solvedclient_get(bp.fieldid, fieldnum) == 1) {
				// field has already been solved.
				logmsg(&bp, "Field %i: field has already been solved.\n", fieldnum);
				goto cleanup;
			}
		}
		solver.do_solvedserver = (bp.solvedserver ? TRUE : FALSE);
		solver.cancelfn = bp.cancelfname;


		// Get the field.
		nfield = xylist_n_entries(bp.xyls, fieldnum);
		if (nfield == -1) {
			logerr(&bp, "Couldn't determine how many objects are in field %i.\n", fieldnum);
			goto cleanup;
		}
		field = realloc(field, 2 * nfield * sizeof(double));
		if (xylist_read_entries(bp.xyls, fieldnum, 0, nfield, field)) {
			logerr(&bp, "Failed to read field.\n");
			goto cleanup;
		}

		memset(&template, 0, sizeof(MatchObj));
		template.fieldnum = fieldnum;
		template.fieldfile = bp.fieldid;
		template.indexid = bp.indexid;
		template.healpix = bp.healpix;

		fieldhdr = xylist_get_field_header(bp.xyls, fieldnum);
		if (fieldhdr) {
			char* idstr = qfits_pretty_string(qfits_header_getstr(fieldhdr, bp.fieldid_key));
			if (idstr)
				strncpy(template.fieldname, idstr, sizeof(template.fieldname)-1);
			qfits_header_destroy(fieldhdr);
		}

		solver.fieldid = bp.fieldid;
		solver.fieldnum = fieldnum;
		solver.parity = bp.parity;
		solver.numtries = 0;
		solver.nummatches = 0;
		solver.numscaleok = 0;
		solver.numcxdxskipped = 0;
		solver.startobj = bp.startdepth;
		solver.field = field;
		solver.nfield = nfield;
		solver.quitNow = FALSE;
		solver.mo_template = &template;
		solver.circle = bp.circle;

		bp.p_quitNow = &solver.quitNow;

		bp.hits->field = field;
		bp.hits->nfield = nfield;

		logmsg(&bp, "\nSolving field %i.\n", fieldnum);

		// The real thing
		solve_field(&solver);

		bp.p_quitNow = NULL;

		logmsg(&bp, "field %i: tried %i quads, matched %i codes.\n",
			   fieldnum, solver.numtries, solver.nummatches);

		if (bp.maxquads && solver.numtries >= bp.maxquads) {
			logmsg(&bp, "  exceeded the number of quads to try: %i >= %i.\n",
					solver.numtries, bp.maxquads);
		}
		if (bp.maxmatches && solver.nummatches >= bp.maxmatches) {
			logmsg(&bp, "  exceeded the number of quads to match: %i >= %i.\n",
					solver.nummatches, bp.maxmatches);
		}
		if (solver.cancelled) {
			logmsg(&bp, "  cancelled at user request.\n");
		}

		// Write the keepable hits.
		if (bp.hits->keepers) {
			int i;
			for (i=0; i<pl_size(bp.hits->keepers); i++) {
				MatchObj* mo = pl_get(bp.hits->keepers, i);
				if (matchfile_write_match(bp.mf, mo))
					logmsg(&bp, "Field %i: error writing a match.\n", fieldnum);
			}
			if (matchfile_fix_header(bp.mf))
				logerr(&bp, "Failed to fix the matchfile header for field %i.\n", fieldnum);
		}

		if (bp.hits->bestmo) {
			sip_t* sip = NULL;
			// Field solved!
			logmsg(&bp, "Field %i solved with overlap %g (%i matched, %i tried, %i in index).\n", fieldnum,
				   bp.hits->bestmo->overlap,
				   bp.hits->bestmo->noverlap, bp.hits->bestmo->ninfield, bp.hits->bestmo->nindex);

			// Tweak, if requested.
			if (bp.do_tweak) {
				sip = tweak(bp.hits->bestmo, &solver, bp.starkd);
			}

			// Write WCS, if requested.
			if (bp.wcs_template) {
				char wcs_fn[1024];
				FILE* fout;
				qfits_header* hdr;
				char* tm;
				MatchObj* mo = bp.hits->bestmo;

				sprintf(wcs_fn, bp.wcs_template, fieldnum);
				fout = fopen(wcs_fn, "ab");
				if (!fout) {
					logerr(&bp, "Failed to open WCS output file %s: %s\n", wcs_fn, strerror(errno));
					exit(-1);
				}

				assert(bp.hits->bestmo->wcs_valid);

				if (sip)
					hdr = blind_wcs_get_sip_header(sip);
				else
					hdr = blind_wcs_get_header(&(bp.hits->bestmo->wcstan));

				boilerplate_add_fits_headers(hdr);
				qfits_header_add(hdr, "HISTORY", "This WCS header was created by the program \"blind\".", NULL, NULL);
				tm = qfits_get_datetime_iso8601();
				qfits_header_add(hdr, "DATE", tm, "Date this file was created.", NULL);
				fits_add_long_comment(hdr, "-- blind solver parameters: --");
				fits_add_long_comment(hdr, "Index name: %s", bp.indexname);
				fits_add_long_comment(hdr, "Field name: %s", bp.fieldfname);
				fits_add_long_comment(hdr, "X col name: %s", bp.xcolname);
				fits_add_long_comment(hdr, "Y col name: %s", bp.ycolname);
				fits_add_long_comment(hdr, "Parity: %i", bp.parity);
				fits_add_long_comment(hdr, "Codetol: %g", bp.codetol);
				fits_add_long_comment(hdr, "Start obj: %i", bp.startdepth);
				fits_add_long_comment(hdr, "End obj: %i", bp.enddepth);
				fits_add_long_comment(hdr, "Field scale lower: %g arcsec/pixel", bp.funits_lower);
				fits_add_long_comment(hdr, "Field scale upper: %g arcsec/pixel", bp.funits_upper);
				fits_add_long_comment(hdr, "Index scale lower: %g arcsec", bp.index_scale_lower);
				fits_add_long_comment(hdr, "Index scale upper: %g arcsec", bp.index_scale_upper);
				fits_add_long_comment(hdr, "Nagree: %i", bp.nagree_toverify);
				if (bp.nagree_toverify > 1) {
					fits_add_long_comment(hdr, "Agreement tolerance: %g arcsec", bp.agreetol);
				}
				fits_add_long_comment(hdr, "Verify distance: %g arcsec", distsq2arcsec(bp.verify_dist2));
				fits_add_long_comment(hdr, "Verify pixels: %g pix", bp.verify_pix);
				fits_add_long_comment(hdr, "Overlap to solve: %g", bp.overlap_tosolve);
				fits_add_long_comment(hdr, "N in field to solve: %i", bp.ninfield_tosolve);
				fits_add_long_comment(hdr, "Tweak: %s", (bp.do_tweak ? "yes" : "no"));
				if (bp.do_tweak) {
					fits_add_long_comment(hdr, "Tweak AB order: %i", bp.tweak_aborder);
					fits_add_long_comment(hdr, "Tweak ABP order: %i", bp.tweak_abporder);
				}

				fits_add_long_comment(hdr, "-- properties of the matching quad: --");
				fits_add_long_comment(hdr, "quadno: %i", mo->quadno);
				fits_add_long_comment(hdr, "stars: %i,%i,%i,%i", mo->star[0], mo->star[1], mo->star[2], mo->star[3]);
				fits_add_long_comment(hdr, "field: %i,%i,%i,%i", mo->field[0], mo->field[1], mo->field[2], mo->field[3]);
				fits_add_long_comment(hdr, "code error: %g", sqrt(mo->code_err));
				fits_add_long_comment(hdr, "noverlap: %i", mo->noverlap);
				fits_add_long_comment(hdr, "nconflict: %i", mo->nconflict);
				fits_add_long_comment(hdr, "ninfield: %i", mo->ninfield);
				fits_add_long_comment(hdr, "nindex: %i", mo->nindex);
				fits_add_long_comment(hdr, "overlap: %g", mo->overlap);
				fits_add_long_comment(hdr, "scale: %g arcsec/pix", mo->scale);
				fits_add_long_comment(hdr, "parity: %i", (int)mo->parity);
				fits_add_long_comment(hdr, "quads tried: %i", mo->quads_tried);
				fits_add_long_comment(hdr, "quads matched: %i", mo->quads_matched);
				fits_add_long_comment(hdr, "quads verified: %i", mo->nverified);
				fits_add_long_comment(hdr, "objs tried: %i", mo->objs_tried);
				fits_add_long_comment(hdr, "cpu time: %g", mo->timeused);
				fits_add_long_comment(hdr, "--");
				
				if (solver.mo_template && solver.mo_template->fieldname[0])
					qfits_header_add(hdr, bp.fieldid_key, solver.mo_template->fieldname, "Field name (copied from input field)", NULL);
				if (qfits_header_dump(hdr, fout)) {
					logerr(&bp, "Failed to write FITS WCS header.\n");
					exit(-1);
				}
				fits_pad_file(fout);
				qfits_header_destroy(hdr);
				fclose(fout);
			}

			if (bp.rdls) {
				double* radec = malloc(nfield * 2 * sizeof(double));
				int i;
				if (sip) {
					for (i=0; i<nfield; i++)
						sip_pixelxy2radec(sip,
										  field[i*2], field[i*2+1],
										  radec+i*2, radec+i*2+1);
				} else {
					for (i=0; i<nfield; i++)
						tan_pixelxy2radec(&(bp.hits->bestmo->wcstan),
										  field[i*2], field[i*2+1],
										  radec+i*2, radec+i*2+1);
				}
				if (rdlist_write_entries(bp.rdls, radec, nfield)) {
					logerr(&bp, "Failed to write RDLS entry.\n");
				}
				free(radec);
			}

			if (bp.indexrdls) {
				kdtree_qres_t* res = NULL;
				double* starxyz;
				int nstars;
				double* radec;
				int i;
				double fieldcenter[3];
				double fieldr2;
				MatchObj* mo = bp.hits->bestmo;
				// find all the index stars that are inside the circle that bounds
				// the field.
				for (i=0; i<3; i++)
					fieldcenter[i] = (mo->sMin[i] + mo->sMax[i]) / 2.0;
				fieldr2 = distsq(fieldcenter, mo->sMin, 3);
				// 1.05 is a little safety factor.
				res = kdtree_rangesearch_options(bp.starkd->tree, fieldcenter,
												 fieldr2 * 1.05,
												 KD_OPTIONS_SMALL_RADIUS |
												 KD_OPTIONS_RETURN_POINTS);
				if (!res || !res->nres) {
					logmsg(&bp, "No index stars found!\n");
				}
				starxyz = res->results.d;
				nstars = res->nres;

				radec = malloc(nstars * 2 * sizeof(double));
				for (i=0; i<nstars; i++)
					xyzarr2radec(starxyz + i*3, radec+i*2, radec+i*2+1);
				for (i=0; i<2*nstars; i++)
					radec[i] = rad2deg(radec[i]);

				if (rdlist_write_entries(bp.indexrdls, radec, nstars)) {
					logerr(&bp, "Failed to write index RDLS entry.\n");
				}

				free(radec);
				kdtree_free_query(res);
			}

			if (sip) {
				sip_free(sip);
			}
			// Record in solved file, or send to solved server.
			if (bp.solved_out) {
				logmsg(&bp, "Field %i solved: writing to file %s to indicate this.\n", fieldnum, bp.solved_out);
				if (solvedfile_set(bp.solved_out, fieldnum)) {
					logerr(&bp, "Failed to write to solvedfile %s.\n", bp.solved_out);
				}
			}
			if (bp.solvedserver) {
				solvedclient_set(bp.fieldid, fieldnum);
			}

		} else {
			// Field unsolved.
			logmsg(&bp, "Field %i is unsolved.\n", fieldnum);
		}

		handlehits_free_matchobjs(bp.hits);
		handlehits_clear(bp.hits);

		get_resource_stats(&utime, &stime, NULL);
		gettimeofday(&wtime, NULL);
		logmsg(&bp, "  Spent %g s user, %g s system, %g s total, %g s wall time.\n",
			   (utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime),
			   millis_between(&last_wtime, &wtime) * 0.001);

		last_utime = utime;
		last_stime = stime;
		last_wtime = wtime;

	cleanup:
		if (bp.rdls) {
			if (rdlist_fix_field(bp.rdls)) {
				logerr(&bp, "Failed to fix RDLS field header.\n");
			}
		}
		if (bp.indexrdls) {
			if (rdlist_fix_field(bp.indexrdls)) {
				logerr(&bp, "Failed to fix index RDLS field header.\n");
			}
		}
	}

	free(field);
	handlehits_free(bp.hits);
}

void getquadids(uint thisquad, uint *iA, uint *iB, uint *iC, uint *iD) {
	quadfile_get_starids(bp.quads, thisquad, iA, iB, iC, iD);
}

void getstarcoord(uint iA, double *sA) {
	startree_get(bp.starkd, iA, sA);
}

uint64_t getstarid(uint iA) {
	if (bp.id)
		return idfile_get_anid(bp.id, iA);
	return 0;
}
