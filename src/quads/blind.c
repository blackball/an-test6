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

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "solver.h"
#include "solver_callbacks.h"
#include "matchobj.h"
#include "matchfile.h"
#include "hitlist.h"
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
#include "handlehits.h"

static void printHelp(char* progname) {
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n", progname);
}

static void solve_fields();
static int read_parameters();
static void reset_next_field();
static void write_hits(int fieldnum, pl* matches);

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
double overlap_tokeep;
double overlap_tosolve;
int ninfield_tokeep;
int ninfield_tosolve;
double cxdx_margin;
int maxquads;

bool quiet;
bool silent;

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
		overlap_tokeep = 0.0;
		overlap_tosolve = 0.0;
		ninfield_tosolve = 0;
		ninfield_tokeep = 0;
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
			fprintf(stderr, "overlap_tokeep %f\n", overlap_tokeep);
			fprintf(stderr, "overlap_tosolve %f\n", overlap_tosolve);
			fprintf(stderr, "ninfield_tokeep %i\n", ninfield_tokeep);
			fprintf(stderr, "ninfield_tosolve %i\n", ninfield_tosolve);
			fprintf(stderr, "xcolname %s\n", xcolname);
			fprintf(stderr, "ycolname %s\n", ycolname);
			fprintf(stderr, "cxdx_margin %g\n", cxdx_margin);
			fprintf(stderr, "maxquads %i\n", maxquads);
			fprintf(stderr, "quiet %i\n", quiet);
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
		if (!id)
			fprintf(stderr, "Couldn't open id file %s.\n", idfname);

		// Do it!
		solve_fields();
		// DEBUG
		write_hits(-1, NULL);

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
		} else if (is_word(buffer, "wcs ", &nextword)) {
			wcs_template = strdup(nextword);
		} else if (is_word(buffer, "fieldid_key ", &nextword)) {
			free(fieldid_key);
			fieldid_key = strdup(nextword);
		} else if (is_word(buffer, "maxquads ", &nextword)) {
			maxquads = atoi(nextword);
		} else if (is_word(buffer, "cxdx_margin ", &nextword)) {
			cxdx_margin = atof(nextword);
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
	return;
}

static qfits_header* compute_wcs(MatchObj* mo, solver_params* params,
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
	if (!corr)
		Ncorr = 4;
	else {
		Ncorr = 0;
		for (i=0; i<params->nfield; i++)
			if (corr[i] != -1)
				Ncorr++;
	}

	// -allocate and fill "p" and "f" arrays.
	p = malloc(Ncorr * 2 * sizeof(double));
	f = malloc(Ncorr * 2 * sizeof(double));
	j = 0;
	if (!corr) {
		// just use the four stars that compose the quad.
		for (i=0; i<4; i++) {
			getstarcoord(mo->star[i], xyz);
			star_coords(xyz, starcmass, p + 2*i, p + 2*i + 1);
			f[2*i+0] = params->field[mo->field[i] * 2 + 0];
			f[2*i+1] = params->field[mo->field[i] * 2 + 1];
		}
	} else {
		// use all correspondences.
		for (i=0; i<params->nfield; i++)
			if (corr[i] != -1) {
				// -project the stars around the quad center
				getstarcoord(corr[i], xyz);
				star_coords(xyz, starcmass, p + 2*j, p + 2*j + 1);
				// -grab the corresponding field coords.
				f[2*j+0] = params->field[2*i+0];
				f[2*j+1] = params->field[2*i+1];
				j++;
			}
	}

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

	// -compute the covariance between field positions and projected
	//  positions of the corresponding stars.
	for (i=0; i<4; i++)
		cov[i] = 0.0;
	for (i=0; i<Ncorr; i++)
		for (j=0; j<2; j++)
			for (k=0; k<2; k++)
				cov[j*2 + k] += p[i*2 + k] * f[i*2 + j];
	// -run SVD
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
		scale = sqrt(pvar / fvar);
	}

	// -write the WCS header.
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

