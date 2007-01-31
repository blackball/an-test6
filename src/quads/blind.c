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

static void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n", progname);
}

static void solve_fields();
static int read_parameters();

#define DEFAULT_CODE_TOL .01
#define DEFAULT_PARITY_FLIP FALSE

// params:
char *fieldfname, *treefname, *quadfname, *startreefname;
char *idfname, *matchfname, *donefname, *solvedfname, *solvedserver;
char* xcolname, *ycolname;
char* wcs_template;
char* fieldid_key;
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
double overlap_toprint;
double overlap_tokeep;
double overlap_tosolve;
int ninfield_tokeep;
int ninfield_tosolve;
int maxquads;
double cxdx_margin;

bool quiet;
bool silent;
bool verbose;

int firstfield, lastfield;
il* fieldlist;

matchfile* mf;
idfile* id;
quadfile* quads;
xylist* xyls;
startree* starkd;
codetree *codekd;
bool circle;
int nverified;

handlehits* hits;

int main(int argc, char *argv[]) {
    uint numfields;
	char* progname = argv[0];
	int i;

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
		fieldid_key = strdup("FIELDID");
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
		overlap_toprint = 0.0;
		overlap_tokeep = 0.0;
		overlap_tosolve = 0.0;
		ninfield_tosolve = 0;
		ninfield_tokeep = 0;
		cxdx_margin = 0.0;
		quiet = FALSE;
		verbose = FALSE;
		il_remove_all(fieldlist);
		quads = NULL;
		starkd = NULL;
		nverified = 0;

		if (read_parameters()) {
			free(xcolname);
			free(ycolname);
			free(fieldid_key);
			break;
		}

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
			fprintf(stderr, "fieldid_key %s\n", fieldid_key);
			fprintf(stderr, "parity %i\n", parity);
			fprintf(stderr, "codetol %g\n", codetol);
			fprintf(stderr, "startdepth %i\n", startdepth);
			fprintf(stderr, "enddepth %i\n", enddepth);
			fprintf(stderr, "fieldunits_lower %g\n", funits_lower);
			fprintf(stderr, "fieldunits_upper %g\n", funits_upper);
			fprintf(stderr, "agreetol %g\n", agreetol);
			fprintf(stderr, "verify_dist %g\n", distsq2arcsec(verify_dist2));
			fprintf(stderr, "nagree_toverify %i\n", nagree_toverify);
			fprintf(stderr, "overlap_toprint %f\n", overlap_toprint);
			fprintf(stderr, "overlap_tokeep %f\n", overlap_tokeep);
			fprintf(stderr, "overlap_tosolve %f\n", overlap_tosolve);
			fprintf(stderr, "ninfield_tokeep %i\n", ninfield_tokeep);
			fprintf(stderr, "ninfield_tosolve %i\n", ninfield_tosolve);
			fprintf(stderr, "xcolname %s\n", xcolname);
			fprintf(stderr, "ycolname %s\n", ycolname);
			fprintf(stderr, "maxquads %i\n", maxquads);
			fprintf(stderr, "quiet %i\n", quiet);
			fprintf(stderr, "verbose %i\n", verbose);
		}

		if (!treefname || !fieldfname || (codetol < 0.0) || !matchfname) {
			fprintf(stderr, "Invalid params... this message is useless.\n");
			exit(-1);
		}

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
			fprintf(stderr, "Index scale: [%g, %g] arcmin, [%g, %g] arcsec\n",
					index_scale_lower/60.0, index_scale/60.0, index_scale_lower, index_scale);

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

		// If the code kdtree has CXDX set, set cxdx_margin.
		if (qfits_header_getboolean(codekd->header, "CXDX", 0))
			// 1.5 = sqrt(2) + fudge factor.
			cxdx_margin = 1.5 * codetol;

		// check for CIRCLE field in ckdt header...
		circle = qfits_header_getboolean(codekd->header, "CIRCLE", 0);

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
			fprintf(stderr, "(Note, this won't cause trouble; you just won't get star IDs for matching quads.)\n");
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
		free(fieldid_key);
		free_fn(fieldfname);
		free_fn(treefname);
		free_fn(quadfname);
		free_fn(idfname);
		free_fn(startreefname);

		if (!silent)
			toc();
	}

	il_free(fieldlist);
	qfits_cache_purge(); // for valgrind
	return 0;
}

