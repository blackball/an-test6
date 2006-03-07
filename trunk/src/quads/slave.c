/**
 *   Solve fields (using Keir's kdtrees)
 *
 * Inputs: .ckdt2 .objs .quad
 * Output: .match
 */

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "blocklist.h"
#include "solver2.h"
#include "solver2_callbacks.h"
#include "matchobj.h"
#include "matchfile.h"
#include "catalog.h"
#include "hitlist.h"
#include "hitlist_healpix.h"
#include "tic.h"

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n", progname);
}

#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP FALSE

void solve_fields(xyarray *thefields,
				  kdtree_t *codekd);

int read_parameters();

// params:
char *fieldfname = NULL, *treefname = NULL;
char *quadfname = NULL, *catfname = NULL;
char* matchfname = NULL;
char* donefname = NULL;
bool parity = DEFAULT_PARITY_FLIP;
double codetol = DEFAULT_CODE_TOL;
int startdepth = 0;
int enddepth = 0;
blocklist* fieldlist = NULL;
double funits_lower = 0.0;
double funits_upper = 0.0;
double index_scale;
double index_scale_lower = 0.0;
bool agreement = FALSE;
int nagree = 4;
double agreetol = 0.0;

catalog* cat;

hitlist* hits = NULL;

matchfile_entry matchfile;

FILE *quadfid = NULL;
FILE* matchfid = NULL;

bool use_mmap = TRUE;

// when not mmap'd
off_t qposmarker;

// when mmap'd
sidx*   quadindex;

