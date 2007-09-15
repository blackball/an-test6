/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>

#include "tweak.h"
#include "sip_qfits.h"

#include "blind.h"

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "quadfile.h"
#include "idfile.h"
#include "solvedclient.h"
#include "solvedfile.h"
#include "starkd.h"
#include "codekd.h"
#include "boilerplate.h"
#include "fitsioutils.h"
#include "blind_wcs.h"
#include "verify.h"
#include "index.h"
#include "log.h"
#include "tic.h"

static bool record_match_callback(MatchObj* mo, void* userdata);
static time_t timer_callback(void* user_data);
static void add_blind_params(blind_t* bp, qfits_header* hdr);
static void get_fields_from_solvedserver(blind_t* bp, solver_t* sp);
static void load_and_parse_wcsfiles(blind_t* bp);
static void solve_fields(blind_t* bp, tan_t* verify_wcs);
static void remove_invalid_fields(il* fieldlist, int maxfield);
static bool is_field_solved(blind_t* bp, int fieldnum);
static int write_solutions(blind_t* bp);
static void solved_field(blind_t* bp, int fieldnum);
static int compare_matchobjs(const void* v1, const void* v2);
static void search_indexrdls(blind_t* bp, MatchObj* mo);
static void remove_duplicate_solutions(blind_t* bp);
static void free_matchobj(MatchObj* mo);

static int get_cpu_usage(blind_t* bp) {
	struct rusage r;
	int sofar;

	if (getrusage(RUSAGE_SELF, &r)) {
		logerr("Failed to get resource usage: %s\n", strerror(errno));
		return -1;
	}
	sofar = ceil((float)(r.ru_utime.tv_sec + r.ru_stime.tv_sec) +
				 (float)(1e-6 * (r.ru_utime.tv_usec + r.ru_stime.tv_usec)));
	return sofar;
}

static void check_time_limits(blind_t* bp) {
	if (bp->total_timelimit || bp->timelimit) {
		time_t now = time(NULL);
		if (bp->total_timelimit && (now - bp->time_total_start > bp->total_timelimit)) {
			logmsg("Total wall-clock time limit reached!\n");
			bp->hit_total_timelimit = TRUE;
		}
		if (bp->timelimit && (now - bp->time_start > bp->timelimit)) {
			logmsg("Wall-clock time limit reached!\n");
			bp->hit_timelimit = TRUE;
		}
	}
	if (bp->total_cpulimit || bp->cpulimit) {
		int now = get_cpu_usage(bp);
		if (bp->total_cpulimit && (now - bp->cpu_total_start > bp->total_cpulimit)) {
			logmsg("Total CPU time limit reached!\n");
			bp->hit_total_cpulimit = TRUE;
		}
		if (bp->cpulimit && (now - bp->cpu_start > bp->cpulimit)) {
			logmsg("CPU time limit reached!\n");
			bp->hit_cpulimit = TRUE;
		}
	}
	if (bp->hit_total_timelimit ||
		bp->hit_total_cpulimit ||
		bp->hit_timelimit ||
		bp->hit_cpulimit)
		bp->solver.quit_now = TRUE;
}

