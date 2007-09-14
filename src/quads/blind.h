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

#ifndef BLIND_H
#define BLIND_H

#include "an-bool.h"
#include "solver.h"
#include "matchobj.h"
#include "matchfile.h"
#include "rdlist.h"
#include "bl.h"

struct blind_params {
	solver_t solver;

	bool indexes_inparallel;

	double logratio_toprint;
	double logratio_tokeep;
	double logratio_tosolve;

    // How many solving quads are required before we stop?
    int nsolves;
    int nsolves_sofar;

	// Filenames
	char *fieldfname;
	char *matchfname, *indexrdlsfname;
	char *startfname, *donefname, *donescript, *logfname;

	// WCS filename template (sprintf format with %i for field number)
	char* wcs_template;

	// List of WCS filenames to run verification on.
	sl* verify_wcsfiles;

	// WCS instances to verify.  (tan_t structs)
	bl* verify_wcs_list;

	// Output solved file.
	char *solved_out;
	// Input solved file.
	char* solved_in;
	// Solvedserver ip:port
	char *solvedserver;
	// If using solvedserver, limits of fields to ask for
	int firstfield, lastfield;

	// Indexes to use (base filenames)
	sl* indexnames;

	// Logfile
	FILE* logfd;
	int dup_stderr;
	int dup_stdout;
	bool silent;
	bool quiet;
	bool verbose;

	// Fields to try
	il* fieldlist;

	// Which field in a multi-HDU xyls file is this?
	int fieldnum;
	// A unique ID for the whole multi-HDU xyls file.
	int fieldid;

	// xylist column names.
	char *xcolname, *ycolname;
	// FITS keyword to copy from xylist to matchfile.
	char *fieldid_key;

	double verify_dist2;

	bool use_idfile;

	// The fields to solve!
	xylist* xyls;

	// How finely to uniformize the field.
	int field_uniform_NX;
	int field_uniform_NY;

	// Output files
	matchfile* mf;
	rdlist* indexrdls;
	bool indexrdls_solvedonly;
	double indexrdls_expand;

    // List of MatchObjs with logodds >= logodds_tokeep
    bl* solutions;

	int cpulimit;
	int cpu_start;
	bool hit_cpulimit;

	int timelimit;
	time_t time_start;
	bool hit_timelimit;

	int total_cpulimit;
	int cpu_total_start;
	bool hit_total_cpulimit;

	int total_timelimit;
	time_t time_total_start;
	bool hit_total_timelimit;

	bool single_field_solved;

	// filename for cancelling
	char* cancelfname;
	bool cancelled;

	bool do_tweak;
	int tweak_aborder;
	int tweak_abporder;
	bool tweak_skipshift;
};
typedef struct blind_params blind_t;


void blind_run(blind_t* bp);

void blind_init(blind_t* bp);

void blind_cleanup(blind_t* bp);

int blind_parameters_are_sane(blind_t* bp, solver_t* sp);

int blind_is_run_obsolete(blind_t* bp, solver_t* sp);

void blind_log_run_parameters(blind_t* bp);

void blind_setup_logging(blind_t* bp);

void blind_restore_logging(blind_t* bp);

#endif