// the largest star and quad available in the corresponding files.
qidx maxquad;

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
    FILE *fieldfid = NULL, *treefid = NULL;
    qidx numfields, numquads;
    char readStatus;
    dimension Dim_Quads;
    xyarray *thefields = NULL;
    kdtree_t *codekd = NULL;

	void*  mmap_quad = NULL;
	size_t mmap_quad_size = 0;

	off_t endoffset;
	char* progname = argv[0];
	int i;
	char* path;
	sidx numstars;

	tic();

    if (argc != 1) {
		printHelp(progname);
		exit(-1);
    }

	fieldlist = blocklist_int_new(256);

	if (read_parameters()) {
		exit(-1);
	}

	hitlist_set_default_parameters();
	if (agreetol != 0.0) {
		char buf[256];
		// total hack...
		sprintf(buf, "%g", agreetol);
		hitlist_process_parameter('m', buf);
	}

	fprintf(stderr, "%s params:\n", progname);
	fprintf(stderr, "fieldfname %s\n", fieldfname);
	fprintf(stderr, "treefname %s\n", treefname);
	fprintf(stderr, "quadfname %s\n", quadfname);
	fprintf(stderr, "catfname %s\n", catfname);
	fprintf(stderr, "matchfname %s\n", matchfname);
	fprintf(stderr, "donefname %s\n", donefname);
	fprintf(stderr, "parity %i\n", parity);
	fprintf(stderr, "codetol %g\n", codetol);
	fprintf(stderr, "startdepth %i\n", startdepth);
	fprintf(stderr, "enddepth %i\n", enddepth);
	fprintf(stderr, "fieldunits_lower %g\n", funits_lower);
	fprintf(stderr, "fieldunits_upper %g\n", funits_upper);
	fprintf(stderr, "index_lower %g\n", index_scale_lower);
	fprintf(stderr, "agreement %i\n", agreement);
	fprintf(stderr, "num-to-agree %i\n", nagree);
	fprintf(stderr, "agreetol %g\n", agreetol);

	fprintf(stderr, "fields ");
	for (i=0; i<blocklist_count(fieldlist); i++)
		fprintf(stderr, "%i ", blocklist_int_access(fieldlist, i));
	fprintf(stderr, "\n");

	if (!treefname || !fieldfname || (codetol < 0.0) || !matchfname) {
		fprintf(stderr, "Invalid params... this message is useless.\n");
		exit(-1);
	}

	fopenout(matchfname, matchfid);

	// Read .xyls file...
	fprintf(stderr, "  Reading fields...");
	fflush(stderr);
	fopenin(fieldfname, fieldfid);
	thefields = readxy(fieldfid, parity);
	fclose(fieldfid);
	if (!thefields)
		exit(-1);
	numfields = (qidx)thefields->size;
	fprintf(stderr, "got %lu fields.\n", numfields);
	if (parity)
		fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");

	// Read .ckdt2 file...
	fprintf(stderr, "  Reading code KD tree from %s...", treefname);
	fflush(stderr);
	fopenin(treefname, treefid);
	codekd = kdtree_read(treefid);
	if (!codekd)
		exit(-1);
	fclose(treefid);
	fprintf(stderr, "done\n    (%d quads, %d nodes, dim %d).\n",
			codekd->ndata, codekd->nnodes, codekd->ndim);

	// Read .quad file...
	fopenin(quadfname, quadfid);
	free_fn(quadfname);
	readStatus = read_quad_header(quadfid, &numquads, &numstars, 
								  &Dim_Quads, &index_scale);
	if (readStatus == READ_FAIL)
		exit(-1);
	qposmarker = ftello(quadfid);
	// check that the quads file is the right size.
	fseeko(quadfid, 0, SEEK_END);
	endoffset = ftello(quadfid) - qposmarker;
	maxquad = endoffset / (DIM_QUADS * sizeof(sidx));
	if (maxquad != numquads) {
		fprintf(stderr, "Error: numquads=%li (specified in .quad file header) does "
				"not match maxquad=%li (computed from size of .quad file).\n",
				numquads, maxquad);
		exit(-1);
	}
	if (use_mmap) {
		mmap_quad_size = endoffset;
		mmap_quad = mmap(0, mmap_quad_size, PROT_READ, MAP_SHARED,
						 fileno(quadfid), 0);
		if (mmap_quad == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap quad file: %s\n", strerror(errno));
			exit(-1);
		}
		fclose(quadfid);
		quadindex = (sidx*)(((char*)(mmap_quad)) + qposmarker);
	}

	// index_scale is specified in radians - switch to arcsec.
	index_scale *= (180.0 / M_PI) * 60 * 60;
	fprintf(stderr, "Index scale: %g arcmin, %g arcsec\n", index_scale/60.0, index_scale);

	cat = catalog_open(catfname);
	if (!cat) {
		fprintf(stderr, "Couldn't open catalog %s.\n", catfname);
		exit(-1);
	}
	free(catfname);

	matchfile.parity = parity;
	path = get_pathname(treefname);
	if (path)
		matchfile.indexpath = path;
	else
		matchfile.indexpath = treefname;
	path = get_pathname(fieldfname);
	if (path)
		matchfile.fieldpath = path;
	else
		matchfile.fieldpath = fieldfname;
	matchfile.codetol = codetol;

	matchfile.fieldunits_lower = funits_lower;
	matchfile.fieldunits_upper = funits_upper;

	Nagreehist = 100;
	agreesizehist = malloc(Nagreehist * sizeof(int));
	for (i=0; i<Nagreehist; i++)
		agreesizehist[i] = 0;

	// Do it!
	solve_fields(thefields, codekd);

	if (donefname) {
		FILE* batchfid = NULL;
		fprintf(stderr, "Writing marker file %s...\n", donefname);
		fopenout(donefname, batchfid);
		fclose(batchfid);
	}

	blocklist_free(fieldlist);
	free_xyarray(thefields);
	fclose(matchfid);
	free_fn(fieldfname);
	free_fn(treefname);
	if (use_mmap) {
		kdtree_close(codekd);
		munmap(mmap_quad, mmap_quad_size);
	} else {
		kdtree_free(codekd);
		fclose(quadfid);
	}

	catalog_close(cat);

	{
		int maxagree = 0;
		for (i=0; i<Nagreehist; i++)
			if (agreesizehist[i])
				maxagree = i;
		/*
		  if (agreesizehist[i] > maxagree)
		  maxagree = agreesizehist[i];
		*/
		fprintf(stderr, "Agreement cluster size histogram:\n");
		fprintf(stderr, "nagreehist_total=[");
		for (i=0; i<=maxagree; i++)
			fprintf(stderr, "%i,", agreesizehist[i]);
		fprintf(stderr, "];\n");

		free(agreesizehist);
	}

	toc();

	return 0;
}

