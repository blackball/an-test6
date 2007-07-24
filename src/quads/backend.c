/*
 This file is part of the Astrometry.net suite.
 Copyright 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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
 * Accepts an augmented xylist that describes a field or set of fields to solve.
 * Reads a config file to find local indices, and merges information about the
 * indices with the job description to create an input file for 'blind'.  Runs blind
 * and merges the results.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>

#include <getopt.h>

#include "fileutil.h"
#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "math.h"
#include "fitsioutils.h"

#include "qfits.h"

static int help_flag;

static struct option long_options[] =
    {
	    // flags
	    {"help",   no_argument, &help_flag, 1},
	    {"config", required_argument, 0, 'c'},
	    {"input",  required_argument, 0, 'i'},
        {"cd",     no_argument,       0, 'C'},
	    {0, 0, 0, 0}
    };

static const char* OPTIONS = "hc:i:";

static void print_help(const char* progname)
{
	printf("Usage:   %s [options] <augmented xylist>\n"
	       "   [-c <backend config file>]  (default \"backend.cfg\")\n"
	       "   [-i <blind input filename>]: save the input file used for blind.\n"
           "   [-C]: change to the directory containing the xylist before running blind.\n"
	       "\n", progname);
}


static const char* default_blind_command = "blind";


struct indexinfo {
	char* indexname;
	// quad size
	double losize;
	double hisize;
};
typedef struct indexinfo indexinfo_t;

struct backend {
	bl* indexinfos;
	int ibiggest;
	int ismallest;
	double sizesmallest;
	double sizebiggest;
	bool inparallel;
	char* blind;
	double minwidth;
	double maxwidth;
};
typedef struct backend backend_t;

static int get_index_scales(const char* indexname,
                            double* losize, double* hisize)
{
	char *quadfname;
	quadfile* quads;
	double hi, lo;

	quadfname = mk_quadfn(indexname);
	printf("Reading quads file %s...\n", quadfname);
	quads = quadfile_open(quadfname, 0);
	if (!quads) {
		printf("Couldn't read quads file %s\n", quadfname);
		free_fn(quadfname);
		return -1;
	}
	free_fn(quadfname);
	lo = quadfile_get_index_scale_lower_arcsec(quads);
	hi = quadfile_get_index_scale_arcsec(quads);
	if (losize)
		*losize = lo;
	if (hisize)
		*hisize = hi;
	printf("Stars: %i, Quads: %i.\n", quads->numstars, quads->numquads);
	printf("Index scale: [%g, %g] arcmin, [%g, %g] arcsec\n",
	       lo / 60.0, hi / 60.0, lo, hi);
	quadfile_close(quads);
	return 0;
}

static int add_index(backend_t* backend, char* index)
{
	double lo, hi;
	indexinfo_t ii;
	if (get_index_scales(index, &lo, &hi)) {
		printf("Failed to get the range of quad scales for index \"%s\".\n", index);
		return -1;
	}
	ii.indexname = strdup(index);
	ii.losize = lo;
	ii.hisize = hi;
	bl_append(backend->indexinfos, &ii);
	if (ii.losize < backend->sizesmallest) {
		backend->sizesmallest = ii.losize;
		backend->ismallest = bl_size(backend->indexinfos) - 1;
	}
	if (ii.hisize > backend->sizebiggest) {
		backend->sizebiggest = ii.hisize;
		backend->ibiggest = bl_size(backend->indexinfos) - 1;
	}
	return 0;
}

static int parse_config_file(FILE* fconf, backend_t* backend)
{
	while (1) {
		char buffer[10240];
		char* nextword;
		char* line;
		if (!fgets(buffer, sizeof(buffer), fconf)) {
			if (feof(fconf))
				break;
			printf("Failed to read a line from the config file: %s\n", strerror(errno));
			return -1;
		}
		line = buffer;
		// strip off newline
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = '\0';
		// skip leading whitespace:
		while (*line && isspace(*line))
			line++;
		// skip comments
		if (line[0] == '#')
			continue;
		// skip blank lines.
		if (line[0] == '\0')
			continue;

		if (is_word(line, "index ", &nextword)) {
			if (add_index(backend, nextword)) {
				return -1;
			}
		} else if (is_word(line, "blind ", &nextword)) {
			free(backend->blind);
			backend->blind = strdup(nextword);
		} else if (is_word(line, "inparallel", &nextword)) {
			backend->inparallel = TRUE;
		} else if (is_word(line, "minwidth ", &nextword)) {
			backend->minwidth = atof(nextword);
		} else if (is_word(line, "maxwidth ", &nextword)) {
			backend->maxwidth = atof(nextword);
		} else {
			printf("Didn't understand this config file line: \"%s\"\n", line);
		}

	}
	return 0;
}

struct job_t
{
	char* fieldfile;
	double imagew;
	double imageh;
	bool run;
	double poserr;
	char* solvedfile;
	char* matchfile;
	char* rdlsfile;
	char* wcsfile;
	char* cancelfile;
	int timelimit;
	int cpulimit;
	int parity;
	bool tweak;
	int tweakorder;
	dl* scales;
	il* depths;
	il* fields;
	double odds_toprint;
	double odds_tokeep;
	double odds_tosolve;
	double image_fraction;
	double codetol;
	double distractor_fraction;
	// Contains tan_t structs.
	bl* verify_wcs;
    bool include_default_scales;
};
typedef struct job_t job_t;

static job_t* job_new()
{
	job_t* job = calloc(1, sizeof(job_t));
	if (!job) {
		printf("Failed to allocate a new job_t.\n");
		return NULL;
	}
	// Default values:
	job->poserr = 1.0;
	job->parity = PARITY_BOTH;
	job->tweak = TRUE;
	job->tweakorder = 3;
	job->scales = dl_new(8);
	job->depths = il_new(8);
	job->fields = il_new(32);
	job->odds_toprint = 1e3;
	job->odds_tokeep = 1e9;
	job->odds_tosolve = 1e9;
	job->image_fraction = 1.0;
	job->codetol = 0.01;
	job->distractor_fraction = 0.25;
	job->verify_wcs = bl_new(8, sizeof(tan_t));

	return job;
}

static void job_free(job_t* job)
{
	if (!job)
		return ;
	free(job->solvedfile);
	free(job->matchfile);
	free(job->rdlsfile);
	free(job->wcsfile);
	free(job->cancelfile);
	dl_free(job->scales);
	il_free(job->depths);
	il_free(job->fields);
	bl_free(job->verify_wcs);
	free(job);
}

static void job_print(job_t* job)
{
	int i;
	printf("Image size: %g x %g\n", job->imagew, job->imageh);
	printf("Positional error: %g pix\n", job->poserr);
	printf("Solved file: %s\n", job->solvedfile);
	printf("Match file: %s\n", job->matchfile);
	printf("RDLS file: %s\n", job->rdlsfile);
	printf("WCS file: %s\n", job->wcsfile);
	printf("Cancel file: %s\n", job->cancelfile);
	printf("Time limit: %i sec\n", job->timelimit);
	printf("CPU limit: %i sec\n", job->cpulimit);
	printf("Parity: %s\n", (job->parity == PARITY_NORMAL ? "pos" :
	                        (job->parity == PARITY_FLIP ? "neg" :
	                         (job->parity == PARITY_BOTH ? "both" : "(unknown)"))));
	printf("Tweak: %s\n", (job->tweak ? "yes" : "no"));
	printf("Tweak order: %i\n", job->tweakorder);
	printf("Odds to print: %g\n", job->odds_toprint);
	printf("Odds to keep: %g\n", job->odds_tokeep);
	printf("Odds to solve: %g\n", job->odds_tosolve);
	printf("Image fraction: %g\n", job->image_fraction);
	printf("Distractor fraction: %g\n", job->distractor_fraction);
	printf("Code tolerance: %g\n", job->codetol);
	printf("Scale ranges:\n");
	for (i = 0; i < dl_size(job->scales) / 2; i++) {
		double lo, hi;
		lo = dl_get(job->scales, i * 2);
		hi = dl_get(job->scales, i * 2 + 1);
		printf("  [%g, %g] arcsec/pix\n", lo, hi);
	}
	printf("Depths:");
	for (i = 0; i < il_size(job->depths); i++) {
		int depth = il_get(job->depths, i);
		printf(" %i", depth);
	}
	printf("\n");
	printf("Fields:");
	for (i = 0; i < il_size(job->fields) / 2; i++) {
		int lo, hi;
		lo = il_get(job->fields, i * 2);
		hi = il_get(job->fields, i * 2 + 1);
		if (lo == hi)
			printf(" %i", lo);
		else
			printf(" %i-%i", lo, hi);
	}
	printf("\n");
	printf("Verify WCS:\n");
	for (i = 0; i < bl_size(job->verify_wcs); i++) {
		tan_t* wcs = bl_access(job->verify_wcs, i);
		printf("  crpix (%g, %g)\n", wcs->crpix[0], wcs->crpix[1]);
		printf("  crval (%g, %g)\n", wcs->crval[0], wcs->crval[1]);
		printf("  cd  = ( %g, %g )\n", wcs->cd[0][0], wcs->cd[0][1]);
		printf("        ( %g, %g )\n", wcs->cd[1][0], wcs->cd[1][1]);
	}
	printf("Run: %s\n", (job->run ? "yes" : "no"));
}

#define WRITE(f, x, ...) \
do {\
  if (fprintf(f, x, ##__VA_ARGS__) < 0) { \
    fprintf(stderr, "Failed to write: %s\n", strerror(errno)); \
    return -1; \
  } \
} while(0)

static int job_write_blind_input(job_t* job, FILE* fout, backend_t* backend)
{
	int i, j, k;
	bool firsttime = TRUE;
	WRITE(fout, "timelimit %i\n", job->timelimit);
	WRITE(fout, "cpulimit %i\n", job->cpulimit);
	for (i = 0;; i++) {
		int startobj, endobj;
		if (il_size(job->depths) < 2) {
			if (i > 0)
				break;
			startobj = 0;
			endobj = 0;
		} else {
			if (i >= il_size(job->depths) - 1)
				break;
			startobj = il_get(job->depths, i);
			endobj = il_get(job->depths, i + 1);
		}

		for (j = 0; j < dl_size(job->scales) / 2; j++) {
			double fmin, fmax;
			double app_max, app_min;
			int nused;

			WRITE(fout, "sdepth %i\n", startobj);
			if (endobj)
				WRITE(fout, "depth %i\n", endobj);
			// arcsec per pixel range
			app_min = dl_get(job->scales, j * 2);
			app_max = dl_get(job->scales, j * 2 + 1);
			WRITE(fout, "fieldunits_lower %g\n", app_min);
			WRITE(fout, "fieldunits_upper %g\n", app_max);

			// range of quad sizes that could be found in the field,
			// in arcsec.
			fmax = 1.0 * MAX(job->imagew, job->imageh) * app_max;
			fmin = 0.1 * MIN(job->imagew, job->imageh) * app_min;

			// Select the indices that should be checked.
			nused = 0;
			for (k = 0; k < bl_size(backend->indexinfos); k++) {
				indexinfo_t* ii = bl_access(backend->indexinfos, k);
				if ((fmin > ii->hisize) || (fmax < ii->losize))
					continue;
				WRITE(fout, "index %s\n", ii->indexname);
				nused++;
			}
			// Use the smallest or largest index if no other one fits.
			if (!nused) {
				indexinfo_t* ii = NULL;
				if (fmin > backend->sizebiggest) {
					ii = bl_access(backend->indexinfos, backend->ibiggest);
				} else if (fmax < backend->sizesmallest) {
					ii = bl_access(backend->indexinfos, backend->ismallest);
				} else {
					assert(0);
				}
				WRITE(fout, "index %s\n", ii->indexname);
			}
			if (backend->inparallel)
				WRITE(fout, "indexes_inparallel\n");

			WRITE(fout, "fields");
			for (k = 0; k < il_size(job->fields) / 2; k++) {
				int lo = il_get(job->fields, k * 2);
				int hi = il_get(job->fields, k * 2 + 1);
				if (lo == hi)
					WRITE(fout, " %i", lo);
				else
					WRITE(fout, " %i/%i", lo, hi);
			}
			WRITE(fout, "\n");

			WRITE(fout, "parity %i\n", job->parity);
			WRITE(fout, "verify_pix %g\n", job->poserr);
			WRITE(fout, "tol %g\n", job->codetol);
			WRITE(fout, "distractors %g\n", job->distractor_fraction);
			WRITE(fout, "ratio_toprint %g\n", job->odds_toprint);
			WRITE(fout, "ratio_tokeep %g\n", job->odds_tokeep);
			WRITE(fout, "ratio_tosolve %g\n", job->odds_tosolve);
			WRITE(fout, "ratio_tobail %g\n", 1e-100);

			if (job->tweak) {
				WRITE(fout, "tweak\n");
				WRITE(fout, "tweak_aborder %i\n", job->tweakorder);
				WRITE(fout, "tweak_abporder %i\n", job->tweakorder);
				WRITE(fout, "tweak_skipshift\n");
			}

			WRITE(fout, "field %s\n", job->fieldfile);
			if (job->solvedfile)
				WRITE(fout, "solved %s\n", job->solvedfile);
			if (job->matchfile)
				WRITE(fout, "match %s\n", job->matchfile);
			if (job->rdlsfile)
				WRITE(fout, "indexrdls %s\n", job->rdlsfile);
			if (job->wcsfile)
				WRITE(fout, "wcs %s\n", job->wcsfile);
			if (job->cancelfile)
				WRITE(fout, "cancel %s\n", job->cancelfile);

			if (firsttime) {
				for (k = 0; k < bl_size(job->verify_wcs); k++) {
					tan_t* wcs = bl_access(job->verify_wcs, k);
					WRITE(fout, "verify_wcs %g %g %g %g %g %g %g %g\n",
					      wcs->crval[0], wcs->crval[1],
					      wcs->crpix[0], wcs->crpix[1],
					      wcs->cd[0][0], wcs->cd[0][1],
					      wcs->cd[1][0], wcs->cd[1][1]);
				}
				firsttime = FALSE;
			}

			WRITE(fout, "run\n\n");
		}

	}
	return 0;
}

static int run_blind(job_t* job, backend_t* backend, char* dir)
{
	int thepipe[2];
	pid_t pid;

	if (pipe(thepipe) == -1) {
		fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
		return -1;
	}

	fflush(stdout);
	fflush(stderr);

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Error fork()ing: %s\n", strerror(errno));
		return -1;
	} else if (pid == 0) {
		int old_stdin;
		// Child process.
		close(thepipe[1]);
		// bind stdin to thepipe[0].
		old_stdin = dup(STDIN_FILENO);
		if (old_stdin == -1) {
			fprintf(stderr, "Failed to save stdin: %s\n", strerror(errno));
			_exit( -1);
		}
		if (dup2(thepipe[0], STDIN_FILENO) == -1) {
			fprintf(stderr, "Failed to dup2 stdin: %s\n", strerror(errno));
			_exit( -1);
		}

		// Use a "system"-like command to allow fancier "blind" commands.
		if (execlp("/bin/sh", "/bin/sh", "-c", backend->blind, (char*)NULL)) {
			fprintf(stderr, "Failed to execlp blind: %s\n", strerror(errno));
			_exit( -1);
		}
		// execlp doesn't return.
	} else {
		FILE* fpipe;
		int status;
		// Parent process.
		close(thepipe[0]);
		fpipe = fdopen(thepipe[1], "a");
		if (!fpipe) {
			fprintf(stderr, "Failed to fdopen pipe: %s\n", strerror(errno));
			return -1;
		}
		// Write input to blind.
		if (job_write_blind_input(job, fpipe, backend)) {
			fprintf(stderr, "Failed to write input file to blind: %s\n", strerror(errno));
			return -1;
		}
		fclose(fpipe);

		// Wait for blind to finish.
		printf("Waiting for blind to finish (PID %i).\n", (int)pid);
		do {
			if (waitpid(pid, &status, 0) == -1) {
				fprintf(stderr, "Failed to waitpid() for blind: %s.\n", strerror(errno));
				return -1;
			}
			if (WIFSIGNALED(status)) {
				// (WTERMSIG(status) == SIGINT || WTERMSIG(status) == SIGQUIT)) {
				//fprintf(stderr, "Blind was killed.\n");
				fprintf(stderr, "Blind was killed by signal %i.\n", WTERMSIG(status));
				return -1;
			} else {
				int exitval = WEXITSTATUS(status);
				if (exitval == 127) {
					fprintf(stderr, "Blind executable not found.\n");
					return -1;
				} else if (exitval) {
					fprintf(stderr, "Blind executable failed: return value %i.\n", exitval);
					return -1;
				}
			}
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
		printf("Blind finished successfully.\n");
	}
	return 0;
}

job_t* parse_job_from_qfits_header(qfits_header* hdr)
{
	double dnil = -HUGE_VAL;
	job_t* job = job_new();

	job->imagew = qfits_header_getdouble(hdr, "IMAGEW", dnil);
	job->imageh = qfits_header_getdouble(hdr, "IMAGEH", dnil);
	if ((job->imagew == dnil) || (job->imageh == dnil) ||
		(job->imagew <= 0.0) || (job->imageh <= 0.0)) {
		printf("Must specify positive \"IMAGEW\" and \"IMAGEH\".\n");
		goto bailout;
	}
	job->run = qfits_header_getboolean(hdr, "ANRUN", 0);
	job->poserr = qfits_header_getdouble(hdr, "ANPOSERR", job->poserr);
	job->solvedfile = fits_get_dupstring(hdr, "ANSOLVED");
	job->matchfile = fits_get_dupstring(hdr, "ANMATCH");
	job->rdlsfile = fits_get_dupstring(hdr, "ANRDLS");
	job->wcsfile = fits_get_dupstring(hdr, "ANWCS");
	job->cancelfile = fits_get_dupstring(hdr, "ANCANCEL");
	job->timelimit = qfits_header_getint(hdr, "ANTLIM", job->timelimit);
	job->cpulimit = qfits_header_getint(hdr, "ANCLIM", job->cpulimit);
    job->include_default_scales = qfits_header_getboolean(hdr, "ANAPPDEF", 0);

	char *pstr = qfits_pretty_string(qfits_header_getstr(hdr, "ANPARITY"));
	if (pstr && !strcmp(pstr, "NEG")) {
		job->parity = PARITY_FLIP;
	} else if (pstr && !strcmp(pstr, "POS")) {
		job->parity = PARITY_NORMAL;
	}
	job->tweak = qfits_header_getboolean(hdr, "ANTWEAK", job->tweak);
	job->tweakorder = qfits_header_getint(hdr, "ANTWEAKO", job->tweakorder);
	int n = 1;
	while (1) {
		char key[64];
		double lo, hi;
		sprintf(key, "ANAPPL%i", n);
		lo = qfits_header_getdouble(hdr, key, dnil);
		if (lo == dnil)
			break;
		sprintf(key, "ANAPPU%i", n);
		hi = qfits_header_getdouble(hdr, key, dnil);
		if (hi == dnil)
			break;
		dl_append(job->scales, lo);
		dl_append(job->scales, hi);
		n++;
	}
	n = 1;
	while (1) {
		char key[64];
		int depth;
		sprintf(key, "ANDEPTH%i", n);
		depth = qfits_header_getint(hdr, key, -1);
		if (depth == -1)
			break;
		il_append(job->depths, depth);
		n++;
	}
	n = 1;
	while (1) {
		char key[64];
		int lo, hi;
		sprintf(key, "ANFDL%i", n);
		lo = qfits_header_getint(hdr, key, -1);
		if (lo == -1)
			break;
		sprintf(key, "ANFDU%i", n);
		hi = qfits_header_getint(hdr, key, -1);
		if (hi == -1)
			break;
		il_append(job->fields, lo);
		il_append(job->fields, hi);
		n++;
	}
	n = 1;
	while (1) {
		char key[64];
		int fld;
		sprintf(key, "ANFD%i", n);
		fld = qfits_header_getint(hdr, key, -1);
		if (fld == -1)
			break;
		il_append(job->fields, fld);
		il_append(job->fields, fld);
		n++;
	}
	job->odds_toprint = qfits_header_getdouble(hdr, "ANODDSPR", job->odds_toprint);
	job->odds_tokeep = qfits_header_getdouble(hdr, "ANODDSKP", job->odds_tokeep);
	job->odds_tosolve = qfits_header_getdouble(hdr, "ANODDSSL", job->odds_tosolve);
	job->image_fraction = qfits_header_getdouble(hdr, "ANIMFRAC", job->image_fraction);
	job->codetol = qfits_header_getdouble(hdr, "ANCTOL", job->codetol);
	job->distractor_fraction = qfits_header_getdouble(hdr, "ANDISTR", job->distractor_fraction);
	n = 1;
	while (1) {
		char key[64];
		tan_t wcs;
		char* keys[] = { "ANW%iPIX1", "ANW%iPIX2", "ANW%iVAL1", "ANW%iVAL2",
				 "ANW%iCD11", "ANW%iCD12", "ANW%iCD21", "ANW%iCD22" };
		double* vals[] = { &(wcs. crval[0]), &(wcs.crval[1]),
				   &(wcs.crpix[0]), &(wcs.crpix[1]),
				   &(wcs.cd[0][0]), &(wcs.cd[0][1]),
				   &(wcs.cd[1][0]), &(wcs.cd[1][1]) };
		int j;
		int bail = 0;
		for (j = 0; j < 8; j++) {
			sprintf(key, keys[j], n);
			*(vals[j]) = qfits_header_getdouble(hdr, key, dnil);
			if (*(vals[j]) == dnil) {
				bail = 1;
				break;
			}
		}
		if (bail)
			break;

		bl_append(job->verify_wcs, &wcs);
		n++;
	}

	// Default: solve first field.
	if (job->run && !il_size(job->fields)) {
		il_append(job->fields, 0);
		il_append(job->fields, 0);
	}

	return job;

 bailout:
	job_free(job);
	return NULL;
}

backend_t* backend_new()
{
	backend_t* backend = calloc(1, sizeof(backend_t));
	backend->indexinfos = bl_new(16, sizeof(indexinfo_t));
	backend->sizesmallest = HUGE_VAL;
	backend->sizebiggest = -HUGE_VAL;
	backend->blind = strdup(default_blind_command);

	// Default scale estimate: field width, in degrees:
	backend->minwidth = 0.1;
	backend->maxwidth = 180.0;
	return backend;
}

void backend_free(backend_t* backend)
{
	int i;
	if (backend->indexinfos) {
		for (i = 0; i < bl_size(backend->indexinfos); i++) {
			indexinfo_t* ii = bl_access(backend->indexinfos, i);
			free(ii->indexname);
		}
		bl_free(backend->indexinfos);
	}
	if (backend->blind)
		free(backend->blind);
}

int main(int argc, char** args)
{
	int c;
	char* configfn = "backend.cfg";
	char* inputfn = NULL;
	FILE* fconf;
	int i;
	backend_t* backend;
    bool cd = FALSE;

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, args, OPTIONS, long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			/* If this option set a flag, do nothing else now. */
			if (long_options[option_index].flag != 0)
				break;
			printf("option %s", long_options[option_index].name);
			if (optarg)
				printf(" with arg %s", optarg);
			printf("\n");
			break;
		case 'h':
			help_flag = 1;
			break;
		case 'c':
			configfn = optarg;
			break;
		case 'i':
			inputfn = optarg;
			break;
        case 'C':
            cd = TRUE;
            break;
		case '?':
			break;
		default:
			exit( -1);
		}
	}

	if (optind == argc) {
		// Need extra args: filename
		printf("You must specify a job file.");
		help_flag = 1;
	}
	if (help_flag) {
		print_help(args[0]);
		exit(0);
	}

	backend = backend_new();

	// Read config file.
	fconf = fopen(configfn, "r");
	if (!fconf) {
		printf("Failed to open config file \"%s\": %s.\n", configfn, strerror(errno));
		exit( -1);
	}
	if (parse_config_file(fconf, backend)) {
		printf("Failed to parse config file.\n");
		exit( -1);
	}
	fclose(fconf);

	if (!pl_size(backend->indexinfos)) {
		printf("You must list at least one index in the config file (%s)\n", configfn);
		exit( -1);
	}

	if (backend->minwidth <= 0.0 || backend->maxwidth <= 0.0) {
		fprintf(stderr, "\"minwidth\" and \"maxwidth\" must be positive!\n");
		exit( -1);
	}

	for (i = optind; i < argc; i++) {
		char* jobfn;
		qfits_header* hdr;
		job_t* job = NULL;
        char* dir;

		jobfn = args[i];

		// Read primary header.
		hdr = qfits_header_read(jobfn);
		if (!hdr) {
			printf("Failed to parse FITS header from file \"%s\".\n", jobfn);
			exit( -1);
		}
		job = parse_job_from_qfits_header(hdr);
		job->fieldfile = jobfn;

		// If the job has no scale estimate, search everything provided
		// by the backend
		if (!dl_size(job->scales) || job->include_default_scales) {
			double arcsecperpix;
			arcsecperpix = deg2arcsec(backend->minwidth) / job->imagew;
			dl_append(job->scales, arcsecperpix);
			arcsecperpix = deg2arcsec(backend->maxwidth) / job->imagew;
			dl_append(job->scales, arcsecperpix);
		}
		qfits_header_destroy(hdr);

		printf("Running job:\n");
		job_print(job);
		printf("\n");
		printf("Input file for blind:\n\n");
		job_write_blind_input(job, stdout, backend);

		if (inputfn) {
			FILE* f = fopen(inputfn, "a");
			if (!f) {
				fprintf(stderr, "Failed to open file \"%s\" to save the input sent to blind: %s.\n",
				        inputfn, strerror(errno));
				exit( -1);
			}
			if (job_write_blind_input(job, f, backend) ||
			        fclose(f)) {
				fprintf(stderr, "Failed to save the blind input file to \"%s\": %s.\n", inputfn, strerror(errno));
				exit( -1);
			}
		}

        if (cd) {
            char* cpy;
            cpy = strdup(jobfn);
            dir = strdup(dirname(cpy));
            free(cpy);
            
        } else {
            dir = NULL;
        }
		if (run_blind(job, backend, dir)) {
			fprintf(stderr, "Failed to run_blind.\n");
		}
        free(dir);

		//cleanup:
		job_free(job);
	}

	backend_free(backend);

	exit(0);
}
