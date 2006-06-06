/**
 *   Solve fields (with slavish devotion)
 *
 * Inputs: .ckdt .objs .quad
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

#include "kdtree.h"
#include "kdtree_io.h"
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

void solve_fields(xylist *thefields,
				  kdtree_t *codekd);

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
il* fieldlist = NULL;
double funits_lower = 0.0;
double funits_upper = 0.0;
double index_scale;
double index_scale_lower;
double index_scale_lower_factor = 0.0;
bool agreement = FALSE;
int nagree = 4;
int maxnagree = 0;
double agreetol = 0.0;
double verify_dist2 = 0.0;

hitlist* hits = NULL;

matchfile* mf;
matchfile_entry me;

catalog* cat;
idfile* id;
quadfile* quads;
kdtree_t* startree;

// histogram of the size of agreement clusters.
int *agreesizehist;
int Nagreehist;


char* get_pathname(char* fname) {
	char resolved[PATH_MAX];
	if (!realpath(fname, resolved)) {
		fprintf(stderr, "Couldn't resolve real path of %s: %s\n", fname, strerror(errno));
		return NULL;
	}
	return strdup(resolved);
}

int main(int argc, char *argv[]) {
    uint numfields;
	xylist* xy;
    kdtree_t *codekd;
	char* progname = argv[0];
	int i;
	char* path;

    if (argc != 1) {
		printHelp(progname);
		exit(-1);
    }

	fieldlist = il_new(256);

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
		hits = NULL;
		quads = NULL;
		startree = NULL;
		xcolname = strdup("ROWC");
		ycolname = strdup("COLC");

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

		mf = matchfile_open_for_writing(matchfname);
		if (!mf) {
			fprintf(stderr, "Failed to open file %s to write match file.\n", matchfname);
			exit(-1);
		}
		
		// Read .xyls file...
		fprintf(stderr, "Reading fields file %s...", fieldfname);
		fflush(stderr);
		xy = xylist_open(fieldfname);
		if (!xy) {
			fprintf(stderr, "Failed to read xylist.\n");
			exit(-1);
		}
		numfields = xy->nfields;
		fprintf(stderr, "got %u fields.\n", numfields);
		if (parity) {
			fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");
			xy->parity = 1;
		}
		xy->xname = xcolname;
		xy->yname = ycolname;

		// Read .ckdt file...
		fprintf(stderr, "Reading code KD tree from %s...", treefname);
		fflush(stderr);
		codekd = kdtree_fits_read_file(treefname);
		if (!codekd)
			exit(-1);
		fprintf(stderr, "done\n    (%d quads, %d nodes, dim %d).\n",
				codekd->ndata, codekd->nnodes, codekd->ndim);

		// Read .quad file...
		fprintf(stderr, "Reading quads file %s...\n", quadfname);
		quads = quadfile_open(quadfname, 0);
		free_fn(quadfname);
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

		// Read .objs file...
		cat = catalog_open(catfname, 0);
		if (!cat) {
			fprintf(stderr, "Couldn't open catalog %s.\n", catfname);
			exit(-1);
		}
		free(catfname);

		id = idfile_open(idfname, 0);
		if (!id) {
			fprintf(stderr, "Couldn't open id file %s.\n", idfname);
			//exit(-1);
		}
		free(idfname);

		fprintf(stderr, "Reading star KD tree from %s...", startreefname);
		fflush(stderr);
		startree = kdtree_fits_read_file(startreefname);
		if (!startree)
			fprintf(stderr, "Star kdtree not found or failed to read.\n");
		else
			fprintf(stderr, "done\n");

		me.parity = parity;
		path = get_pathname(treefname);
		if (path)
			me.indexpath = path;
		else
			me.indexpath = treefname;
		path = get_pathname(fieldfname);
		if (path)
			me.fieldpath = path;
		else
			me.fieldpath = fieldfname;
		me.codetol = codetol;

		/*
		  me.fieldunits_lower = funits_lower;
		  me.fieldunits_upper = funits_upper;
		*/

		Nagreehist = 100;
		agreesizehist = malloc(Nagreehist * sizeof(int));
		for (i=0; i<Nagreehist; i++)
			agreesizehist[i] = 0;

		// Do it!
		solve_fields(xy, codekd);

		if (donefname) {
			FILE* batchfid = NULL;
			fprintf(stderr, "Writing marker file %s...\n", donefname);
			fopenout(donefname, batchfid);
			fclose(batchfid);
		}
		free(donefname);
		free(solvedfname);
		free(matchfname);

		free(me.indexpath);
		free(me.fieldpath);

		free(xcolname);
		free(ycolname);

		xylist_close(xy);

		matchfile_close(mf);

		free_fn(fieldfname);
		free_fn(treefname);
		free_fn(startreefname);

		kdtree_close(codekd);
		catalog_close(cat);
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
					"    run\n"
					"    help\n"
					"    quit\n");
		} else if (is_word(buffer, "xcol ", &nextword)) {
			xcolname = strdup(nextword);
		} else if (is_word(buffer, "ycol ", &nextword)) {
			ycolname = strdup(nextword);
		} else if (is_word(buffer, "agreement", &nextword)) {
			agreement = TRUE;
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
		} else if (is_word(buffer, "verify_dist", &nextword)) {
			double d = atof(nextword);
			verify_dist2 = arcsec2distsq(d);
		} else if (is_word(buffer, "field ", &nextword)) {
			char* fname = nextword;
			fieldfname = mk_fieldfn(fname);
		} else if (is_word(buffer, "match ", &nextword)) {
			char* fname = nextword;
			matchfname = strdup(fname);
		} else if (is_word(buffer, "done ", &nextword)) {
			char* fname = nextword;
			donefname = strdup(fname);
		} else if (is_word(buffer, "solved", &nextword)) {
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
					il_append(fieldlist, fld);
				} else {
					if (firstfld > fld) {
						fprintf(stderr, "Ranges must be specified as <start>/<end>: %i/%i\n", firstfld, fld);
					} else {
						for (i=firstfld+1; i<=fld; i++) {
							il_append(fieldlist, i);
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

int verify_hit(MatchObj* mo, solver_params* p) {
	xy* field = p->field;
	int i, NF;
	double* fieldstars;
	intmap* map;
	int matches;
	int unmatches;
	int conflicts;
	double avgmatch;
	double maxmatch;
	assert(mo->transform);
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
	map = intmap_new();
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

	printf("%i matches, %i unmatches, %i conflicts.  Avg match dist: %g arcsec, max dist %g arcsec\n",
		   matches, unmatches, conflicts, rad2arcsec(distsq2arc(square(avgmatch))),
		   rad2arcsec(distsq2arc(maxmatch)));
	fflush(stdout);

	intmap_free(map);
	free(fieldstars);
	return 1;
}

int handlehit(solver_params* p, MatchObj* mo) {
	if (agreement) {
		int nagree;
		// hack - share this struct between all the matches for this
		// field...
		mo->extra = &me;
		// compute (x,y,z) center, scale, rotation.
		hitlist_healpix_compute_vector(mo);
		nagree = hitlist_healpix_add_hit(hits, mo);
		if (nagree >= 2) {
			if (startree && (verify_dist2 > 0.0)) {
				if (!verify_hit(mo, p)) {
					// hit not verified.
					return 0;
				}
			}
		}
		return nagree;
	} else {
		if (matchfile_write_match(mf, mo)) {
			fprintf(stderr, "Failed to write matchfile entry.\n");
		}
		if (mo->transform)
			free(mo->transform);
		free_MatchObj(mo);
		return 1;
	}
}

void solve_fields(xylist *thefields, kdtree_t* codekd) {
	int i;
	solver_params solver;
	double last_utime, last_stime;
	int nfields;

	get_resource_stats(&last_utime, &last_stime, NULL);

	solver_default_params(&solver);

	solver.codekd = codekd;
	solver.endobj = enddepth;
	solver.maxtries = 0;
	solver.max_matches_needed = maxnagree;
	solver.codetol = codetol;
	solver.cornerpix = mk_xy(2);
	solver.handlehit = handlehit;
	solver.quitNow = FALSE;

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

	nfields = thefields->nfields;

	for (i=0; i<il_size(fieldlist); i++) {
		xy *thisfield;
		int fieldnum;
		double utime, stime;

		fieldnum = il_get(fieldlist, i);
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
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
				continue;
			}
		}

		thisfield = xylist_get_field(thefields, fieldnum);
		if (!thisfield) {
			fprintf(stderr, "Couldn't get field %i\n", fieldnum);
			continue;
		}

		me.fieldnum = fieldnum;

		solver.fieldnum = fieldnum;
		solver.numtries = 0;
		solver.nummatches = 0;
		solver.mostagree = 0;
		solver.startobj = startdepth;
		solver.field = thisfield;

		if (agreement) {
			hits = hitlist_healpix_new(agreetol);
		}

		if (matchfile_start_table(mf, &me) ||
			matchfile_write_table(mf)) {
			fprintf(stderr, "Error: Failed to write matchfile table.\n");
		}

		// The real thing
		solve_field(&solver);

		free_xy(thisfield);

		fprintf(stderr, "    field %i: tried %i quads, matched %i codes.\n\n",
				fieldnum, solver.numtries, solver.nummatches);

		if (agreement) {
			int nbest, j;
			pl* best;
			int* thisagreehist;
			int maxagree = 0;
			int k;
			int nperlist;
			thisagreehist = calloc(Nagreehist, sizeof(int));
			hitlist_healpix_histogram_agreement_size(hits, thisagreehist, Nagreehist);

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
			nbest = hitlist_healpix_count_best(hits);
			nperlist = nbest;
			fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);

			//best = hitlist_get_best(hits);
			best = hitlist_healpix_get_all_best(hits);
			//best = hitlist_get_all_above_size(hits, nagree);

			nbest = pl_size(best);
			if (nbest)
				fprintf(stderr, "(There are %i sets of agreeing hits of size %i.)\n",
						pl_size(best) / nperlist, nperlist);

			// if there are only singleton hits, don't write anything.
			if (nbest > 1) {
				for (j=0; j<nbest; j++) {
					matchfile_entry* me;
					MatchObj* mo = (MatchObj*)pl_get(best, j);
					me = (matchfile_entry*)mo->extra;
					if (matchfile_write_match(mf, mo)) {
						fprintf(stderr, "Error writing a match.\n");
					}
				}
			}
			pl_free(best);
			hitlist_healpix_clear(hits);
			hitlist_healpix_free(hits);

			if (maxnagree && (nbest >= maxnagree) && solvedfname) {
				// write a file to indicate that the field was solved.
				char fn[256];
				FILE* f;
				sprintf(fn, solvedfname, fieldnum);
				fprintf(stderr, "Field %i solved: writing file %s to indicate this.\n",
						fieldnum, fn);
				if (!(f = fopen(fn, "w")) ||
					fclose(f)) {
					fprintf(stderr, "Failed to write field-finished file %s.\n", fn);
				}
			}

		}

		if (matchfile_fix_table(mf)) {
			fprintf(stderr, "Error: Failed to fix matchfile table.\n");
		}

		get_resource_stats(&utime, &stime, NULL);
		fprintf(stderr, "    spent %g s user, %g s system, %g s total.\n",
				(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime));
		last_utime = utime;
		last_stime = stime;
	}
	free_xy(solver.cornerpix);
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
	memcpy(sA, catalog_get_star(cat, iA), DIM_STARS * sizeof(double));
}

uint64_t getstarid(uint iA) {
	if (id)
		return idfile_get_anid(id, iA);
	return 0;
}