int read_parameters() {
	for (;;) {
		char buffer[10240];
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

		if (strncmp(buffer, "help", 4) == 0) {
			fprintf(stderr, "Commands:\n"
					"    index <index-file-name>\n"
					"    match <match-file-name>\n"
					"    done <done-file-name>\n"
					"    field <field-file-name>\n"
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
					"    agreetol <agreement-tolerance (arcsec)>\n"
					"    run\n"
					"    help\n");
		} else if (strncmp(buffer, "agreement", 9) == 0) {
			agreement = TRUE;
		} else if (strncmp(buffer, "nagree ", 7) == 0) {
			char* nag = buffer + 7;
			nagree = atoi(nag);
		} else if (strncmp(buffer, "agreetol ", 9) == 0) {
			char* ag = buffer + 9;
			agreetol = atof(ag);
		} else if (strncmp(buffer, "index ", 6) == 0) {
			char* fname = buffer + 6;
			treefname = mk_ctree2fn(fname);
			quadfname = mk_quadfn(fname);
			catfname = strdup(fname);
		} else if (strncmp(buffer, "field ", 6) == 0) {
			char* fname = buffer + 6;
			fieldfname = mk_fieldfn(fname);
		} else if (strncmp(buffer, "match ", 6) == 0) {
			char* fname = buffer + 6;
			matchfname = strdup(fname);
		} else if (strncmp(buffer, "done ", 5) == 0) {
			char* fname = buffer + 5;
			donefname = strdup(fname);
		} else if (strncmp(buffer, "sdepth ", 7) == 0) {
			int d = atoi(buffer + 7);
			startdepth = d;
		} else if (strncmp(buffer, "depth ", 6) == 0) {
			int d = atoi(buffer + 6);
			enddepth = d;
		} else if (strncmp(buffer, "tol ", 4) == 0) {
			double t = atof(buffer + 4);
			codetol = t;
		} else if (strncmp(buffer, "parity ", 7) == 0) {
			int d = atoi(buffer + 7);
			parity = (d ? TRUE : FALSE);
		} else if (strncmp(buffer, "index_lower ", 12) == 0) {
			double d = atof(buffer + 12);
			index_scale_lower = d;
		} else if (strncmp(buffer, "fieldunits_lower ", 17) == 0) {
			double d = atof(buffer + 17);
			funits_lower = d;
		} else if (strncmp(buffer, "fieldunits_upper ", 17) == 0) {
			double d = atof(buffer + 17);
			funits_upper = d;
		} else if (strncmp(buffer, "fields ", 7) == 0) {
			char* str = buffer + 7;
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
					blocklist_int_append(fieldlist, fld);
				} else {
					if (firstfld > fld) {
						fprintf(stderr, "Ranges must be specified as <start>/<end>: %i/%i\n", firstfld, fld);
					} else {
						for (i=firstfld+1; i<=fld; i++) {
							blocklist_int_append(fieldlist, i);
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
		} else if (strncmp(buffer, "run", 3) == 0) {
			return 0;
		} else {
			fprintf(stderr, "I didn't understand that command.\n");
			fflush(stderr);
		}
	}
}

int handlehit(struct solver_params* p, MatchObj* mo) {
	if (agreement) {
		// hack - share this struct between all the matches for this
		// field...
		mo->extra = &matchfile;
		// compute (x,y,z) center, scale, rotation.
		hitlist_healpix_compute_vector(mo);
		hitlist_add_hit(hits, mo);
	} else {
		if (matchfile_write_match(matchfid, mo, &matchfile)) {
			fprintf(stderr, "Failed to write matchfile entry: %s\n", strerror(errno));
		}
		free_star(mo->sMin);
		free_star(mo->sMax);
		free_MatchObj(mo);
	}
	return 1;
}

void solve_fields(xyarray *thefields, kdtree_t* codekd) {
	int i;
	solver_params solver;
	double last_utime, last_stime;
	int nfields;

	get_resource_stats(&last_utime, &last_stime, NULL);

	solver_default_params(&solver);

	solver.codekd = codekd;
	solver.endobj = enddepth;
	solver.maxtries = 0;
	solver.max_matches_needed = 0;
	solver.codetol = codetol;
	solver.cornerpix = mk_xy(2);
	solver.handlehit = handlehit;
	solver.quitNow = FALSE;

	if (funits_upper != 0.0) {
		solver.minAB = index_scale * index_scale_lower / funits_upper;
		solver.arcsec_per_pixel_upper = funits_upper;
		solver.arcsec_per_pixel_lower = funits_lower;
		fprintf(stderr, "Set minAB to %g\n", solver.minAB);
	}
	if (funits_lower != 0.0) {
		solver.maxAB = index_scale / funits_lower;
		fprintf(stderr, "Set maxAB to %g\n", solver.maxAB);
	}

	nfields = dyv_array_size(thefields);

	for (i=0; i<blocklist_count(fieldlist); i++) {
		xy *thisfield;
		int fieldnum;
		double utime, stime;

		fieldnum = blocklist_int_access(fieldlist, i);
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			continue;
		}
		thisfield = xya_ref(thefields, fieldnum);
		if (!thisfield) {
			fprintf(stderr, "Field %i is null.\n", fieldnum);
			continue;
		}

		matchfile.fieldnum = fieldnum;

		solver.numtries = 0;
		solver.nummatches = 0;
		solver.mostagree = 0;
		solver.startobj = startdepth;
		solver.field = thisfield;

		if (agreement) {
			hits = hitlist_new();
		}

		// The real thing
		solve_field(&solver);

		fprintf(stderr, "    field %i: tried %i quads, matched %i codes.\n\n",
				fieldnum, solver.numtries, solver.nummatches);

		if (agreement) {
			int nbest, j;
			blocklist* best;
			int* thisagreehist;
			int maxagree = 0;
			int k;
			thisagreehist = malloc(Nagreehist * sizeof(int));
			for (k=0; k<Nagreehist; k++)
				thisagreehist[k] = 0;

			hitlist_histogram_agreement_size(hits, thisagreehist, Nagreehist);

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

			nbest = hitlist_count_best(hits);

			fprintf(stderr, "Field %i: %i in agreement.\n", fieldnum, nbest);

			//best = hitlist_get_best(hits);
			best = hitlist_get_all_best(hits);
			//best = hitlist_get_all_above_size(hits, nagree);

			fprintf(stderr, "(There are %i sets of agreeing hits of size %i.)\n",
					blocklist_count(best) / nbest, nbest);

			nbest = blocklist_count(best);

			for (j=0; j<nbest; j++) {
				matchfile_entry* me;
				MatchObj* mo = (MatchObj*)blocklist_pointer_access(best, j);
				me = (matchfile_entry*)mo->extra;
				if (matchfile_write_match(matchfid, mo, me)) {
					fprintf(stderr, "Error writing a match: %s\n", strerror(errno));
				}
			}
			blocklist_free(best);
			hitlist_clear(hits);
			hitlist_free(hits);
		}

		get_resource_stats(&utime, &stime, NULL);
		fprintf(stderr, "    spent %g s user, %g s system, %g s total.\n",
				(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime));
		last_utime = utime;
		last_stime = stime;
	}
	free_xy(solver.cornerpix);
}

void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD) {
	if (thisquad >= maxquad) {
		fprintf(stderr, "thisquad %lu >= maxquad %lu\n", thisquad, maxquad);
	}
	if (use_mmap) {

		*iA = quadindex[thisquad*DIM_QUADS + 0];
		*iB = quadindex[thisquad*DIM_QUADS + 1];
		*iC = quadindex[thisquad*DIM_QUADS + 2];
		*iD = quadindex[thisquad*DIM_QUADS + 3];

	} else {
		fseeko(quadfid, qposmarker + thisquad * DIM_QUADS * sizeof(sidx), SEEK_SET);
		readonequad(quadfid, iA, iB, iC, iD);
	}
}

void getstarcoords(star *sA, star *sB, star *sC, star *sD,
				   sidx iA, sidx iB, sidx iC, sidx iD)
{
	memcpy(sA->farr, catalog_get_star(cat, iA), DIM_STARS * sizeof(double));
	memcpy(sB->farr, catalog_get_star(cat, iB), DIM_STARS * sizeof(double));
	memcpy(sC->farr, catalog_get_star(cat, iC), DIM_STARS * sizeof(double));
	memcpy(sD->farr, catalog_get_star(cat, iD), DIM_STARS * sizeof(double));
}

	