void blind_run(blind_t* bp) {
	solver_t* sp = &(bp->solver);
	int i, I;
    int index_options = 0;

	// Write the start file.
	if (bp->startfname) {
		FILE* fstart = NULL;
		logmsg("Writing marker file %s...\n", bp->startfname);
		fstart = fopen(bp->startfname, "wb");
		if (fstart)
			fclose(fstart);
		else
			logerr("Failed to write marker file %s: %s\n", bp->startfname, strerror(errno));
	}

	// Record current time for total wall-clock time limit.
	bp->time_total_start = time(NULL);

	// Record current CPU usage for total cpu-usage limit.
	bp->cpu_total_start = get_cpu_usage(bp);

	get_fields_from_solvedserver(bp, sp);

	// Parse WCS files submitted for verification.
	load_and_parse_wcsfiles(bp);

	// Read .xyls file...
	logmsg("Reading fields file %s...", bp->fieldfname);
	bp->xyls = xylist_open(bp->fieldfname);
	if (!bp->xyls) {
		logerr("Failed to read xylist.\n");
		exit( -1);
	}
	bp->xyls->xname = bp->xcolname;
	bp->xyls->yname = bp->ycolname;
	logmsg("got %u fields.\n", xylist_n_fields(bp->xyls));

    remove_invalid_fields(bp->fieldlist, xylist_n_fields(bp->xyls));

    if (bp->use_idfile)
        index_options |= INDEX_USE_IDFILE;

	// Verify any WCS estimates we have.
	if (pl_size(bp->verify_wcs_list)) {
        int i;

        // We want to get the best logodds out of all the indices, so we set the
        // logodds-to-solve impossibly high so that a "good enough" solution doesn't
        // stop us from continuing to search...
        double oldodds = bp->logratio_tosolve;
        bp->logratio_tosolve = HUGE_VAL;

		for (I = 0; I < sl_size(bp->indexnames); I++) {
			char* fname;
			int w;
			index_t* index;
			if (bp->single_field_solved)
				break;
			if (bp->cancelled)
				break;
			fname = sl_get(bp->indexnames, I);
			index = index_load(fname, index_options);
			if (!index) 
				exit( -1);
			pl_append(sp->indexes, index);
			sp->index_num = I;
			sp->index = index;
			logmsg("Verifying WCS with index %i of %i\n",  I + 1, sl_size(bp->indexnames));
			for (w = 0; w < bl_size(bp->verify_wcs_list); w++) {
				tan_t* wcs = bl_access(bp->verify_wcs_list, w);
				// Do it!
				solve_fields(bp, wcs);
			}
			// Clean up this index...
			index_close(index);
			pl_remove_all(sp->indexes);
			sp->index = NULL;
		}

        bp->logratio_tosolve = oldodds;

		logmsg("Got %i solutions.\n", bl_size(bp->solutions));

		if (bp->best_hit_only)
			remove_duplicate_solutions(bp);

        for (i=0; i<bl_size(bp->solutions); i++) {
            MatchObj* mo = bl_access(bp->solutions, i);
            if (mo->logodds >= bp->logratio_tosolve)
                solved_field(bp, mo->fieldnum);
        }
	}

	verify_init();

	// Start solving...
	if (bp->indexes_inparallel) {

		// Read all the indices...
		// FIXME avoid re-reading indices!!!
		for (I = 0; I < sl_size(bp->indexnames); I++) {
			char* fname;
			index_t* index;

			fname = sl_get(bp->indexnames, I);
			logmsg("\nLoading index %s...\n", fname);
			index = index_load(fname, index_options);
			if (!index) {
				logmsg("\nError loading index %s...\n", fname);
				exit( -1);
			}
			pl_append(sp->indexes, index);
		}

		// Record current CPU usage.
		bp->cpu_start = get_cpu_usage(bp);
		// Record current wall-clock time.
		bp->time_start = time(NULL);

		// Do it!
		solve_fields(bp, NULL);

		// Clean up the indices...
		for (I = 0; I < bl_size(sp->indexes); I++) {
			index_t* index;
			index = pl_get(sp->indexes, I);
			index_close(index);
		}
		bl_remove_all(sp->indexes);
		sp->index = NULL;

	} else {

		for (I = 0; I < sl_size(bp->indexnames); I++) {
			char* fname;
			index_t* index;

			if (bp->hit_total_timelimit || bp->hit_total_cpulimit)
				break;
			if (bp->single_field_solved)
				break;
			if (bp->cancelled)
				break;

			// Load the index...
			fname = sl_get(bp->indexnames, I);
			index = index_load(fname, index_options);
			if (!index) {
				exit( -1);
			}
			pl_append(sp->indexes, index);
			logmsg("\n\nTrying index %s...\n", fname);

			// Record current CPU usage.
			bp->cpu_start = get_cpu_usage(bp);
			// Record current wall-clock time.
			bp->time_start = time(NULL);

			// Do it!
			solve_fields(bp, NULL);

			// Clean up this index...
			index_close(index);
			pl_remove_all(sp->indexes);
			sp->index = NULL;
		}
	}

	verify_cleanup();

	// Clean up.
    // verified:
	xylist_close(bp->xyls);

	if (bp->solvedserver)
		solvedclient_set_server(NULL);

    if (write_solutions(bp))
        exit(-1);

	for (i=0; i<bl_size(bp->solutions); i++) {
		MatchObj* mo = bl_access(bp->solutions, i);
		free_matchobj(mo);
	}
	bl_remove_all(bp->solutions);

	if (bp->donefname) {
		FILE* fdone = NULL;
		logmsg("Writing marker file %s...\n", bp->donefname);
		fdone = fopen(bp->donefname, "wb");
		if (fdone)
			fclose(fdone);
		else
			logerr("Failed to write marker file %s: %s\n", bp->donefname, strerror(errno));
	}

	if (bp->donescript) {
		int rtn = system(bp->donescript);
		if (rtn == -1) {
			logerr("Donescript failed.\n");
		} else {
			logmsg("Donescript returned %i.\n", rtn);
		}
	}
}

void blind_setup_logging(blind_t* bp) {
	if (!bp->logfname)
		return;
	bp->logfd = fopen(bp->logfname, "a");
	if (!bp->logfd) {
		logerr("Failed to open log file %s: %s\n", bp->logfname, strerror(errno));
		goto bailout;
	}
	// Save old stdout/stderr...
	bp->dup_stdout = dup(STDOUT_FILENO);
	if (bp->dup_stdout == -1) {
		logerr("Failed to dup stdout: %s\n", strerror(errno));
		goto bailout;
	}
	bp->dup_stderr = dup(STDERR_FILENO);
	if (bp->dup_stderr == -1) {
		logerr("Failed to dup stderr: %s\n", strerror(errno));
		goto bailout;
	}
	// Replace stdout/stderr with logfile...
	if (dup2(fileno(bp->logfd), STDERR_FILENO) == -1) {
		logerr("Failed to dup2 log file: %s\n", strerror(errno));
		goto bailout;
	}
	if (dup2(fileno(bp->logfd), STDOUT_FILENO) == -1) {
		logerr("Failed to dup2 log file: %s\n", strerror(errno));
		goto bailout;
	}
	return;

 bailout:
	if (bp->logfd)
		fclose(bp->logfd);
	free(bp->logfname);
	bp->logfname = NULL;
}

void blind_restore_logging(blind_t* bp) {
	// Put stdout and stderr back to the way they were!
	if (bp->logfname) {
		if (dup2(bp->dup_stdout, fileno(stdout)) == -1) {
			logerr("Failed to dup2() back to stdout.\n");
		}
		if (dup2(bp->dup_stderr, fileno(stderr)) == -1) {
			logerr("Failed to dup2() back to stderr.\n");
		}
		if (close(bp->dup_stdout) || close(bp->dup_stderr)) {
			logerr("Failed to close duplicate stdout/stderr: %s\n", strerror(errno));
		}
		if (fclose(bp->logfd)) {
			logerr("Failed to close log file: %s\n",
				   strerror(errno));
		}
	}
}