static int blind_handle_hit(solver_params* p, MatchObj* mo) {
	bool solved;

	solved = handlehits_add(hits, mo);
	if (!solved)
		return 0;

	/*
	  if (!quiet && !silent)
	  fprintf(stderr, "    field %i (%i agree): overlap %4.1f%%: %i in field (%im/%iu/%ic)\n",
	  fieldnum, nagree, 100.0 * mo->overlap, mo->ninfield, matches, unmatches, conflicts);
	  fflush(stderr);
	  }
	*/

	/*
	  p->quitNow = TRUE;
	*/

	return 1;
}


static int next_field_index = 0;

static void reset_next_field() {
	next_field_index = 0;
}

static int next_field(xy** p_field, qfits_header** p_fieldhdr) {
	int rtn;

	if (next_field_index >= il_size(fieldlist))
		rtn = -1;
	else {
		rtn = il_get(fieldlist, next_field_index);
		next_field_index++;
		if (p_field)
			*p_field = xylist_get_field(xyls, rtn);
		if (p_fieldhdr)
			*p_fieldhdr = xylist_get_field_header(xyls, rtn);
	}

	return rtn;
}

static void solve_fields() {
	solver_params solver;
	double last_utime, last_stime;
	double utime, stime;
	struct timeval wtime, last_wtime;
	int nfields;
	double* field = NULL;

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
		if (!silent)
			fprintf(stderr, "Set minAB to %g\n", solver.minAB);
	}
	if (funits_lower != 0.0) {
		solver.arcsec_per_pixel_lower = funits_lower;
		solver.maxAB = index_scale / funits_lower;
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
	hits->do_corr = (wcs_template ? 1 : 0);

	nfields = xyls->nfields;

	for (;;) {
		xy *thisfield = NULL;
		int fieldnum;
		MatchObj template;
		int nfield;
		qfits_header* fieldhdr = NULL;

		fieldnum = next_field(&thisfield, &fieldhdr);

		if (fieldnum == -1)
			break;
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			write_hits(fieldnum, NULL);
			goto cleanup;
		}
		if (!thisfield) {
			// HACK - why is this happening? QFITS + multithreading interaction bug?
			// or running out of address space?
			fprintf(stderr, "Couldn't get field %i\n", fieldnum);
			write_hits(fieldnum, NULL);
			goto cleanup;
		}

		if (solvedfname) {
			if (solvedfile_get(solvedfname, fieldnum)) {
				// file exists; field has already been solved.
				if (!silent)
					fprintf(stderr, "Field %i: solvedfile %s: field has been solved.\n", fieldnum, solvedfname);
				write_hits(fieldnum, NULL);
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
				write_hits(fieldnum, NULL);
				goto cleanup;
			}
		}
		solver.do_solvedserver = (solvedserver ? TRUE : FALSE);

		nfield = xy_size(thisfield);
		field = realloc(field, 2 * nfield * sizeof(double));
		dl_copy(thisfield, 0, 2 * nfield, field);
		free_xy(thisfield);
		thisfield = NULL;

		memset(&template, 0, sizeof(MatchObj));
		template.fieldnum = fieldnum;
		template.parity = parity;
		template.fieldfile = fieldid;
		template.indexid = indexid;
		template.healpix = healpix;

		if (fieldhdr) {
			char* idstr = qfits_pretty_string(qfits_header_getstr(fieldhdr, fieldid_key));
			if (idstr)
				strncpy(template.fieldname, idstr, sizeof(template.fieldname)-1);
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

		handlehits_clear(hits);

		hits->field = field;
		hits->nfield = nfield;

		if (!silent)
			fprintf(stderr, "Solving field %i.\n", fieldnum);

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
		if (hits->keepers)
			write_hits(fieldnum, hits->keepers);
		else
			write_hits(fieldnum, NULL);

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
				wcs = compute_wcs(hits->bestmo, &solver, hits->bestcorr);
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

		get_resource_stats(&utime, &stime, NULL);
		gettimeofday(&wtime, NULL);
		if (!silent)
			fprintf(stderr, "Spent %g s user, %g s system, %g s total, %g s wall time.\n\n\n",
					(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime),
					millis_between(&last_wtime, &wtime) * 0.001);
		last_utime = utime;
		last_stime = stime;
		last_wtime = wtime;

	cleanup:
		if (thisfield)
			free_xy(thisfield);
		if (fieldhdr)
			qfits_header_destroy(fieldhdr);
	}

	free(field);
	handlehits_free(hits);
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
