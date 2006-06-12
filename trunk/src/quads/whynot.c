#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "kdtree_io.h"
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "matchfile.h"
#include "catalog.h"
#include "hitlist_healpix.h"
#include "tic.h"
#include "quadfile.h"
#include "idfile.h"
#include "intmap.h"
#include "xylist.h"
#include "rdlist.h"
#include "qidxfile.h"

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n", progname);
}

void solve_fields();

int read_parameters();

#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP FALSE

// params:
char *fieldfname = NULL, *treefname = NULL;
char *quadfname = NULL, *catfname = NULL;
char* qidxfname = NULL;
char *rdlsfname = NULL;
char* startreefname = NULL;
char *idfname = NULL;
char* matchfname = NULL;
char* xcolname;
char* ycolname;
char* racolname = "RA";
char* deccolname = "DEC";
bool parity = DEFAULT_PARITY_FLIP;
double codetol = DEFAULT_CODE_TOL;

int startdepth = 0;
int enddepth = 0;

double funits_lower = 0.0;
double funits_upper = 0.0;
double index_scale;
double index_scale_lower;
double index_scale_lower_factor = 0.0;

bool agreement = FALSE;
int nagree = 4;
int maxnagree = 0;
double agreetol = 0.0;

bool do_verify = FALSE;
int nagree_toverify = 0;
double verify_dist2 = 0.0;
int noverlap_tosolve = 0;
int noverlap_toconfirm = 0;

int threads = 1;
il* fieldlist = NULL;

matchfile* mf;

catalog* cat;
idfile* id;
quadfile* quads;
kdtree_t *codetree;
xylist* xyls;
rdlist* rdls;
kdtree_t* startree;
qidxfile* qidx;

int* inverse_perm = NULL;

int* code_inverse_perm = NULL;

double arcsec_per_pixel_lower, arcsec_per_pixel_upper;
double minAB, maxAB;

intmap* fieldtoind;
intmap* indtofield;

il* quadsinfield;

void try_code(double* code, int iA, int iB, int iC, int iD) {
	int i;
	for (i=0; i<il_size(quadsinfield); i++) {
		double* qcode = codetree->data +
			code_inverse_perm[il_get(quadsinfield, i)] * codetree->ndim;
		fprintf(stderr, "        Dist to quad %i: %g\n",
				il_get(quadsinfield, i), sqrt(distsq(qcode, code, codetree->ndim)));
	}
}

void try_all_codes(double Cx, double Cy, double Dx, double Dy,
				   uint iA, uint iB, uint iC, uint iD,
                   xy *ABCDpix) {
    double thequery[4];
    kdtree_qres_t* result;
	double tol = square(codetol);

	fprintf(stderr, "      Trying quad (%i, %i, %i, %i)\n",
			iA, iB, iC, iD);

	if ((intmap_get(fieldtoind, iA, -1) == -1) ||
		(intmap_get(fieldtoind, iB, -1) == -1) ||
		(intmap_get(fieldtoind, iC, -1) == -1) ||
		(intmap_get(fieldtoind, iD, -1) == -1)) {
		fprintf(stderr, "        Not all field objects are indexed.\n");
		return;
	}

	fprintf(stderr, "        Code is (%f,%f,%f,%f)\n", Cx, Cy, Dx, Dy);

    // ABCD
    thequery[0] = Cx;
    thequery[1] = Cy;
    thequery[2] = Dx;
    thequery[3] = Dy;

	fprintf(stderr, "        ABCD:\n");
	try_code(thequery, iA, iB, iC, iD);

    // BACD
    thequery[0] = 1.0 - Cx;
    thequery[1] = 1.0 - Cy;
    thequery[2] = 1.0 - Dx;
    thequery[3] = 1.0 - Dy;

	fprintf(stderr, "        BACD:\n");
	try_code(thequery, iB, iA, iC, iD);

    // ABDC
    thequery[0] = Dx;
    thequery[1] = Dy;
    thequery[2] = Cx;
    thequery[3] = Cy;

	fprintf(stderr, "        ABDC:\n");
	try_code(thequery, iA, iB, iD, iC);

    // BADC
    thequery[0] = 1.0 - Dx;
    thequery[1] = 1.0 - Dy;
    thequery[2] = 1.0 - Cx;
    thequery[3] = 1.0 - Cy;

	fprintf(stderr, "        BADC:\n");
	try_code(thequery, iB, iA, iD, iC);
}

