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

matchfile_entry matchfile;

FILE *quadfid = NULL, *catfid = NULL;
FILE* matchfid = NULL;

bool use_mmap = TRUE;

// when not mmap'd
off_t qposmarker, cposmarker;

// when mmap'd
double* catalogue;
sidx*   quadindex;

// the largest star and quad available in the corresponding files.
sidx maxstar;
qidx maxquad;

char* get_pathname(char* fname) {
	char resolved[PATH_MAX];
	if (!realpath(fname, resolved)) {
		fprintf(stderr, "Couldn't resolve real path of %s: %s\n", fname, strerror(errno));
		return NULL;
	}
	return strdup(resolved);
}

int get_resource_stats(double* p_usertime, double* p_systime, long* p_maxrss) {
	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage)) {
		fprintf(stderr, "getrusage failed: %s\n", strerror(errno));
		return 1;
	}
	if (p_usertime) {
		*p_usertime = usage.ru_utime.tv_sec + 1e-6 * usage.ru_utime.tv_usec;
	}
	if (p_systime) {
		*p_systime = usage.ru_stime.tv_sec + 1e-6 * usage.ru_stime.tv_usec;
	}
	if (p_maxrss) {
		*p_maxrss = usage.ru_maxrss;
	}
	return 0;
}

int main(int argc, char *argv[]) {
    FILE *fieldfid = NULL, *treefid = NULL;
    qidx numfields, numquads;
    sidx numstars;
    char readStatus;
	double ramin, ramax, decmin, decmax;
    dimension Dim_Quads, Dim_Stars;
    xyarray *thefields = NULL;
    kdtree_t *codekd = NULL;

    void*  mmap_tree = NULL;
    size_t mmap_tree_size = 0;
	void*  mmap_cat = NULL;
	size_t mmap_cat_size = 0;
	void*  mmap_quad = NULL;
	size_t mmap_quad_size = 0;

	off_t endoffset;
	char* progname = argv[0];
	int i;
	char* path;
	time_t starttime, endtime;

	double minAB=0.0, maxAB=0.0;

	starttime = time(NULL);

    if (argc != 1) {
		printHelp(progname);
		exit(-1);
    }

	fieldlist = blocklist_int_new(256);

	if (read_parameters()) {
		exit(-1);
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
	codekd = kdtree_read(treefid, use_mmap, &mmap_tree, &mmap_tree_size);
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

	// Read .objs file...
	fopenin(catfname, catfid);
	free_fn(catfname);
	readStatus = read_objs_header(catfid, &numstars, &Dim_Stars,
								  &ramin, &ramax, &decmin, &decmax);
	if (readStatus == READ_FAIL) {
		exit(-1);
	}
	cposmarker = ftello(catfid);
	// check that the catalogue file is the right size.
	fseeko(catfid, 0, SEEK_END);
	endoffset = ftello(catfid) - cposmarker;
	maxstar = endoffset / (DIM_STARS * sizeof(double));
	if (maxstar != numstars) {
		fprintf(stderr, "Error: numstars=%li (specified in .objs file header) does "
				"not match maxstars=%li (computed from size of .objs file).\n",
				numstars, maxstar);
		exit(-1);
	}
	if (use_mmap) {
		mmap_cat_size = endoffset;
		mmap_cat = mmap(0, mmap_cat_size, PROT_READ, MAP_SHARED, fileno(catfid), 0);
		if (mmap_cat == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap catalogue file: %s\n", strerror(errno));
			exit(-1);
		}
		fclose(catfid);
		catalogue = (double*)(((char*)(mmap_cat)) + cposmarker);
	}

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
		munmap(mmap_tree, mmap_tree_size);
		free(codekd);
		munmap(mmap_quad, mmap_quad_size);
		munmap(mmap_cat, mmap_cat_size);
	} else {
		kdtree_free(codekd);
		fclose(quadfid);
		fclose(catfid);
	}

	{
		double utime, stime;
		long rss;
		int dtime;
		endtime = time(NULL);
		dtime = (int)(endtime - starttime);
		if (!get_resource_stats(&utime, &stime, &rss)) {
			fprintf(stderr, "Finished: used %g s user, %g s system (%g s total), %i s wall time, max rss %li\n",
					utime, stime, utime+stime, dtime, rss);
		}
	}

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
					"    run\n"
					"    help\n");
		} else if (strncmp(buffer, "index ", 6) == 0) {
			char* fname = buffer + 6;
			treefname = mk_ctree2fn(fname);
			quadfname = mk_quadfn(fname);
			catfname = mk_catfn(fname);
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
	if (matchfile_write_match(matchfid, mo, &matchfile)) {
		fprintf(stderr, "Failed to write matchfile entry: %s\n", strerror(errno));
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

	if (funits_lower != 0.0) {
		solver.maxAB = index_scale / funits_lower;
	}
	if (funits_upper != 0.0) {
		solver.minAB = index_scale / funits_upper;
	}

	if ((funits_lower > 0.0) || (funits_upper > 0.0)) {
		
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

		// The real thing
		solve_field(&solver);

		fprintf(stderr, "    field %i: tried %i quads, matched %i codes.\n\n",
				fieldnum, solver.numtries, solver.nummatches);

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
	if (iA >= maxstar) {
		fprintf(stderr, "iA %lu > maxstar %lu\n", iA, maxstar);
	}
	if (iB >= maxstar) {
		fprintf(stderr, "iB %lu > maxstar %lu\n", iB, maxstar);
	}
	if (iC >= maxstar) {
		fprintf(stderr, "iC %lu > maxstar %lu\n", iC, maxstar);
	}
	if (iD >= maxstar) {
		fprintf(stderr, "iD %lu > maxstar %lu\n", iD, maxstar);
	}

	if (use_mmap) {

		memcpy(sA->farr, catalogue + iA * DIM_STARS, DIM_STARS * sizeof(double));
		memcpy(sB->farr, catalogue + iB * DIM_STARS, DIM_STARS * sizeof(double));
		memcpy(sC->farr, catalogue + iC * DIM_STARS, DIM_STARS * sizeof(double));
		memcpy(sD->farr, catalogue + iD * DIM_STARS, DIM_STARS * sizeof(double));

	} else {
		fseekocat(iA, cposmarker, catfid);
		freadstar(sA, catfid);
		fseekocat(iB, cposmarker, catfid);
		freadstar(sB, catfid);
		fseekocat(iC, cposmarker, catfid);
		freadstar(sC, catfid);
		fseekocat(iD, cposmarker, catfid);
		freadstar(sD, catfid);
	}
}

	