void blind_init(blind_t* bp) {
	solver_t* sp = &(bp->solver);
	// Reset params.
	memset(bp, 0, sizeof(blind_t));

	bp->fieldlist = il_new(256);
    bp->solutions = bl_new(16, sizeof(MatchObj));
	bp->indexnames = sl_new(16);
	bp->verify_wcs_list = bl_new(1, sizeof(tan_t));
	bp->verify_wcsfiles = sl_new(1);
	bp->fieldid_key = strdup("FIELDID");
	bp->xcolname = strdup("X");
	bp->ycolname = strdup("Y");
	bp->firstfield = -1;
	bp->lastfield = -1;
	bp->tweak_aborder = DEFAULT_TWEAK_ABORDER;
	bp->tweak_abporder = DEFAULT_TWEAK_ABPORDER;
    bp->nsolves = 1;

	sp->userdata = bp;
	sp->record_match_callback = record_match_callback;
	sp->timer_callback = timer_callback;
}

int blind_parameters_are_sane(blind_t* bp, solver_t* sp) {
	if (sp->distractor_ratio == 0) {
		logerr("You must set a \"distractors\" proportion.\n");
		return 0;
	}
	if (!sl_size(bp->indexnames)) {
		logerr("You must specify an index.\n");
		return 0;
	}
	if (!bp->fieldfname) {
		logerr("You must specify a field filename (xylist).\n");
		return 0;
	}
	if (sp->codetol < 0.0) {
		logerr("You must specify codetol > 0\n");
		return 0;
	}
	if ((((sp->verify_pix > 0.0) ? 1 : 0) +
		 ((bp->verify_dist2 > 0.0) ? 1 : 0)) != 1) {
		logerr("You must specify either verify_pix or verify_dist2.\n");
		return 0;
	}
	if (bp->verify_dist2 > 0.0) {
		logerr("verify_dist2 mode is broken; email mierle@gmail.com to complain.\n");
		return 0;
	}
	if ((sp->funits_lower != 0.0) && (sp->funits_upper != 0.0) &&
		(sp->funits_lower > sp->funits_upper)) {
		logerr("fieldunits_lower MUST be less than fieldunits_upper.\n");
		logerr("\n(in other words, the lower-bound of scale estimate must "
		       "be less than the upper-bound!)\n\n");
		return 0;
	}
	return 1;
}

int blind_is_run_obsolete(blind_t* bp, solver_t* sp) {
	// If we're just solving one field, check to see if it's already
	// solved before doing a bunch of work and spewing tons of output.
	if ((il_size(bp->fieldlist) == 1) && bp->solved_in) {
        if (is_field_solved(bp, il_get(bp->fieldlist, 0)))
            return 1;
    }
	// Early check to see if this job was cancelled.
	if (bp->cancelfname) {
		if (file_exists(bp->cancelfname)) {
			logerr("Run cancelled.\n");
			return 1;
		}
	}

	return 0;
}

static void get_fields_from_solvedserver(blind_t* bp, solver_t* sp) {
	if (!bp->solvedserver)
        return;
    if (solvedclient_set_server(bp->solvedserver)) {
        logerr("Error setting solvedserver.\n");
        exit( -1);
    }

    if ((il_size(bp->fieldlist) == 0) && (bp->firstfield != -1) && (bp->lastfield != -1)) {
        int j;
        il_free(bp->fieldlist);
        logmsg("Contacting solvedserver to get field list...\n");
        bp->fieldlist = solvedclient_get_fields(bp->fieldid, bp->firstfield, bp->lastfield, 0);
        if (!bp->fieldlist) {
            logerr("Failed to get field list from solvedserver.\n");
            exit( -1);
        }
        logmsg("Got %i fields from solvedserver: ", il_size(bp->fieldlist));
        for (j = 0; j < il_size(bp->fieldlist); j++) {
            logmsg("%i ", il_get(bp->fieldlist, j));
        }
        logmsg("\n");
    }
}

static void load_and_parse_wcsfiles(blind_t* bp) {
	int i;
	for (i = 0; i < sl_size(bp->verify_wcsfiles); i++) {
		tan_t wcs;
		qfits_header* hdr;
		char* fn = sl_get(bp->verify_wcsfiles, i);
		logmsg("Reading WCS header to verify from file %s\n", fn);
		hdr = qfits_header_read(fn);
		if (!hdr) {
			logerr("Failed to read FITS header from file %s\n", fn);
			continue;
		}
		if (!tan_read_header(hdr, &wcs)) {
			logerr("Failed to parse WCS header from file %s\n", fn);
			qfits_header_destroy(hdr);
			continue;
		}
		pl_append(bp->verify_wcs_list, &wcs);
		qfits_header_destroy(hdr);
	}
}


void blind_log_run_parameters(blind_t* bp)
{
	solver_t* sp = &(bp->solver);
	int i;

	logmsg("fields ");
	for (i = 0; i < il_size(bp->fieldlist); i++)
		logmsg("%i ", il_get(bp->fieldlist, i));
	logmsg("\n");
	logmsg("indexes:\n");
	for (i = 0; i < sl_size(bp->indexnames); i++)
		logmsg("  %s\n", sl_get(bp->indexnames, i));
	logmsg("fieldfname %s\n", bp->fieldfname);
	for (i = 0; i < sl_size(bp->verify_wcsfiles); i++)
		logmsg("verify %s\n", sl_get(bp->verify_wcsfiles, i));
	logmsg("fieldid %i\n", bp->fieldid);
	logmsg("matchfname %s\n", bp->matchfname);
	logmsg("startfname %s\n", bp->startfname);
	logmsg("donefname %s\n", bp->donefname);
	logmsg("donescript %s\n", bp->donescript);
	logmsg("solved_in %s\n", bp->solved_in);
	logmsg("solved_out %s\n", bp->solved_out);
	logmsg("solvedserver %s\n", bp->solvedserver);
	logmsg("cancel %s\n", bp->cancelfname);
	logmsg("wcs %s\n", bp->wcs_template);
	logmsg("fieldid_key %s\n", bp->fieldid_key);
	logmsg("parity %i\n", sp->parity);
	logmsg("codetol %g\n", sp->codetol);
	logmsg("startdepth %i\n", sp->startobj);
	logmsg("enddepth %i\n", sp->endobj);
	logmsg("fieldunits_lower %g\n", sp->funits_lower);
	logmsg("fieldunits_upper %g\n", sp->funits_upper);
	logmsg("verify_dist %g\n", distsq2arcsec(bp->verify_dist2));
	logmsg("verify_pix %g\n", sp->verify_pix);
	logmsg("xcolname %s\n", bp->xcolname);
	logmsg("ycolname %s\n", bp->ycolname);
	logmsg("maxquads %i\n", sp->maxquads);
	logmsg("maxmatches %i\n", sp->maxmatches);
	logmsg("quiet %i\n", bp->quiet);
	logmsg("verbose %i\n", bp->verbose);
	logmsg("logfname %s\n", bp->logfname);
	logmsg("cpulimit %i\n", bp->cpulimit);
	logmsg("timelimit %i\n", bp->timelimit);
	logmsg("total_timelimit %i\n", bp->total_timelimit);
	logmsg("total_cpulimit %i\n", bp->total_cpulimit);
	logmsg("tweak %s\n", bp->do_tweak ? "on" : "off");
	if (bp->do_tweak) {
		logmsg("tweak_aborder %i\n", bp->tweak_aborder);
		logmsg("tweak_abporder %i\n", bp->tweak_abporder);
	}
}