static int read_parameters() {
	for (;;) {
		char buffer[10240];
		char* nextword;
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

		if (buffer[0] == '#') {
			if (!silent) {
				fprintf(stderr, "Skipping comment.\n");
				fflush(stderr);
			}
			continue;
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

		} else if (is_word(buffer, "agreetol ", &nextword)) {
			agreetol = atof(nextword);
		} else if (is_word(buffer, "verify_dist ", &nextword)) {
			verify_dist2 = arcsec2distsq(atof(nextword));
		} else if (is_word(buffer, "nagree_toverify ", &nextword)) {
			nagree_toverify = atoi(nextword);
		} else if (is_word(buffer, "overlap_tosolve ", &nextword)) {
			overlap_tosolve = atof(nextword);
		} else if (is_word(buffer, "overlap_tokeep ", &nextword)) {
			overlap_tokeep = atof(nextword);
		} else if (is_word(buffer, "overlap_toprint ", &nextword)) {
			overlap_toprint = atof(nextword);
		} else if (is_word(buffer, "min_ninfield ", &nextword)) {
			// LEGACY
			fprintf(stderr, "Warning, the \"min_ninfield\" command is deprecated."
					"Use \"ninfield_tokeep\" and \"ninfield_tosolve\" instead.\n");
			ninfield_tokeep = ninfield_tosolve = atoi(nextword);
		} else if (is_word(buffer, "ninfield_tokeep ", &nextword)) {
			ninfield_tokeep = atoi(nextword);
		} else if (is_word(buffer, "ninfield_tosolve ", &nextword)) {
			ninfield_tosolve = atoi(nextword);
		} else if (is_word(buffer, "match ", &nextword)) {
			matchfname = strdup(nextword);
		} else if (is_word(buffer, "solved ", &nextword)) {
			solvedfname = strdup(nextword);
		} else if (is_word(buffer, "solvedserver ", &nextword)) {
			solvedserver = strdup(nextword);

		} else if (is_word(buffer, "silent", &nextword)) {
			silent = TRUE;
		} else if (is_word(buffer, "quiet", &nextword)) {
			quiet = TRUE;
		} else if (is_word(buffer, "verbose", &nextword)) {
			verbose = TRUE;
		} else if (is_word(buffer, "wcs ", &nextword)) {
			wcs_template = strdup(nextword);
		} else if (is_word(buffer, "fieldid_key ", &nextword)) {
			free(fieldid_key);
			fieldid_key = strdup(nextword);
		} else if (is_word(buffer, "maxquads ", &nextword)) {
			maxquads = atoi(nextword);
		} else if (is_word(buffer, "xcol ", &nextword)) {
			free(xcolname);
			xcolname = strdup(nextword);
		} else if (is_word(buffer, "ycol ", &nextword)) {
			free(ycolname);
			ycolname = strdup(nextword);
		} else if (is_word(buffer, "index ", &nextword)) {
			char* fname = nextword;
			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);
		} else if (is_word(buffer, "field ", &nextword)) {
			fieldfname = mk_fieldfn(nextword);
		} else if (is_word(buffer, "fieldid ", &nextword)) {
			fieldid = atoi(nextword);
		} else if (is_word(buffer, "done ", &nextword)) {
			donefname = strdup(nextword);
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

static void verified(handlehits* hh, MatchObj* mo) {
	if (!quiet && !silent && verbose && (mo->overlap >= overlap_toprint)) {
		fprintf(stderr, "    field %i", mo->fieldnum);
		if (hh->nagree_toverify)
			fprintf(stderr, " (%i agree)", mo->nagree);
		fprintf(stderr, ": overlap %4.1f%%: %i in field (%im/%iu/%ic)\n",
				100.0 * mo->overlap,
				mo->ninfield, mo->noverlap,
				(mo->ninfield - mo->noverlap - mo->nconflict), mo->nconflict);
		fflush(stderr);
	}
}

static int blind_handle_hit(solver_params* p, MatchObj* mo) {
	bool solved;

	solved = handlehits_add(hits, mo);
	if (!solved)
		return 0;

	return 1;
}

//static double iter_wcs_rads[] = { 2, 3, 4, 5, 6, 7, 8, 9 };

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
	solver.codekd = codekd->tree;
	solver.endobj = enddepth;
	solver.maxtries = maxquads;
	solver.codetol = codetol;
	solver.handlehit = blind_handle_hit;
	solver.cxdx_margin = cxdx_margin;
	solver.quiet = quiet;

	if (funits_upper != 0.0) {
		solver.arcsec_per_pixel_upper = funits_upper;
		solver.minAB = index_scale_lower / funits_upper;

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
		scalefudge = index_scale * M_SQRT1_2 * codetol / funits_upper;

		solver.minAB -= scalefudge;

		if (!silent) {
			fprintf(stderr, "Scale fudge: %g pixels.\n", scalefudge);
			fprintf(stderr, "Set minAB to %g\n", solver.minAB);
		}
	}
	if (funits_lower != 0.0) {
		solver.arcsec_per_pixel_lower = funits_lower;
		solver.maxAB = index_scale / funits_lower;
		solver.maxAB += scalefudge;
		if (!silent)
			fprintf(stderr, "Set maxAB to %g\n", solver.maxAB);
	}

	hits = handlehits_new();
	hits->agreetol = agreetol;
	hits->verify_dist2 = verify_dist2;
	hits->nagree_toverify = nagree_toverify;
	hits->overlap_tokeep  = overlap_tokeep;
	hits->overlap_tosolve = overlap_tosolve;
	hits->ninfield_tokeep  = ninfield_tokeep;
	hits->ninfield_tosolve = ninfield_tosolve;
	hits->startree = starkd->tree;
	hits->do_wcs = (wcs_template ? 1 : 0);
	hits->verified = verified;

	hits->iter_wcs_steps = 0;
	/*
	  hits->iter_wcs_steps = sizeof(iter_wcs_rads)/sizeof(double);
	  hits->iter_wcs_thresh = 0.05;
	  hits->iter_wcs_rads = iter_wcs_rads;
	*/

	nfields = xyls->nfields;

	for (fi=0; fi<il_size(fieldlist); fi++) {
		int fieldnum;
		MatchObj template;
		int nfield;
		qfits_header* fieldhdr = NULL;

		fieldnum = il_get(fieldlist, fi);
		if (fieldnum >= nfields) {
			if (!silent)
				fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			goto cleanup;
		}

		// Has the field already been solved?
		if (solvedfname) {
			if (solvedfile_get(solvedfname, fieldnum)) {
				// file exists; field has already been solved.
				if (!silent)
					fprintf(stderr, "Field %i: solvedfile %s: field has been solved.\n", fieldnum, solvedfname);
				goto cleanup;
			}
			solver.solvedfn = solvedfname;
		} else
			solver.solvedfn = NULL;

		if (solvedserver) {
			if (solvedclient_get(fieldid, fieldnum) == 1) {
				// field has already been solved.
				if (!silent)
					fprintf(stderr, "Field %i: field has already been solved.\n", fieldnum);
				goto cleanup;
			}
		}
		solver.do_solvedserver = (solvedserver ? TRUE : FALSE);


		// Get the field.
		nfield = xylist_n_entries(xyls, fieldnum);
		if (nfield == -1) {
			fprintf(stderr, "Couldn't determine how many objects are in field %i.\n", fieldnum);
			goto cleanup;
		}
		field = realloc(field, 2 * nfield * sizeof(double));
		if (xylist_read_entries(xyls, fieldnum, 0, nfield, field)) {
			fprintf(stderr, "Failed to read field.\n");
			goto cleanup;
		}

		memset(&template, 0, sizeof(MatchObj));
		template.fieldnum = fieldnum;
		template.parity = parity;
		template.fieldfile = fieldid;
		template.indexid = indexid;
		template.healpix = healpix;

		fieldhdr = xylist_get_field_header(xyls,fieldnum);
		if (fieldhdr) {
			char* idstr = qfits_pretty_string(qfits_header_getstr(fieldhdr, fieldid_key));
			if (idstr)
				strncpy(template.fieldname, idstr, sizeof(template.fieldname)-1);
			qfits_header_destroy(fieldhdr);
		}

		solver.fieldid = fieldid;
		solver.fieldnum = fieldnum;
		solver.numtries = 0;
		solver.nummatches = 0;
		solver.numscaleok = 0;
		solver.numcxdxskipped = 0;
		solver.startobj = startdepth;
		solver.field = field;
		solver.nfield = nfield;
		solver.quitNow = FALSE;
		solver.mo_template = &template;
		solver.circle = circle;

		hits->field = field;
		hits->nfield = nfield;

		if (!silent)
			fprintf(stderr, "\nSolving field %i.\n", fieldnum);

		// The real thing
		solve_field(&solver);

		if (!silent)
			fprintf(stderr, "field %i: tried %i quads, matched %i codes.\n",
					fieldnum, solver.numtries, solver.nummatches);

		if (maxquads && solver.numtries >= maxquads && !silent) {
			fprintf(stderr, "  exceeded the number of quads to try: %i >= %i.\n",
					solver.numtries, maxquads);
		}

		// Write the keepable hits.
		if (hits->keepers) {
			int i;
			for (i=0; i<pl_size(hits->keepers); i++) {
				MatchObj* mo = pl_get(hits->keepers, i);
				if (matchfile_write_match(mf, mo))
					fprintf(stderr, "Field %i: error writing a match.\n", fieldnum);
			}
			if (matchfile_fix_header(mf))
				fprintf(stderr, "Failed to fix the matchfile header for field %i.\n", fieldnum);
		}

		if (hits->bestmo) {
			// Field solved!
			if (!silent)
				fprintf(stderr, "Field %i solved with overlap %g.\n", fieldnum,
						hits->bestmo->overlap);

			// Write WCS, if requested.
			if (wcs_template) {
				char wcs_fn[1024];
				FILE* fout;
				qfits_header* wcs;

				sprintf(wcs_fn, wcs_template, fieldnum);
				fout = fopen(wcs_fn, "ab");
				if (!fout) {
					fprintf(stderr, "Failed to open WCS output file %s: %s\n", wcs_fn, strerror(errno));
					exit(-1);
				}

				assert(hits->bestmo->wcs_valid);

				wcs = blind_wcs_get_header(&(hits->bestmo->wcstan));
				boilerplate_add_fits_headers(wcs);
				qfits_header_add(wcs, "HISTORY", "This WCS header was created by the program \"blind\".", NULL, NULL);
				if (solver.mo_template && solver.mo_template->fieldname[0])
					qfits_header_add(wcs, fieldid_key, solver.mo_template->fieldname, "Field name (copied from input field)", NULL);
				if (qfits_header_dump(wcs, fout)) {
					fprintf(stderr, "Failed to write FITS WCS header.\n");
					exit(-1);
				}
				fits_pad_file(fout);
				qfits_header_destroy(wcs);
				fclose(fout);
			}

			// Record in solved file, or send to solved server.
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

		} else {
			// Field unsolved.
			if (!silent)
				fprintf(stderr, "Field %i is unsolved.\n", fieldnum);
		}

		handlehits_free_matchobjs(hits);
		handlehits_clear(hits);

		get_resource_stats(&utime, &stime, NULL);
		gettimeofday(&wtime, NULL);
		if (!silent)
			fprintf(stderr, "  Spent %g s user, %g s system, %g s total, %g s wall time.\n",
					(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime),
					millis_between(&last_wtime, &wtime) * 0.001);

		last_utime = utime;
		last_stime = stime;
		last_wtime = wtime;

	cleanup:
		if (0) {} // to keep gcc happy...
	}

	free(field);
	handlehits_free(hits);
}

void getquadids(uint thisquad, uint *iA, uint *iB, uint *iC, uint *iD) {
	quadfile_get_starids(quads, thisquad, iA, iB, iC, iD);
}

void getstarcoord(uint iA, double *sA) {
	startree_get(starkd, iA, sA);
}

uint64_t getstarid(uint iA) {
	if (id)
		return idfile_get_anid(id, iA);
	return 0;
}