inline void try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
					  char* inbox, int maxind, xy* field) {
	int i;
    int iC, iD;
    double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
	double dx, dy;
    double costheta, sintheta, scale, xxtmp;
    double xs[maxind];
    double ys[maxind];
	double fieldxs[maxind];
	double fieldys[maxind];
    xy *ABCDpix;
	int ninbox;

    Ax = xy_refx(field, iA);
    Ay = xy_refy(field, iA);
    Bx = xy_refx(field, iB);
    By = xy_refy(field, iB);

	dx = Bx - Ax;
	dy = By - Ay;
	scale = dx*dx + dy*dy;

	if ((scale < square(minAB)) || (scale > square(maxAB))) {
		fprintf(stderr, "    Quad scale %g: not in allowable range [%g, %g].\n", scale, minAB, maxAB);
		return;
	}

    ABCDpix = mk_xy(DIM_QUADS);

    xy_setx(ABCDpix, 0, Ax);
    xy_sety(ABCDpix, 0, Ay);
    xy_setx(ABCDpix, 1, Bx);
    xy_sety(ABCDpix, 1, By);

    costheta = (dx + dy) / scale;
    sintheta = (dy - dx) / scale;

    // check which C, D points are inside the square.
	ninbox = 0;
    for (i=0; i<maxind; i++) {
		if (!inbox[i]) continue;
		Cx = xy_refx(field, i);
		Cy = xy_refy(field, i);

		fieldxs[i] = Cx;
		fieldys[i] = Cy;

		Cx -= Ax;
		Cy -= Ay;
		xxtmp = Cx;
		Cx = Cx * costheta + Cy * sintheta;
		Cy = -xxtmp * sintheta + Cy * costheta;
		if ((Cx >= 1.0) || (Cx <= 0.0) ||
			(Cy >= 1.0) || (Cy <= 0.0)) {
			inbox[i] = 0;
		} else {
			xs[i] = Cx;
			ys[i] = Cy;
			ninbox++;
		}
    }

	fprintf(stderr, "    %i points are in the box defined by (%i, %i)\n",
			iA, iB, ninbox);

    for (i=0; i<ncd; i++) {
		double Cfx, Cfy, Dfx, Dfy;
		iC = iCs[i];
		iD = iDs[i];
		// are both C and D in the box?
		if (!inbox[iC]) continue;
		if (!inbox[iD]) continue;

		Cx = xs[iC];
		Cy = ys[iC];
		Dx = xs[iD];
		Dy = ys[iD];

		Cfx = fieldxs[iC];
		Cfy = fieldys[iC];
		Dfx = fieldxs[iD];
		Dfy = fieldys[iD];

		xy_setx(ABCDpix, 2, Cfx);
		xy_sety(ABCDpix, 2, Cfy);
		xy_setx(ABCDpix, 3, Dfx);
		xy_sety(ABCDpix, 3, Dfy);

		try_all_codes(Cx, Cy, Dx, Dy, iA, iB, iC, iD, ABCDpix);
    }

    free_xy(ABCDpix);
}

// find min and max coordinates in this field;
// place them in "cornerpix"
void find_corners(xy *thisfield, xy *cornerpix) {
	double minx, maxx, miny, maxy;
	double x, y;
	uint i;

	minx = miny = 1e308;
	maxx = maxy = -1e308;
	
	for (i=0; i<xy_size(thisfield); i++) {
		x = xy_refx(thisfield, i);
		y = xy_refy(thisfield, i);
		if (x < minx) minx = x;
		if (x > maxx) maxx = x;
		if (y < miny) miny = y;
		if (y > maxy) maxy = y;
	}

    xy_setx(cornerpix, 0, minx);
    xy_sety(cornerpix, 0, miny);
    xy_setx(cornerpix, 1, maxx);
    xy_sety(cornerpix, 1, maxy);
}