void blind_cleanup(blind_t* bp) {
	il_free(bp->fieldlist);
    bl_free(bp->solutions);
	sl_free2(bp->indexnames);
	sl_free2(bp->verify_wcsfiles);
	bl_free(bp->verify_wcs_list);

	free(bp->cancelfname);
	free(bp->donefname);
	free(bp->donescript);
	free(bp->fieldfname);
	free(bp->fieldid_key);
	free(bp->indexrdlsfname);
	free(bp->logfname);
	free(bp->matchfname);
	free(bp->solvedserver);
	free(bp->solved_in);
	free(bp->solved_out);
	free(bp->startfname);
	free(bp->wcs_template);
	free(bp->xcolname);
	free(bp->ycolname);
}

static sip_t* tweak(blind_t* bp, MatchObj* mo, startree* starkd) {
	solver_t* sp = &(bp->solver);
	tweak_t* twee = NULL;
	double *imgx = NULL, *imgy = NULL;
	int i;
	double* starxyz;
	int nstars;
	kdtree_qres_t* res = NULL;
	double fieldcenter[3];
	double fieldr2;
	sip_t* sip = NULL;

	logmsg("Tweaking!\n");

	twee = tweak_new();
    if (bp->verbose)
        twee->verbose = TRUE;
    if (bp->quiet)
        twee->quiet = TRUE;

	if (bp->verify_dist2 > 0.0)
		twee->jitter = distsq2arcsec(bp->verify_dist2);
	else {
		twee->jitter = hypot(mo->scale * sp->verify_pix, sp->index->index_jitter);
		logmsg("Star jitter: %g arcsec.\n", twee->jitter);
	}
	// Set tweak's jitter to 6 sigmas.
	//twee->jitter *= 6.0;
	logmsg("Setting tweak jitter: %g arcsec.\n", twee->jitter);

	// pull out the field coordinates into separate X and Y arrays.
	imgx = malloc(sp->nfield * sizeof(double));
	imgy = malloc(sp->nfield * sizeof(double));
	for (i = 0; i < sp->nfield; i++) {
		imgx[i] = sp->field[i * 2 + 0];
		imgy[i] = sp->field[i * 2 + 1];
	}
	logmsg("Pushing %i image coordinates.\n", sp->nfield);
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
	logmsg("Pushing %i star coordinates.\n", nstars);
	tweak_push_ref_xyz(twee, starxyz, nstars);

	tweak_push_wcs_tan(twee, &(mo->wcstan));
	twee->sip->a_order = twee->sip->b_order = bp->tweak_aborder;
	twee->sip->ap_order = twee->sip->bp_order = bp->tweak_abporder;

	if (bp->tweak_skipshift) {
		logmsg("Skipping shift operation.\n");
		tweak_skip_shift(twee);
	}

	logmsg("Begin tweaking (to order %i)...\n", bp->tweak_aborder);
	/*
	  while (!(twee->state & TWEAK_HAS_LINEAR_CD)) {
	  unsigned int r = tweak_advance_to(twee, TWEAK_HAS_LINEAR_CD);
	  if (r == -1) {
	  logerr("Tweak error!\n");
	  goto bailout;
	  }
	  }
	*/

	twee->weighted_fit = 1;

	{
		int order;
		int k;
		for (order = 1; order <= MAX(1, bp->tweak_aborder); order++) {
			logmsg("\n");
			logmsg("--------------------------------\n");
			logmsg("Order %i\n", order);
			logmsg("--------------------------------\n");

			twee->sip->a_order = twee->sip->b_order = order;
			twee->sip->ap_order = twee->sip->bp_order = order;
			tweak_go_to(twee, TWEAK_HAS_CORRESPONDENCES);

			for (k = 0; k < 5; k++) {
				logmsg("\n");
				logmsg("--------------------------------\n");
				logmsg("Iterating tweak: order %i, step %i\n", order, k);
				twee->state &= ~TWEAK_HAS_LINEAR_CD;
				tweak_go_to(twee, TWEAK_HAS_LINEAR_CD);
				tweak_clear_correspondences(twee);
			}
		}
	}
	fflush(stdout);
	fflush(stderr);


	logmsg("Done tweaking!\n");

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

static void print_match(blind_t* bp, MatchObj* mo)
{
	int Nmin = MIN(mo->nindex, mo->nfield);
	int ndropout = Nmin - mo->noverlap - mo->nconflict;
	logmsg("  logodds ratio %g (%g), %i match, %i conflict, %i dropout, %i index.\n",
	       mo->logodds, exp(mo->logodds), mo->noverlap, mo->nconflict, ndropout, mo->nindex);
}

static bool record_match_callback(MatchObj* mo, void* userdata) {
	blind_t* bp = userdata;
	solver_t* sp = &(bp->solver);

	check_time_limits(bp);

	if (mo->logodds >= bp->logratio_toprint)
        print_match(bp, mo);

	if (mo->logodds < bp->logratio_tokeep)
		return FALSE;

    logmsg("Pixel scale: %g arcsec/pix.\n", mo->scale);

	assert(sp->index);
	assert(sp->index->indexname);
	mo->indexname = sp->index->indexname;
    bl_insert_sorted(bp->solutions, mo, compare_matchobjs);

	if (mo->logodds < bp->logratio_tosolve)
		return FALSE;

    bp->nsolves_sofar++;
    if (bp->nsolves_sofar >= bp->nsolves) {
        if (bp->solver.index) {
            char* copy;
            char* base;
            copy = strdup(bp->solver.index->indexname);
            base = strdup(basename(copy));
            free(copy);
            logerr("Field %i: solved with index %s.\n", mo->fieldnum, base);
            free(base);
        } else {
            logerr("Field %i: solved with index %i", mo->fieldnum, mo->indexid);
            if (mo->healpix >= 0)
                logerr(", healpix %i\n", mo->healpix);
            else
                logerr("\n");
        }
        return TRUE;
    } else {
        logmsg("Found a quad that solves the image; that makes %i of %i required.\n",
               bp->nsolves_sofar, bp->nsolves);
    }
	return FALSE;
}

static time_t timer_callback(void* user_data) {
	blind_t* bp = user_data;

	check_time_limits(bp);

	// check if the field has already been solved...
    if (is_field_solved(bp, bp->fieldnum))
        return 0;
	if (bp->cancelfname && file_exists(bp->cancelfname)) {
		bp->cancelled = TRUE;
		logmsg("File \"%s\" exists: cancelling.\n", bp->cancelfname);
		return 0;
	}
	return 1; // wait 1 second... FIXME config?
}

static void add_blind_params(blind_t* bp, qfits_header* hdr) {
	solver_t* sp = &(bp->solver);
	int i;
	fits_add_long_comment(hdr, "-- blind solver parameters: --");
	if (sp->index) {
		fits_add_long_comment(hdr, "Index name: %s", sp->index->indexname);
		fits_add_long_comment(hdr, "Index id: %i", sp->index->indexid);
		fits_add_long_comment(hdr, "Index healpix: %i", sp->index->healpix);
		fits_add_long_comment(hdr, "Index scale lower: %g arcsec", sp->index->index_scale_lower);
		fits_add_long_comment(hdr, "Index scale upper: %g arcsec", sp->index->index_scale_upper);
		fits_add_long_comment(hdr, "Index jitter: %g", sp->index->index_jitter);
		fits_add_long_comment(hdr, "Circle: %s", sp->index->circle ? "yes" : "no");
		fits_add_long_comment(hdr, "Cxdx margin: %g", sp->cxdx_margin);
	}
	for (i = 0; i < sl_size(bp->indexnames); i++)
		fits_add_long_comment(hdr, "Index(%i): %s", i, sl_get(bp->indexnames, i));

	fits_add_long_comment(hdr, "Field name: %s", bp->fieldfname);
	fits_add_long_comment(hdr, "Field scale lower: %g arcsec/pixel", sp->funits_lower);
	fits_add_long_comment(hdr, "Field scale upper: %g arcsec/pixel", sp->funits_upper);
	fits_add_long_comment(hdr, "X col name: %s", bp->xcolname);
	fits_add_long_comment(hdr, "Y col name: %s", bp->ycolname);
	fits_add_long_comment(hdr, "Start obj: %i", sp->startobj);
	fits_add_long_comment(hdr, "End obj: %i", sp->endobj);

	fits_add_long_comment(hdr, "Solved_in: %s", bp->solved_in);
	fits_add_long_comment(hdr, "Solved_out: %s", bp->solved_out);
	fits_add_long_comment(hdr, "Solvedserver: %s", bp->solvedserver);

	fits_add_long_comment(hdr, "Parity: %i", sp->parity);
	fits_add_long_comment(hdr, "Codetol: %g", sp->codetol);
	fits_add_long_comment(hdr, "Verify distance: %g arcsec", distsq2arcsec(bp->verify_dist2));
	fits_add_long_comment(hdr, "Verify pixels: %g pix", sp->verify_pix);

	fits_add_long_comment(hdr, "Maxquads: %i", sp->maxquads);
	fits_add_long_comment(hdr, "Maxmatches: %i", sp->maxmatches);
	fits_add_long_comment(hdr, "Cpu limit: %i s", bp->cpulimit);
	fits_add_long_comment(hdr, "Time limit: %i s", bp->timelimit);
	fits_add_long_comment(hdr, "Total time limit: %i s", bp->total_timelimit);
	fits_add_long_comment(hdr, "Total CPU limit: %i s", bp->total_cpulimit);

	fits_add_long_comment(hdr, "Tweak: %s", (bp->do_tweak ? "yes" : "no"));
	if (bp->do_tweak) {
		fits_add_long_comment(hdr, "Tweak AB order: %i", bp->tweak_aborder);
		fits_add_long_comment(hdr, "Tweak ABP order: %i", bp->tweak_abporder);
	}
	fits_add_long_comment(hdr, "--");
}

static void remove_invalid_fields(il* fieldlist, int maxfield) {
    int i;
    for (i=0; i<il_size(fieldlist); i++) {
        int fieldnum = il_get(fieldlist, i);
        if (fieldnum >= 1 && fieldnum <= maxfield)
            continue;
        if (fieldnum > maxfield) {
			logerr("Field %i does not exist (max=%i).\n", fieldnum, maxfield);
        }
        if (fieldnum < 1) {
            logerr("Field %i is invalid (must be >= 1).\n", fieldnum);
        }
        il_remove(fieldlist, i);
        i--;
    }
}

static void solve_fields(blind_t* bp, tan_t* verify_wcs) {
	solver_t* sp = &(bp->solver);
	double last_utime, last_stime;
	double utime, stime;
	struct timeval wtime, last_wtime;
	int nfields;
	int fi;

	get_resource_stats(&last_utime, &last_stime, NULL);
	gettimeofday(&last_wtime, NULL);

	nfields = xylist_n_fields(bp->xyls);
	sp->field = NULL;

	for (fi = 0; fi < il_size(bp->fieldlist); fi++) {
		int fieldnum;
		MatchObj template ;
		qfits_header* fieldhdr = NULL;

		fieldnum = il_get(bp->fieldlist, fi);

		memset(&template, 0, sizeof(MatchObj));
		template.fieldnum = fieldnum;
		template.fieldfile = bp->fieldid;

		// Get the FIELDID string from the xyls FITS header.
		fieldhdr = xylist_get_field_header(bp->xyls, fieldnum);
		if (fieldhdr) {
			char* idstr = qfits_pretty_string(qfits_header_getstr(fieldhdr, bp->fieldid_key));
			if (idstr)
				strncpy(template.fieldname, idstr, sizeof(template.fieldname) - 1);
			qfits_header_destroy(fieldhdr);
		}

		// Has the field already been solved?
        if (is_field_solved(bp, fieldnum))
            goto cleanup;

		// Get the field.
		sp->nfield = xylist_n_entries(bp->xyls, fieldnum);
		if (sp->nfield == -1) {
			logerr("Couldn't determine how many objects are in field %i.\n", fieldnum);
			goto cleanup;
		}
		sp->field = realloc(sp->field, 2 * sp->nfield * sizeof(double));
		if (xylist_read_entries(bp->xyls, fieldnum, 0, sp->nfield, sp->field)) {
			logerr("Failed to read field.\n");
			goto cleanup;
		}

		sp->numtries = 0;
		sp->nummatches = 0;
		sp->numscaleok = 0;
		sp->num_cxdx_skipped = 0;
		sp->num_verified = 0;
		sp->quit_now = FALSE;
		sp->mo_template = &template ;

		solver_reset_best_match(sp);

		bp->fieldnum = fieldnum;
        bp->nsolves_sofar = 0;

		sp->logratio_record_threshold = MIN(bp->logratio_tokeep, bp->logratio_toprint);
		sp->record_match_callback = record_match_callback;
		sp->userdata = bp;

		if (bp->field_uniform_NX && bp->field_uniform_NY) {
			solver_uniformize_field(sp, bp->field_uniform_NX, bp->field_uniform_NY);
		}

		solver_preprocess_field(sp);

		if (verify_wcs) {
			// fabricate a match...
			MatchObj mo;
			memcpy(&mo, &template, sizeof(MatchObj));
			memcpy(&(mo.wcstan), verify_wcs, sizeof(tan_t));
			mo.wcs_valid = TRUE;
			mo.scale = tan_pixel_scale(verify_wcs);
			solver_transform_corners(sp, &mo);

			sp->distance_from_quad_bonus = FALSE;

			logmsg("Verifying WCS of field %i.\n", fieldnum);

			solver_inject_match(sp, &mo);

			// print it, if it hasn't been already.
			if (mo.logodds < bp->logratio_toprint)
				print_match(bp, &mo);

		} else {
			logmsg("Solving field %i.\n", fieldnum);

			sp->distance_from_quad_bonus = TRUE;
			
			// The real thing
			solver_run(sp);

			logmsg("Field %i: tried %i quads, matched %i codes.\n",
			       fieldnum, sp->numtries, sp->nummatches);

			if (sp->maxquads && sp->numtries >= sp->maxquads) {
				logmsg("  exceeded the number of quads to try: %i >= %i.\n",
				       sp->numtries, sp->maxquads);
			}
			if (sp->maxmatches && sp->nummatches >= sp->maxmatches) {
				logmsg("  exceeded the number of quads to match: %i >= %i.\n",
				       sp->nummatches, sp->maxmatches);
			}
			if (bp->cancelled) {
				logmsg("  cancelled at user request.\n");
			}
		}


        if (sp->best_match_solves) {
            solved_field(bp, fieldnum);
		} else if (!verify_wcs) {
			// Field unsolved.
            logerr("Field %i did not solve", fieldnum);
            if (bp->solver.index && bp->solver.index->indexname) {
                char* copy;
                char* base;
                copy = strdup(bp->solver.index->indexname);
                base = strdup(basename(copy));
                free(copy);
                logerr(" (index %s", base);
                free(base);
                if (bp->solver.endobj)
                    logerr(", field objects %i-%i", bp->solver.startobj+1, bp->solver.endobj);
                logerr(")");
            }
            logerr(".\n");
			if (sp->have_best_match) {
				logmsg("Best match encountered: ");
				print_match(bp, &(sp->best_match));
			} else {
				logmsg("Best odds encountered: %g\n", exp(sp->best_logodds));
			}
		}

		solver_free_field(sp);

		get_resource_stats(&utime, &stime, NULL);
		gettimeofday(&wtime, NULL);
		logmsg("Spent %g s user, %g s system, %g s total, %g s wall time.\n",
		       (utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime),
		       millis_between(&last_wtime, &wtime) * 0.001);

		last_utime = utime;
		last_stime = stime;
		last_wtime = wtime;

	cleanup:
		logmsg("\n");
	}

	free(sp->field);
	sp->field = NULL;
}

static bool is_field_solved(blind_t* bp, int fieldnum) {
    if (bp->solved_in) {
        logverb("Checking %s field %i to see if the field is solved: %s.\n",
                bp->solved_in, fieldnum,
                (solvedfile_get(bp->solved_in, fieldnum) ? "yes" : "no"));
    }
    if (bp->solved_in &&
        (solvedfile_get(bp->solved_in, fieldnum) == 1)) {
        // file exists; field has already been solved.
        logmsg("Field %i: solvedfile %s: field has been solved.\n", fieldnum, bp->solved_in);
        return TRUE;
    }
    if (bp->solvedserver &&
        (solvedclient_get(bp->fieldid, fieldnum) == 1)) {
        // field has already been solved.
        logmsg("Field %i: field has already been solved.\n", fieldnum);
        return TRUE;
    }
    return FALSE;
}

static void solved_field(blind_t* bp, int fieldnum) {
    // Record in solved file, or send to solved server.
    if (bp->solved_out) {
        logmsg("Field %i solved: writing to file %s to indicate this.\n", fieldnum, bp->solved_out);
        if (solvedfile_set(bp->solved_out, fieldnum)) {
            logerr("Failed to write to solvedfile %s.\n", bp->solved_out);
        }
    }
    if (bp->solvedserver) {
        solvedclient_set(bp->fieldid, fieldnum);
    }
    // If we're just solving a single field, and we solved it...
    if (il_size(bp->fieldlist) == 1)
        bp->single_field_solved = TRUE;
}

static void search_indexrdls(blind_t* bp, MatchObj* mo) {
    kdtree_qres_t* res = NULL;
    double* starxyz;
    int nstars;
    double* radec;
    int i;
    double safetyfactor;
	solver_t* sp = &(bp->solver);

    // find all the index stars that are inside the circle that bounds
    // the field.
    safetyfactor = 1.05;
    if (bp->indexrdls_expand > 0.0)
        safetyfactor *= bp->indexrdls_expand;
    res = kdtree_rangesearch_options(sp->index->starkd->tree, mo->center,
                                     square(mo->radius) * safetyfactor,
                                     KD_OPTIONS_SMALL_RADIUS |
                                     KD_OPTIONS_RETURN_POINTS);
    if (!res || !res->nres) {
        logmsg("No index stars found!\n");
    }
    starxyz = res->results.d;
    nstars = res->nres;
    radec = malloc(nstars * 2 * sizeof(double));
    for (i = 0; i < nstars; i++)
        xyzarr2radecdegarr(starxyz + i*3, radec + i*2);
    kdtree_free_query(res);

    mo->indexrdls = radec;
    mo->nindexrdls = nstars;
}

static void free_matchobj(MatchObj* mo) {
    if (!mo) return;
    if (mo->sip) {
        sip_free(mo->sip);
        mo->sip = NULL;
    }
    if (mo->indexrdls) {
        free(mo->indexrdls);
        mo->indexrdls = NULL;
        mo->nindexrdls = 0;
    }
}

static void remove_duplicate_solutions(blind_t* bp) {
    int i, j;
    for (i=0; i<bl_size(bp->solutions); i++) {
        MatchObj* mo = bl_access(bp->solutions, i);
        j = i+1;
        while (j < bl_size(bp->solutions)) {
            MatchObj* mo2 = bl_access(bp->solutions, j);
            if (mo->fieldfile != mo2->fieldfile)
                break;
            if (mo->fieldnum != mo2->fieldnum)
                break;
            assert(mo2->logodds <= mo->logodds);
            free_matchobj(mo2);
            bl_remove_index(bp->solutions, j);
        }
    }
}

static int write_solutions(blind_t* bp) {
    int i;
	solver_t* sp = &(bp->solver);

	if (bp->do_tweak || bp->indexrdlsfname) {
		for (i=0; i<bl_size(bp->solutions); i++) {
			index_t* index;
			const char* fn;
			MatchObj* mo = bl_access(bp->solutions, i);
			//assert(mo->indexnum >= 0);
			//assert(mo->indexnum < sl_size(bp->indexnames));
			//fn = sl_get(bp->indexnames, mo->indexnum);
			fn = mo->indexname;
			assert(fn);
			index = index_load(fn, INDEX_ONLY_LOAD_SKDT);
			// Tweak, if requested.
			if (bp->do_tweak) {
				sp->index = index;
				mo->sip = tweak(bp, mo, index->starkd);
			}
			// Gather stars for index rdls, if requested.
			if (bp->indexrdlsfname)
				search_indexrdls(bp, mo);
			index_close(index);
		}
	}

	if (bp->matchfname) {
		bp->mf = matchfile_open_for_writing(bp->matchfname);
		if (!bp->mf) {
			logerr("Failed to open file %s to write match file.\n", bp->matchfname);
            return -1;
		}
		boilerplate_add_fits_headers(bp->mf->header);
		qfits_header_add(bp->mf->header, "HISTORY", "This file was created by the program \"blind\".", NULL, NULL);
		qfits_header_add(bp->mf->header, "DATE", qfits_get_datetime_iso8601(), "Date this file was created.", NULL);
		add_blind_params(bp, bp->mf->header);
		if (matchfile_write_header(bp->mf)) {
			logerr("Failed to write matchfile header.\n");
            return -1;
		}

        for (i=0; i<bl_size(bp->solutions); i++) {
            MatchObj* mo = bl_access(bp->solutions, i);

            if (matchfile_write_match(bp->mf, mo)) {
                logerr("Field %i: error writing a match.\n", mo->fieldnum);
                return -1;
            }
        }

		if (matchfile_fix_header(bp->mf) ||
			matchfile_close(bp->mf)) {
			logerr("Error closing matchfile.\n");
            return -1;
		}
        bp->mf = NULL;
	}

	if (bp->indexrdlsfname) {
		bp->indexrdls = rdlist_open_for_writing(bp->indexrdlsfname);
		if (!bp->indexrdls) {
			logerr("Failed to open index RDLS file %s for writing.\n",
				   bp->indexrdlsfname);
            return -1;
		}

        boilerplate_add_fits_headers(bp->indexrdls->header);
        fits_add_long_history(bp->indexrdls->header, "This \"indexrdls\" file was created by the program \"blind\"."
                              "  It contains the RA/DEC of index objects that were found inside a solved field.");
        qfits_header_add(bp->indexrdls->header, "DATE", qfits_get_datetime_iso8601(), "Date this file was created.", NULL);
        add_blind_params(bp, bp->indexrdls->header);
        if (rdlist_write_header(bp->indexrdls)) {
            logerr("Failed to write index RDLS header.\n");
            return -1;
        }

        for (i=0; i<bl_size(bp->solutions); i++) {
            MatchObj* mo = bl_access(bp->solutions, i);
            if (rdlist_new_field(bp->indexrdls)) {
                logerr("Failed to write index RDLS field header.\n");
                return -1;
            }
            if (strlen(mo->fieldname))
                qfits_header_add(bp->indexrdls->fieldheader, "FIELDID", mo->fieldname, "Name of this field", NULL);
            if (rdlist_write_field_header(bp->indexrdls)) {
                logerr("Failed to write index RDLS field header.\n");
                return -1;
            }
            assert(mo->indexrdls);

            if (rdlist_write_entries(bp->indexrdls, mo->indexrdls, mo->nindexrdls)) {
                logerr("Failed to write index RDLS entry.\n");
                return -1;
            }
            if (rdlist_fix_field(bp->indexrdls)) {
                logerr("Failed to fix index RDLS field header.\n");
                return -1;
            }
        }

		if (rdlist_fix_header(bp->indexrdls) ||
			rdlist_close(bp->indexrdls)) {
			logerr("Failed to close index RDLS file.\n");
            return -1;
		}
		bp->indexrdls = NULL;
	}

    if (bp->wcs_template) {
        // We want to write only the best WCS for each field.
        remove_duplicate_solutions(bp);

        for (i=0; i<bl_size(bp->solutions); i++) {
            char wcs_fn[1024];
            FILE* fout;
            qfits_header* hdr;
            char* tm;

            MatchObj* mo = bl_access(bp->solutions, i);
            snprintf(wcs_fn, sizeof(wcs_fn), bp->wcs_template, mo->fieldnum);
            fout = fopen(wcs_fn, "wb");
            if (!fout) {
                logerr("Failed to open WCS output file %s: %s\n", wcs_fn, strerror(errno));
                return -1;
            }
            assert(mo->wcs_valid);

            if (mo->sip)
                hdr = sip_create_header(mo->sip);
            else
                hdr = tan_create_header(&(mo->wcstan));

            boilerplate_add_fits_headers(hdr);
            qfits_header_add(hdr, "HISTORY", "This WCS header was created by the program \"blind\".", NULL, NULL);
            tm = qfits_get_datetime_iso8601();
            qfits_header_add(hdr, "DATE", tm, "Date this file was created.", NULL);
            add_blind_params(bp, hdr);
            fits_add_long_comment(hdr, "-- properties of the matching quad: --");
            fits_add_long_comment(hdr, "quadno: %i", mo->quadno);
            fits_add_long_comment(hdr, "stars: %i,%i,%i,%i", mo->star[0], mo->star[1], mo->star[2], mo->star[3]);
            fits_add_long_comment(hdr, "field: %i,%i,%i,%i", mo->field[0], mo->field[1], mo->field[2], mo->field[3]);
            fits_add_long_comment(hdr, "code error: %g", sqrt(mo->code_err));
            fits_add_long_comment(hdr, "noverlap: %i", mo->noverlap);
            fits_add_long_comment(hdr, "nconflict: %i", mo->nconflict);
            fits_add_long_comment(hdr, "nfield: %i", mo->nfield);
            fits_add_long_comment(hdr, "nindex: %i", mo->nindex);
            fits_add_long_comment(hdr, "scale: %g arcsec/pix", mo->scale);
            fits_add_long_comment(hdr, "parity: %i", (int)mo->parity);
            fits_add_long_comment(hdr, "quads tried: %i", mo->quads_tried);
            fits_add_long_comment(hdr, "quads matched: %i", mo->quads_matched);
            fits_add_long_comment(hdr, "quads verified: %i", mo->nverified);
            fits_add_long_comment(hdr, "objs tried: %i", mo->objs_tried);
            fits_add_long_comment(hdr, "cpu time: %g", mo->timeused);
            fits_add_long_comment(hdr, "--");

            if (strlen(mo->fieldname)) {
                qfits_header_add(hdr, bp->fieldid_key, mo->fieldname, "Field name (copied from input field)", NULL);
				if (qfits_header_dump(hdr, fout)) {
					logerr("Failed to write FITS WCS header.\n");
                    return -1;
				}
				fits_pad_file(fout);
				qfits_header_destroy(hdr);
				fclose(fout);
            }
        }
    }
    return 0;
}

static int compare_matchobjs(const void* v1, const void* v2) {
    int diff;
    float fdiff;
    const MatchObj* mo1 = v1;
    const MatchObj* mo2 = v2;
    diff = mo1->fieldfile - mo2->fieldfile;
    if (diff) return diff;
    diff = mo1->fieldnum - mo2->fieldnum;
    if (diff) return diff;
    fdiff = mo1->logodds - mo2->logodds;
    if (fdiff == 0.0)
        return 0;
    if (fdiff > 0.0)
        return -1;
    return 1;
}

