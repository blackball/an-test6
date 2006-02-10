/**
 *   Solve fields (using Keir's kdtrees)
 *
 * Inputs: .ckdt2 .objs .quad
 * Output: .hits
 */

#include <sys/mman.h>
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
int enddepth = -1;
blocklist* fieldlist = NULL;

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

int main(int argc, char *argv[]) {
    FILE *fieldfid = NULL, *treefid = NULL;
    qidx numfields, numquads;
    sidx numstars;
    char readStatus;
    double index_scale, ramin, ramax, decmin, decmax;
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

    if (argc != 1) {
		printHelp(progname);
		exit(-1);
    }

	fieldlist = blocklist_int_new(256);

	if (read_parameters()) {
		exit(-1);
	}

	fprintf(stderr, "%s params:", progname);
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
	matchfile.indexpath = treefname;
	matchfile.fieldpath = fieldfname;
	matchfile.codetol = codetol;

	solve_fields(thefields, codekd);

	if (donefname) {
		FILE* batchfid;
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
					"    field <field-file-name>\n"
					"    match <match-file-name>\n"
					"    fields <field-number> [<field-number> ...]\n"
					"    sdepth <start-field-object>\n"
					"    depth <end-field-object>\n"
					"    parity <0 || 1>\n"
					"    tol <code-tolerance>\n"
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
		} else if (strncmp(buffer, "fields ", 7) == 0) {
			char* str = buffer + 7;
			char* endp;
			for (;;) {
				int fld = strtol(str, &endp, 10);
				if (str == endp) {
					// non-numeric value
					fprintf(stderr, "\nCouldn't parse: %.20s [etc]\n", str);
					break;
				}
				blocklist_int_append(fieldlist, fld);
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

	solver.codekd = codekd;
	solver.endobj = enddepth;
	solver.maxtries = 0;
	solver.max_matches_needed = 0;
	solver.codetol = codetol;
	solver.cornerpix = mk_xy(2);
	solver.handlehit = handlehit;
	solver.quitNow = FALSE;

	for (i=0; i<blocklist_count(fieldlist); i++) {
		xy *thisfield;
		int fieldnum;

		fieldnum = blocklist_int_access(fieldlist, i);
		thisfield = xya_ref(thefields, fieldnum);
		if (!thisfield) {
			fprintf(stderr, "Field %i is null.\n", fieldnum);
			continue;
		}
		//numxy = xy_size(thisfield);

		matchfile.fieldnum = fieldnum;

		solver.numtries = 0;
		solver.nummatches = 0;
		solver.mostagree = 0;
		solver.startobj = startdepth;
		solver.field = thisfield;

		solve_field(&solver);

		fprintf(stderr, "\n    field %i: tried %i quads, matched %i codes.\n",
				fieldnum, solver.numtries, solver.nummatches);
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

	