void why_not() {
	double last_utime, last_stime;
	double utime, stime;
	int nfields;
	int fieldind;
	xy* cornerpix = mk_xy(2);

	get_resource_stats(&last_utime, &last_stime, NULL);

	/*
	  solver_default_params(&solver);
	  solver.codekd = codetree;
	  solver.endobj = enddepth;
	  solver.maxtries = 0;
	  solver.max_matches_needed = maxnagree;
	  solver.codetol = codetol;
	  solver.cornerpix = mk_xy(2);
	  solver.handlehit = handlehit;
	*/

	if (funits_upper != 0.0) {
		arcsec_per_pixel_upper = funits_upper;
		minAB = index_scale_lower / funits_upper;
		fprintf(stderr, "Set minAB to %g\n", minAB);
	}
	if (funits_lower != 0.0) {
		arcsec_per_pixel_lower = funits_lower;
		maxAB = index_scale / funits_lower;
		fprintf(stderr, "Set maxAB to %g\n", maxAB);
	}

	nfields = xyls->nfields;

	for (fieldind=0; fieldind<nfields; fieldind++) {
		xy *thisfield = NULL;
		int fieldnum;
		rd* thisrd;

		double nearbyrad = 3.0;
		double nearbyd2;
		int i;

		uint numxy, iA, iB, iC, iD, newpoint;
		int *iCs, *iDs;
		char *iunion;
		int ncd;

		il* indstars;
		il* indquads;

		fieldnum = il_get(fieldlist, fieldind);
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			continue;
		}
		thisfield = xylist_get_field(xyls, fieldnum);
		thisrd = rdlist_get_field(rdls, fieldnum);
		if (!thisfield || !thisrd) {
			fprintf(stderr, "Couldn't get xylist or rdlist for field %i\n", fieldnum);
			continue;
		}

		assert(xy_size(thisfield) == rd_size(thisrd));

		intmap_clear(fieldtoind);
		intmap_clear(indtofield);

		indstars = il_new(16);
		indquads = il_new(16);

		// for each object in the field, find nearby stars in the index.
		nearbyd2 = arcsec2distsq(nearbyrad);
		for (i=0; i<rd_size(thisrd); i++) {
			double xyz[3];
			double ra, dec;
			kdtree_qres_t* res;
			ra = rd_refra(thisrd, i);
			dec = rd_refdec(thisrd, i);
			radec2xyzarr(deg2rad(ra), deg2rad(dec), xyz);
			res = kdtree_rangesearch(startree, xyz, nearbyd2);
			assert(res);
			assert(res->nres == 0 || res->nres == 1);
			if (!res->nres)
				continue;

			if (intmap_add(fieldtoind, i, res->inds[0])) {
				fprintf(stderr, "Conflicting mapping (f->i): %i <-> %i\n", i, res->inds[0]);
			}
			if (intmap_add(indtofield, res->inds[0], i)) {
				fprintf(stderr, "Conflicting mapping (i->f): %i <-> %i\n", i, res->inds[0]);
			}
			il_append(indstars, res->inds[0]);
		}

		fprintf(stderr, "Of %i field stars, %i have nearby index stars.\n",
				rd_size(thisrd), intmap_length(fieldtoind));

		fprintf(stderr, "Field stars that are indexed:\n");
		for (i=0; i<intmap_length(fieldtoind); i++) {
			int fld;
			intmap_get_entry(fieldtoind, i, &fld, NULL);
			fprintf(stderr, "%i ", fld);
		}
		fprintf(stderr, "\n");
		
		for (i=0; i<il_size(indstars); i++) {
			uint* quads, nquads;
			uint star = il_get(indstars, i);
			int j;
			if (qidxfile_get_quads(qidx, star, &quads, &nquads)) {
				fprintf(stderr, "Failed to read qidxfile for star %i.\n", star);
				exit(-1);
			}
			fprintf(stderr, "Index star %i (field %i) is in quads:\n  ",
					star, intmap_get(indtofield, star, -1));
			for (j=0; j<nquads; j++) {
				fprintf(stderr, "%i ", quads[j]);
			}
			fprintf(stderr, "\n");

			for (j=0; j<nquads; j++) {
				il_insert_ascending(indquads, quads[j]);
			}
		}

		/*
		  fprintf(stderr, "Partially indexed quads:\n  ");
		  for (i=0; i<il_size(indquads); i++) {
		  fprintf(stderr, "%i ", il_get(indquads, i));
		  }
		  fprintf(stderr, "\n");
		*/

		for (i=0; i<il_size(indquads)-3; i++) {
			if (il_get(indquads, i) != il_get(indquads, i+3)) {
				// not a string of four equal quads.
				il_remove(indquads, i);
				i--;
				continue;
			}
		}

		fprintf(stderr, "Index-to-field map:\n");
		for (i=0; i<intmap_length(indtofield); i++) {
			int from, to;
			intmap_get_entry(indtofield, i, &from, &to);
			fprintf(stderr, "  %i -> %i\n", from, to);
		}

		fprintf(stderr, "Indexed quads:\n");
		for (i=0; i<il_size(indquads); i++) {
			int quad = il_get(indquads, i);
			uint sA, sB, sC, sD;
			quadfile_get_starids(quads, quad, &sA, &sB, &sC, &sD);
			fprintf(stderr, "   %i: index (%i, %i, %i, %i), field (%i, %i, %i, %i)\n",
					quad, sA, sB, sC, sD,
					intmap_get(indtofield, sA, -1),
					intmap_get(indtofield, sB, -1),
					intmap_get(indtofield, sC, -1),
					intmap_get(indtofield, sD, -1));
		}
		fprintf(stderr, "\n");

		quadsinfield = indquads;

		/*
		  solver.fieldnum = fieldnum;
		  solver.numtries = 0;
		  solver.nummatches = 0;
		  solver.mostagree = 0;
		  solver.startobj = startdepth;
		  solver.field = thisfield;
		  solver.quitNow = FALSE;
		  solver.userdata = my;
		*/

		find_corners(thisfield, cornerpix);

		numxy = xy_size(thisfield);

		iCs = malloc(numxy * numxy * sizeof(int));
		iDs = malloc(numxy * numxy * sizeof(int));
		iunion = malloc(numxy * sizeof(char));

		for (newpoint=0; newpoint<numxy; newpoint++) {
			if (intmap_get(fieldtoind, newpoint, -1) == -1) {
				fprintf(stderr, "Field object %i is not in the index - skipping.\n", newpoint);
				continue;
			}
			fprintf(stderr, "Making quads with object %i.\n", newpoint);

			// quads with the new star on the diagonal:
			iB = newpoint;
			for (iA=0; iA<newpoint; iA++) {
				ncd = 0;
				memset(iunion, 0, newpoint);
				for (iC=0; iC<newpoint; iC++) {
					if ((iC == iA) || (iC == iB))
						continue;
					iunion[iC] = 1;
					for (iD=iC+1; iD<newpoint; iD++) {
						if ((iD == iA) || (iD == iB))
							continue;
						iCs[ncd] = iC;
						iDs[ncd] = iD;
						ncd++;
					}
				}
				fprintf(stderr, "  Trying %i quads with newpoint on the diagonal (%i, %i).\n", ncd, newpoint, iA);
				try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint, thisfield);
			}
			// quads with the new star not on the diagonal:
			iD = newpoint;
			for (iA=0; iA<newpoint; iA++) {
				for (iB=iA+1; iB<newpoint; iB++) {
					ncd = 0;
					memset(iunion, 0, newpoint+1);
					iunion[iD] = 1;
					for (iC=0; iC<newpoint; iC++) {
						if ((iC == iA) || (iC == iB))
							continue;
						iunion[iC] = 1;
						iCs[ncd] = iC;
						iDs[ncd] = iD;
						ncd++;
					}
					fprintf(stderr, "  Trying %i quads with newpoint not on the diagonal (diag %i, %i).\n", ncd, iA, iB);
					try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint+1, thisfield);
				}
			}
		}

		free(iCs);
		free(iDs);
		free(iunion);

		free_xy(thisfield);
		free_xy(thisrd);

		il_free(indstars);
		il_free(indquads);

		get_resource_stats(&utime, &stime, NULL);
		fprintf(stderr, "    spent %g s user, %g s system, %g s total.\n",
				(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime));
		last_utime = utime;
		last_stime = stime;
	}
}

/*
  char* get_pathname(char* fname) {
  char* resolved = malloc(PATH_MAX);
  if (!realpath(fname, resolved)) {
  fprintf(stderr, "Couldn't resolve real path of %s: %s\n", fname, strerror(errno));
  return NULL;
  }
  resolved = realloc(resolved, strlen(resolved) + 1);
  return resolved;
  }
*/

int main(int argc, char *argv[]) {
    uint numfields;
	char* progname = argv[0];
	int i;

    if (argc != 1) {
		printHelp(progname);
		exit(-1);
    }

	printf("Running on host:\n");
	fflush(stdout);
	system("hostname");
	printf("\n");
	fflush(stdout);

	fieldlist = il_new(256);

	fieldtoind = intmap_new();
	indtofield = intmap_new();

	qfits_err_statset(1);

	for (;;) {
		
		tic();

		fieldfname = NULL;
		treefname = NULL;
		startreefname = NULL;
		quadfname = NULL;
		catfname = NULL;
		idfname = NULL;
		matchfname = NULL;
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
		quads = NULL;
		startree = NULL;
		xcolname = strdup("ROWC");
		ycolname = strdup("COLC");
		verify_dist2 = 0.0;
		nagree_toverify = 0;
		noverlap_toconfirm = 0;
		noverlap_tosolve = 0;

		if (read_parameters()) {
			break;
		}

		if (agreement && (agreetol <= 0.0)) {
			fprintf(stderr, "If you set 'agreement', you must set 'agreetol'.\n");
			exit(-1);
		}

		fprintf(stderr, "%s params:\n", progname);
		fprintf(stderr, "fieldfname %s\n", fieldfname);
		fprintf(stderr, "rdlsfname %s\n", fieldfname);
		fprintf(stderr, "treefname %s\n", treefname);
		fprintf(stderr, "startreefname %s\n", startreefname);
		fprintf(stderr, "quadfname %s\n", quadfname);
		fprintf(stderr, "catfname %s\n", catfname);
		fprintf(stderr, "idfname %s\n", idfname);
		fprintf(stderr, "matchfname %s\n", matchfname);
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
		fprintf(stderr, "nagree_toverify %i\n", nagree_toverify);
		fprintf(stderr, "noverlap_toconfirm %i\n", noverlap_toconfirm);
		fprintf(stderr, "noverlap_tosolve %i\n", noverlap_tosolve);
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

		// Read .xyls file...
		fprintf(stderr, "Reading fields file %s...", fieldfname);
		fflush(stderr);
		xyls = xylist_open(fieldfname);
		if (!xyls) {
			fprintf(stderr, "Failed to read xylist.\n");
			exit(-1);
		}
		numfields = xyls->nfields;
		fprintf(stderr, "got %u fields.\n", numfields);
		if (parity) {
			fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");
			xyls->parity = 1;
		}
		xyls->xname = xcolname;
		xyls->yname = ycolname;

		// Read .rdls file...
		fprintf(stderr, "Reading fields file %s...", rdlsfname);
		fflush(stderr);
		rdls = rdlist_open(rdlsfname);
		if (!rdls) {
			fprintf(stderr, "Failed to read rdls file.\n");
			exit(-1);
		}
		rdls->xname = racolname;
		rdls->yname = deccolname;

		// Read .ckdt file...
		fprintf(stderr, "Reading code KD tree from %s...", treefname);
		fflush(stderr);
		codetree = kdtree_fits_read_file(treefname);
		if (!codetree)
			exit(-1);
		fprintf(stderr, "done\n    (%d quads, %d nodes, dim %d).\n",
				codetree->ndata, codetree->nnodes, codetree->ndim);

		fprintf(stderr, "Computing inverse permutation...\n");
		fflush(stderr);
		code_inverse_perm = malloc(codetree->ndata * sizeof(int));
		for (i=0; i<codetree->ndata; i++)
			code_inverse_perm[codetree->perm[i]] = i;

		// Read .qidx file...
		fprintf(stderr, "Reading qidxfile %s...", qidxfname);
		fflush(stderr);
		qidx = qidxfile_open(qidxfname, 0);
		if (!qidx) {
			fprintf(stderr, "Failed to open qidx file.\n");
			exit(-1);
		}

		// Read .quad file...
		fprintf(stderr, "Reading quads file %s...\n", quadfname);
		quads = quadfile_open(quadfname, 0);
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

		// Read .skdt file...
		fprintf(stderr, "Reading star KD tree from %s...", startreefname);
		fflush(stderr);
		startree = kdtree_fits_read_file(startreefname);
		if (!startree)
			fprintf(stderr, "Star kdtree not found or failed to read.\n");
		else
			fprintf(stderr, "done\n");

		do_verify = startree && (verify_dist2 > 0.0);

		if (startree) {
			inverse_perm = malloc(startree->ndata * sizeof(int));
			for (i=0; i<startree->ndata; i++)
				inverse_perm[startree->perm[i]] = i;
			cat = NULL;
		} else {
			// Read .objs file...
			cat = catalog_open(catfname, 0);
			if (!cat) {
				fprintf(stderr, "Couldn't open catalog %s.\n", catfname);
				exit(-1);
			}
		}

		id = idfile_open(idfname, 0);
		if (!id) {
			fprintf(stderr, "Couldn't open id file %s.\n", idfname);
			//exit(-1);
		}

		// Do it!

		why_not();



		free(matchfname);

		free(xcolname);
		free(ycolname);

		xylist_close(xyls);

		matchfile_close(mf);

		free_fn(fieldfname);

		free_fn(treefname);
		free_fn(quadfname);
		free_fn(catfname);
		free_fn(idfname);
		free_fn(startreefname);

		kdtree_close(codetree);
		if (startree)
			kdtree_close(startree);
		if (cat)
			catalog_close(cat);
		if (inverse_perm)
			free(inverse_perm);
		if (id)
			idfile_close(id);
		quadfile_close(quads);

		toc();
	}

	il_free(fieldlist);

	intmap_free(fieldtoind);
	intmap_free(indtofield);

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
					"    maxnagree <max-to-agree>\n"
					"    agreetol <agreement-tolerance (arcsec)>\n"
					"    verify_dist <early-verification-dist (arcsec)>\n"
					"    nagree_toverify <nagree>\n"
					"    noverlap_tosolve <noverlap>\n"
					"    noverlap_toconfirm <noverlap>\n"
					"    run\n"
					"    help\n"
					"    quit\n");
		} else if (is_word(buffer, "xcol ", &nextword)) {
			xcolname = strdup(nextword);
		} else if (is_word(buffer, "ycol ", &nextword)) {
			ycolname = strdup(nextword);
		} else if (is_word(buffer, "racol ", &nextword)) {
			racolname = strdup(nextword);
		} else if (is_word(buffer, "deccol ", &nextword)) {
			deccolname = strdup(nextword);
		} else if (is_word(buffer, "agreement", &nextword)) {
			agreement = TRUE;
		} else if (is_word(buffer, "threads ", &nextword)) {
			threads = atoi(nextword);
		} else if (is_word(buffer, "nagree ", &nextword)) {
			nagree = atoi(nextword);
		} else if (is_word(buffer, "maxnagree ", &nextword)) {
			maxnagree = atoi(nextword);
		} else if (is_word(buffer, "agreetol ", &nextword)) {
			agreetol = atof(nextword);
		} else if (is_word(buffer, "index ", &nextword)) {
			char* fname = nextword;
			qidxfname = mk_qidxfn(fname);
			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			catfname = mk_catfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);
		} else if (is_word(buffer, "verify_dist ", &nextword)) {
			double d = atof(nextword);
			verify_dist2 = arcsec2distsq(d);
		} else if (is_word(buffer, "nagree_toverify ", &nextword)) {
			nagree_toverify = atoi(nextword);
		} else if (is_word(buffer, "noverlap_tosolve ", &nextword)) {
			noverlap_tosolve = atoi(nextword);
		} else if (is_word(buffer, "noverlap_toconfirm ", &nextword)) {
			noverlap_toconfirm = atoi(nextword);
		} else if (is_word(buffer, "field ", &nextword)) {
			char* fname = nextword;
			fieldfname = mk_fieldfn(fname);
		} else if (is_word(buffer, "rdls ", &nextword)) {
			rdlsfname = strdup(nextword);
		} else if (is_word(buffer, "match ", &nextword)) {
			char* fname = nextword;
			matchfname = strdup(fname);
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

