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

#include "blocklist.h"

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"

#define OPTIONS "hpef:o:t:m:n:x:d:r:R:D:H:F:T:v"
const char HelpString[] =
"solvexy -f fname -o fieldname [-m agree_tol(arcsec)] [-t code_tol] [-p] [-e]\n"
"   [-n matches_needed_to_agree] [-x max_matches_needed]\n"
"   [-F maximum-number-of-field-objects-to-process]\n"
"   [-T maximum-number-of-field-quads-to-try]\n"
"   [-H hits-file-name.hits]\n"
"       (-e : compute the code errors)\n"
"   -p flips parity\n"
"   code tol is the RADIUS (not diameter or radius^2) in 4d codespace\n";

#define DEFAULT_MIN_MATCHES_TO_AGREE 3
#define DEFAULT_MAX_MATCHES_NEEDED 8
#define DEFAULT_AGREE_TOL 7.0
#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP 0

typedef struct match_struct {
    qidx quadno;
    sidx iA, iB, iC, iD;
    qidx idx;
    sidx fA, fB, fC, fD;
    double code_err;
    star *sMin, *sMax;
    double vector[6];
} MatchObj;

#define MATCH_VECTOR_SIZE 6

#define mk_MatchObj() ((MatchObj *)malloc(sizeof(MatchObj)))
#define free_MatchObj(m) free(m)

qidx solve_fields(xyarray *thefields, int maxfieldobjs, int maxtries,
				  kdtree_t *codekd, double codetol);

qidx try_all_codes(double Cx, double Cy, double Dx, double Dy, xy *cornerpix,
                   xy *ABCDpix, sidx iA, sidx iB, sidx iC, sidx iD,
				   kdtree_t *codekd, double codetol);
void resolve_matches(xy *cornerpix, kdtree_qres_t* krez, kdtree_t* codekd, double *query,
					 xy *ABCDpix, char order, sidx fA, sidx fB, sidx fC, sidx fD);
int find_matching_hit(MatchObj* mo);
void find_corners(xy *thisfield, xy *cornerpix);

void output_match(MatchObj *mo);
int output_good_matches();
int add_star_correspondences(MatchObj *mo, ivec *slist, ivec *flist);
void free_hitlist();

void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD);
void getstarcoords(star *sA, star *sB, star *sC, star *sD,
                   sidx iA, sidx iB, sidx iC, sidx iD);


char *fieldfname = NULL, *treefname = NULL, *hitfname = NULL;
char *quadfname = NULL, *catfname = NULL;
FILE *hitfid = NULL, *quadfid = NULL, *catfid = NULL;
off_t qposmarker, cposmarker;
// the largest star and quad available in the corresponding files.
sidx maxstar;
qidx maxquad;

char buff[100], maxstarWidth, oneobjWidth;

blocklist* hitlist;

double AgreeArcSec = DEFAULT_AGREE_TOL;
double AgreeTol;
qidx mostAgree;
char ParityFlip = DEFAULT_PARITY_FLIP;
unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;
unsigned int max_matches_needed = DEFAULT_MAX_MATCHES_NEEDED;

double DebuggingRAMin;
double DebuggingRAMax;
double DebuggingDecMin;
double DebuggingDecMax;
int Debugging = 0;

bool verbose = FALSE;

int nctrlcs = 0;
bool quitNow = FALSE;

extern char *optarg;
extern int optind, opterr, optopt;

void signal_handler(int sig);

int main(int argc, char *argv[]) {
    int argidx, argchar; //  opterr = 0;
    double codetol = DEFAULT_CODE_TOL;
    FILE *fieldfid = NULL, *treefid = NULL;
    qidx numfields, numquads, numsolved;
    sidx numstars;
    char readStatus;
    double index_scale, ramin, ramax, decmin, decmax;
    dimension Dim_Quads, Dim_Stars;
    xyarray *thefields = NULL;
    kdtree_t *codekd = NULL;
    int do_codeerr = 0;
    int fieldobjs = 0;
    int fieldtries = 0;

    bool use_mmap = TRUE;
    int mmapped_size = 0;
    void* mmapped = NULL;

	off_t endoffset;
	//int i;

    if (argc <= 4) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'v':
			verbose = TRUE;
			break;
		case 'd':
			Debugging++;
			DebuggingDecMin = strtod(optarg, NULL);
			break;
		case 'D':
			Debugging++;
			DebuggingDecMax = strtod(optarg, NULL);
			break;
		case 'r':
			Debugging++;
			DebuggingRAMin = strtod(optarg, NULL);
			break;
		case 'R':
			Debugging++;
			DebuggingRAMax = strtod(optarg, NULL);
			break;
		case 'F':
			fieldobjs = atoi(optarg);
			if (!fieldobjs) {
				fprintf(stderr, "Couldn't parse -F argument: %s\n",
						optarg);
				exit(-1);
			}
			break;
		case 'T':
			fieldtries = atoi(optarg);
			if (!fieldtries) {
				fprintf(stderr, "Couldn't parse -T argument: %s\n",
						optarg);
				exit(-1);
			}
			break;
		case 'e':
			do_codeerr = 1;
			break;
		case 'H':
			if (hitfname) free_fn(hitfname);
			hitfname = strdup(optarg);
			break;
		case 'f':
			treefname = mk_ctree2fn(optarg);
			quadfname = mk_quadfn(optarg);
			catfname = mk_catfn(optarg);
			break;
		case 'o':
			fieldfname = mk_fieldfn(optarg);
			if (!hitfname)
				hitfname = mk_hitfn(optarg);
			break;
		case 't':
			codetol = strtod(optarg, NULL);
			break;
		case 'm':
			AgreeArcSec = strtod(optarg, NULL);
			break;
		case 'p':
			ParityFlip = 1 - ParityFlip;
			break;
		case 'n':
			min_matches_to_agree = (unsigned int) strtol(optarg, NULL, 0);
			break;
		case 'x':
			max_matches_needed = (unsigned int) strtol(optarg, NULL, 0);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

    if (Debugging != 0 && Debugging != 4) {
		fprintf(stderr, "When debugging need all of -d -D -r -R\n");
		return (OPT_ERR);
    }

    if (treefname == NULL || fieldfname == NULL || codetol < 0) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
    }

    if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (OPT_ERR);
    }


    fprintf(stderr, "solvexy: solving fields in %s using %s\n",
			fieldfname, treefname);

    fprintf(stderr, "  Reading fields...");
    fflush(stderr);
    fopenin(fieldfname, fieldfid);
    thefields = readxy(fieldfid, ParityFlip);
    fclose(fieldfid);
    if (thefields == NULL)
		return (1);
    numfields = (qidx)thefields->size;
    fprintf(stderr, "got %lu fields.\n", numfields);
    if (ParityFlip)
		fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");

    fprintf(stderr, "  Reading code KD tree from %s...", treefname);
    fflush(stderr);
    fopenin(treefname, treefid);

    codekd = kdtree_read(treefid, use_mmap, &mmapped, &mmapped_size);
    if (!codekd)
		return (2);

	/*
	  for (i=0; i<10; i++) {
	  fprintf(stderr, "perm[%i] = %i\n", i, codekd->perm[i]);
	  }
	  for (i=0; i<10; i++) {
	  fprintf(stderr, "data[%i] = %g\n", i, codekd->data[i]);
	  }
	  for (i=0; i<256; i++) {
	  if (i && (i % 32 == 0)) {
	  fprintf(stderr, "\n");
	  }
	  fprintf(stderr, "%02x ", (unsigned int)(((unsigned char*)(codekd->tree))[i]));
	  }
	*/

    fclose(treefid);

	/*
	  for (i=0; i<10; i++) {
	  fprintf(stderr, "perm[%i] = %i\n", i, codekd->perm[i]);
	  }
	  for (i=0; i<10; i++) {
	  fprintf(stderr, "data[%i] = %g\n", i, codekd->data[i]);
	  }
	  for (i=0; i<256; i++) {
	  if (i && (i % 32 == 0)) {
	  fprintf(stderr, "\n");
	  }
	  fprintf(stderr, "%02x ", (unsigned int)(((unsigned char*)(codekd->tree))[i]));
	  }
	*/

    fprintf(stderr, "done\n    (%d quads, %d nodes, dim %d).\n",
			codekd->ndata, codekd->nnodes, codekd->ndim);
    /*
      fprintf(stderr, "    (index scale = %lf arcmin)\n", rad2arcmin(index_scale));
      if (do_codeerr) {
      fprintf(stderr, "  Making dyv_array from kdtree...\n");
      codearray = mk_dyv_array_from_kdtree(codekd);
      }
    */

    fopenin(quadfname, quadfid);
    free_fn(quadfname);
    readStatus = read_quad_header(quadfid, &numquads, &numstars, 
								  &Dim_Quads, &index_scale);
    if (readStatus == READ_FAIL)
		return (3);
    qposmarker = ftello(quadfid);

	fseeko(quadfid, 0, SEEK_END);
	endoffset = ftello(quadfid) - qposmarker;
	maxquad = endoffset / (DIM_QUADS * sizeof(sidx));

    //fopenin(catfname, catfid);
	catfid = fopen(catfname, "rb");
	if (!catfid) {
		fprintf(stderr, "Couldn't open catalogue %s for reading: %s\n",
				catfname, strerror(errno));
		exit(-1);
	}
    free_fn(catfname);
    readStatus = read_objs_header(catfid, &numstars, &Dim_Stars,
								  &ramin, &ramax, &decmin, &decmax);
    if (readStatus == READ_FAIL) {
		exit(-1);
	}
    cposmarker = ftello(catfid);

	fseeko(catfid, 0, SEEK_END);
	endoffset = ftello(catfid) - cposmarker;
	maxstar = endoffset / (DIM_STARS * sizeof(double));

	fprintf(stderr, "maxstar = %li\n", maxstar);
	fprintf(stderr, "maxquad = %li\n", maxquad);

	fprintf(stderr, "numstars = %li\n", numstars);
	fprintf(stderr, "numquads = %li\n", numquads);

    AgreeTol = sqrt(2.0) * radscale2xyzscale(arcsec2rad(AgreeArcSec));
    fprintf(stderr,
			"  Solving %lu fields (code_match_tol=%lg,agreement_tol=%lg arcsec)...\n",
			numfields, codetol, AgreeArcSec);
    fprintf(stderr, "Using HITS output file %s\n", hitfname);
    fopenout(hitfname, hitfid);
    free_fn(hitfname);
    fprintf(hitfid, "# SOLVER PARAMS:\n");
    fprintf(hitfid, "# solving fields in %s using %s\n", fieldfname, treefname);
    fprintf(hitfid, "field_to_solve = '%s'\n", fieldfname);
    fprintf(hitfid, "index_used = '%s'\n", treefname);
    free_fn(fieldfname);
    free_fn(treefname);
    /*
      fprintf(hitfid, "nfields = %lu\nnquads = %lu\nobjects_in_catalog = %lu\n",
      numfields, (qidx)kdtree_num_points(codekd), numstars);
    */
    fprintf(hitfid, "code_tol = %lf\nagree_tol = %lf\n", codetol, AgreeArcSec);
    if (ParityFlip) {
		fprintf(hitfid, "# flipping parity (swapping row/col image coordinates)\n");
		fprintf(hitfid, "parity_flip = True\n");
    }
    else {
		fprintf(hitfid, "parity_flip = False\n");
    }
    fprintf(hitfid, "min_matches_to_agree = %u\n", min_matches_to_agree);
    fprintf(hitfid, "max_matches_needed = %u\n", max_matches_needed);

    signal(SIGINT, signal_handler);

    hitlist = blocklist_pointer_new(256);

    numsolved = solve_fields(thefields, fieldobjs, fieldtries,
							 codekd, codetol);

    blocklist_pointer_free(hitlist);

    fclose(hitfid);
    fclose(quadfid);
    fclose(catfid);
    fprintf(stderr, "done (solved %lu).                                  \n",
			numsolved);

    free_xyarray(thefields);

    if (use_mmap) {
		munmap(mmapped, mmapped_size);
    } else {
		kdtree_free(codekd);
    }

    return (0);
}


void signal_handler(int sig) {
    if (sig != SIGINT) return;
	nctrlcs++;
	switch (nctrlcs) {
	case 1:
		fprintf(stderr, "Received signal SIGINT - stopping ASAP...\n");
		break;
	case 2:
		fprintf(stderr, "Hit ctrl-C again to hard-quit - warning, hits file won't be written.\n");
		break;
	default:
		exit(-1);
	}
    quitNow = TRUE;
}

qidx numtries, nummatches;

inline void try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
					  char* inbox, int maxind,
					  xy *thisfield,
					  xy *cornerpix,
					  kdtree_t* codekd,
					  double codetol) {
    int i;
    int iC, iD;
    double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
    double costheta, sintheta, scale, xxtmp;
    double xs[maxind];
    double ys[maxind];
    xy *ABCDpix;

    ABCDpix = mk_xy(DIM_QUADS);

    Ax = xy_refx(thisfield, iA);
    Ay = xy_refy(thisfield, iA);
    xy_setx(ABCDpix, 0, Ax);
    xy_sety(ABCDpix, 0, Ay);

    Bx = xy_refx(thisfield, iB);
    By = xy_refy(thisfield, iB);
    xy_setx(ABCDpix, 1, Bx);
    xy_sety(ABCDpix, 1, By);
    Bx -= Ax;
    By -= Ay;
    scale = Bx * Bx + By * By;
    costheta = (Bx + By) / scale;
    sintheta = (By - Bx) / scale;

    // check which C, D points are inside the square.
    for (i=0; i<maxind; i++) {
		if (!inbox[i]) continue;
		Cx = xy_refx(thisfield, i);
		Cy = xy_refy(thisfield, i);
		Cx -= Ax;
		Cy -= Ay;
		xxtmp = Cx;
		Cx = Cx * costheta + Cy * sintheta;
		Cy = -xxtmp * sintheta + Cy * costheta;
		if ((Cx >= 1.0) || (Cx <= 0.0) ||
			(Cy >= 1.0) || (Cy <= 0.0)) { //C inside AB box?
			inbox[i] = 0;
		} else {
			xs[i] = Cx;
			ys[i] = Cy;
		}
    }

    for (i=0; i<ncd; i++) {
		iC = iCs[i];
		iD = iDs[i];
		// are both C and D in the box?
		if (!inbox[iC]) continue;
		if (!inbox[iD]) continue;

		Cx = xs[iC];
		Cy = ys[iC];
		Dx = xs[iD];
		Dy = ys[iD];

		xy_setx(ABCDpix, 2, Cx);
		xy_sety(ABCDpix, 2, Cy);
		xy_setx(ABCDpix, 3, Dx);
		xy_sety(ABCDpix, 3, Dy);

		numtries++;
		nummatches += try_all_codes(Cx, Cy, Dx, Dy, cornerpix, ABCDpix,
									iA, iB, iC, iD, codekd, codetol);
    }

    free_xy(ABCDpix);

}

qidx solve_fields(xyarray *thefields, int maxfieldobjs, int maxtries,
				  kdtree_t *codekd, double codetol) {
    qidx numsolved, numgood, ii;
    sidx numxy, iA, iB, iC, iD, newpoint;
    xy *thisfield;
    xy *cornerpix = mk_xy(2);
    int *iCs, *iDs;
    char *iunion;
    int ncd;

    numsolved = dyv_array_size(thefields);

    fprintf(hitfid, "############################################################\n");
    fprintf(hitfid, "# Result data, stored as a list of dictionaries\n");
    fprintf(hitfid, "results = [ \n");

    for (ii=0; ii< dyv_array_size(thefields); ii++) {
		numtries = 0;
		nummatches = 0;
		mostAgree = 0;
		thisfield = xya_ref(thefields, ii);
		numxy = xy_size(thisfield);
		if (maxfieldobjs && (numxy > maxfieldobjs))
			numxy = maxfieldobjs;
		find_corners(thisfield, cornerpix);

		if (numxy < DIM_QUADS) //if there are<4 objects in field, forget it
			continue;

		iCs = (int*)malloc(numxy * numxy * sizeof(int));
		iDs = (int*)malloc(numxy * numxy * sizeof(int));
		iunion = (char*)malloc(numxy * sizeof(char));

		// We keep the invariants that iA < iB and iC < iD.
		// We try the A<->B and C<->D permutation in try_all_points.
		for (newpoint=3; newpoint<numxy; newpoint++) {
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

				try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint,
						  thisfield, cornerpix, codekd, codetol);

			}

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

					try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint+1,
							  thisfield, cornerpix, codekd, codetol);
				}
			}

			fprintf(stderr,
					"    field %lu: using %lu of %lu objects (%lu quads agree so far; %lu tried, %lu matched)      \r",
					ii, newpoint, numxy, mostAgree, numtries, nummatches);

			if ((mostAgree >= max_matches_needed) ||
				(maxtries && (numtries >= maxtries)) ||
				(quitNow))
				break;
		}
		free(iCs);
		free(iDs);
		free(iunion);


		fprintf(hitfid, "# --------------------\n");
		fprintf(hitfid, "dict(\n");
		fprintf(hitfid, "    field=%lu,\n", ii);
		fprintf(hitfid, "    objects_in_field=%lu,\n", numxy);
		fprintf(hitfid, "    # Image corners\n");
		if (ParityFlip)
			fprintf(hitfid, "    min_uv_corner=(%lf,%lf), max_uv_corner=(%lf,%lf),\n",
					xy_refy(cornerpix, 0), xy_refx(cornerpix, 0),
					xy_refy(cornerpix, 1), xy_refx(cornerpix, 1));
		else
			fprintf(hitfid, "    min_uv_corner=(%lf,%lf), max_uv_corner=(%lf,%lf),\n",
					xy_refx(cornerpix, 0), xy_refy(cornerpix, 0),
					xy_refx(cornerpix, 1), xy_refy(cornerpix, 1));
		fprintf(hitfid, "    quads_tried=%lu, codes_matched=%lu,\n",
				numtries, nummatches);

		fprintf(stderr,"\n");
		numgood = output_good_matches();

		fprintf(hitfid, "),\n");
		fflush(hitfid);
		if (numgood == 0)
			numsolved--;

		fprintf(stderr, "\n    field %lu: tried %lu quads, matched %lu codes, "
				"%lu agree\n", ii, numtries, nummatches, numgood);

		free_hitlist();
    }

    fprintf(hitfid, "] \n");
    fprintf(hitfid, "# END OF RESULTS\n");
    fprintf(hitfid, "############################################################\n");

    free_xy(cornerpix);

    return numsolved;
}

/*
  kdtree_qres_t* rangesearch(kdtree_t* tree, double* query, double tol) {
  kdtree_qres_t* result;
  int i;
  result = kdtree_rangesearch(tree, query, square(tol));
  if (!result) {
  fprintf(stderr, "NULL result from kdtree_rangesearch.\n");
  return result;
  }
  if (result->nres > 0) {
  fprintf(stderr, "%u results:\n", result->nres);
  for (i=0; i<result->nres; i++) {
  fprintf(stderr, " %u\n", result->inds[i]);
  }
  }
  return result;
  }
*/

qidx try_all_codes(double Cx, double Cy, double Dx, double Dy, xy *cornerpix,
                   xy *ABCDpix, sidx iA, sidx iB, sidx iC, sidx iD,
				   kdtree_t *codekd, double codetol) {
    double thequery[4];
    qidx nummatch = 0;
    kdtree_qres_t* result;

    // ABCD
    thequery[0] = Cx;
    thequery[1] = Cy;
    thequery[2] = Dx;
    thequery[3] = Dy;

    result = kdtree_rangesearch(codekd, thequery, square(codetol));
    //result = rangesearch(codekd, thequery, codetol);
    if (result->nres) {
		nummatch += result->nres;
		resolve_matches(cornerpix, result, codekd, thequery, ABCDpix, ABCD_ORDER, iA, iB, iC, iD);
    }
    kdtree_free_query(result);

    // BACD
    thequery[0] = 1.0 - Cx;
    thequery[1] = 1.0 - Cy;
    thequery[2] = 1.0 - Dx;
    thequery[3] = 1.0 - Dy;

    result = kdtree_rangesearch(codekd, thequery, square(codetol));
    //result = rangesearch(codekd, thequery, codetol);
    if (result->nres) {
		nummatch += result->nres;
		resolve_matches(cornerpix, result, codekd, thequery, ABCDpix, BACD_ORDER, iB, iA, iC, iD);
    }
    kdtree_free_query(result);

    // ABDC
    thequery[0] = Dx;
    thequery[1] = Dy;
    thequery[2] = Cx;
    thequery[3] = Cy;

    result = kdtree_rangesearch(codekd, thequery, square(codetol));
    //result = rangesearch(codekd, thequery, codetol);
    if (result->nres) {
		nummatch += result->nres;
		resolve_matches(cornerpix, result, codekd, thequery, ABCDpix, ABDC_ORDER, iA, iB, iD, iC);
    }
    kdtree_free_query(result);

    // BADC
    thequery[0] = 1.0 - Dx;
    thequery[1] = 1.0 - Dy;
    thequery[2] = 1.0 - Cx;
    thequery[3] = 1.0 - Cy;

    result = kdtree_rangesearch(codekd, thequery, square(codetol));
    //result = rangesearch(codekd, thequery, codetol);
    if (result->nres) {
		nummatch += result->nres;
		resolve_matches(cornerpix, result, codekd, thequery, ABCDpix, BADC_ORDER, iB, iA, iD, iC);
    }
    kdtree_free_query(result);

    return nummatch;
}

void resolve_matches(xy *cornerpix, kdtree_qres_t* krez, kdtree_t* codekd, double *query,
					 xy *ABCDpix, char order, sidx fA, sidx fB, sidx fC, sidx fD) {
    qidx jj, thisquadno;
    sidx iA, iB, iC, iD;
    double *transform;
    star *sA, *sB, *sC, *sD, *sMin, *sMax;
    MatchObj *mo;

    sA = mk_star();
    sB = mk_star();
    sC = mk_star();
    sD = mk_star();

    // This should normally only loop once; only one match should be foun
    for (jj=0; jj<krez->nres; jj++) {
		int nagree;

		mo = mk_MatchObj();

		thisquadno = (qidx)krez->inds[jj];
		getquadids(thisquadno, &iA, &iB, &iC, &iD);
		/*
		  fprintf(stderr, "iA=%lu, iB=%lu, iC=%lu, iD=%lu\n",
		  iA, iB, iC, iD);
		*/
		getstarcoords(sA, sB, sC, sD, iA, iB, iC, iD);
		transform = fit_transform(ABCDpix, order, sA, sB, sC, sD);
		sMin = mk_star();
		sMax = mk_star();
		image_to_xyz(xy_refx(cornerpix, 0), xy_refy(cornerpix, 0), sMin, transform);
		image_to_xyz(xy_refx(cornerpix, 1), xy_refy(cornerpix, 1), sMax, transform);

		mo->quadno = thisquadno;
		mo->iA = iA;
		mo->iB = iB;
		mo->iC = iC;
		mo->iD = iD;
		mo->fA = fA;
		mo->fB = fB;
		mo->fC = fC;
		mo->fD = fD;

		mo->sMin = sMin;
		mo->sMax = sMax;

		mo->vector[0] = star_ref(sMin, 0);
		mo->vector[1] = star_ref(sMin, 1);
		mo->vector[2] = star_ref(sMin, 2);
		mo->vector[3] = star_ref(sMax, 0);
		mo->vector[4] = star_ref(sMax, 1);
		mo->vector[5] = star_ref(sMax, 2);

		mo->code_err = krez->sdists[jj];

		nagree = find_matching_hit(mo);
		if (nagree >= mostAgree) {
			mostAgree = nagree;
		}

		free(transform);
    }

    free_star(sA);
    free_star(sB);
    free_star(sC);
    free_star(sD);
}

double distsq(double* d1, double* d2, int D) {
    double dist2;
    int i;
    dist2 = 0.0;
    for (i=0; i<D; i++) {
		dist2 += square(d1[i] - d2[i]);
    }
    return dist2;
}


int find_matching_hit(MatchObj* mo) {
    int i, N;

    N = blocklist_count(hitlist);

	if (verbose) {
		fprintf(stderr, "\n\nFinding matching hit to:\n");
		fprintf(stderr, " min (%g, %g, %g)\n", mo->vector[0], mo->vector[1], mo->vector[2]);
		fprintf(stderr, " max (%g, %g, %g)\n", mo->vector[3], mo->vector[4], mo->vector[5]);
		{
			double ra1, dec1, ra2, dec2;
			ra1 = xy2ra(mo->vector[0], mo->vector[1]);
			dec1 = z2dec(mo->vector[2]);
			ra2 = xy2ra(mo->vector[3], mo->vector[4]);
			dec2 = z2dec(mo->vector[5]);
			ra1  *= 180.0/M_PI;
			dec1 *= 180.0/M_PI;
			ra2  *= 180.0/M_PI;
			dec2 *= 180.0/M_PI;
			fprintf(stderr, "ra,dec (%g, %g), (%g, %g)\n",
					ra1, dec1, ra2, dec2);
		}
		fprintf(stderr, "%i other hits.\n", N);
	}

    for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);
		if (verbose)
			fprintf(stderr, "  hit %i: %i elements.\n", i, M);
		for (j=0; j<M; j++) {
			double d2;
			//double arcsec;
			MatchObj* m = (MatchObj*)blocklist_pointer_access(hits, j);
			d2 = distsq(mo->vector, m->vector, MATCH_VECTOR_SIZE);
			// DEBUG
			//arcsec = sqrt(d2) / sqrt(2.0)
			if (verbose)
				fprintf(stderr, "    el %i: dist %g (thresh %g)\n", j, sqrt(d2), AgreeTol);
			if (d2 < square(AgreeTol)) {
				blocklist_pointer_append(hits, mo);
				if (verbose)
					fprintf(stderr, "match!  (now %i agree)\n",
							blocklist_count(hits));
				return blocklist_count(hits);
			}
		}
    }

	if (verbose)
		fprintf(stderr, "no agreement.\n");

    // no agreement - create new list.
    blocklist* newlist = blocklist_pointer_new(10);
    blocklist_pointer_append(newlist, mo);
    blocklist_pointer_append(hitlist, newlist);

    return 1;
}


/*
  int add_transformed_corners(star *sMin, star *sMax, qidx thisquad) {
  int i, N;
  hitnode* node;

  node = (hitnode*)malloc(sizeof(hitnode));
  node->vector[0] = star_ref(sMin, 0);
  node->vector[1] = star_ref(sMin, 1);
  node->vector[2] = star_ref(sMin, 2);
  node->vector[3] = star_ref(sMax, 0);
  node->vector[4] = star_ref(sMax, 1);
  node->vector[5] = star_ref(sMax, 2);
  node->quad = thisquad;

  N = blocklist_count(hitlist);
  for (i=0; i<N; i++) {
  int j, M;
  blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
  M = blocklist_count(hits);
  for (j=0; j<M; j++) {
  double d2;
  hitnode* n = (hitnode*)blocklist_pointer_access(hits, j);
  d2 = distsq(node->vector, n->vector, sizeof(node->vector)/sizeof(double));
  if (d2 < square(AgreeTol)) {
  blocklist_pointer_append(hits, node);
  return blocklist_count(hits);
  }
  }
  }

  // no agreement - create new list.
  
  blocklist* newlist = blocklist_pointer_new(256);
  blocklist_pointer_append(newlist, node);
  blocklist_pointer_append(hitlist, newlist);

  return 1;
  }
*/
/*
  double dist_sq;
  dyv *hitdyv;
  int tmpmatch;
  kresult *kr;
  ivec *nearlist = NULL;

  hitdyv = mk_dyv(2 * DIM_STARS);
  dyv_set(hitdyv, 0, star_ref(sMin, 0));
  dyv_set(hitdyv, 1, star_ref(sMin, 1));
  dyv_set(hitdyv, 2, star_ref(sMin, 2));
  dyv_set(hitdyv, 3, star_ref(sMax, 0));
  dyv_set(hitdyv, 4, star_ref(sMax, 1));
  dyv_set(hitdyv, 5, star_ref(sMax, 2));

  if (*kdt == NULL) {
  dyv_array *tmp = mk_dyv_array(1);
  dyv_array_set(tmp, 0, hitdyv);
  *kdt = mk_kdtree_from_points(tmp, DEFAULT_KDRMIN);
  free_dyv_array(tmp);
  qlist = mk_ivec_1((int)thisquad);
  } else {
  dist_sq = add_point_to_kdtree_dsq(*kdt, hitdyv, &tmpmatch);
  add_to_ivec(qlist, (int)thisquad);
  if ((dist_sq < (AgreeTol*AgreeTol)) &&
  ((qidx)ivec_ref(qlist, tmpmatch) != thisquad)) {
  kr = mk_kresult_from_kquery(nearquery, *kdt, hitdyv);
  nearlist = mk_copy_ivec(kr->pindexes);
  free_kresult(kr);
  }
  }

  free_dyv(hitdyv);
  return (nearlist);
*/


void output_match(MatchObj *mo)
{
    if (mo == NULL)
		fprintf(hitfid, "        # No agreement between matches. Could not resolve field.\n");
    else {
		fprintf(hitfid, "        dict(\n");
		fprintf(hitfid, "            quad=%lu,\n", mo->quadno);
		fprintf(hitfid, "            starids_ABCD=(%lu,%lu,%lu,%lu),\n",
				mo->iA, mo->iB, mo->iC, mo->iD);
		fprintf(hitfid, "            field_objects_ABCD=(%lu,%lu,%lu,%lu),\n",
				mo->fA, mo->fB, mo->fC, mo->fD);
		fprintf(hitfid, "            min_xyz=(%lf,%lf,%lf), min_radec=(%lf,%lf),\n",
				star_ref(mo->sMin, 0), star_ref(mo->sMin, 1), star_ref(mo->sMin, 2),
				rad2deg(xy2ra(star_ref(mo->sMin, 0), star_ref(mo->sMin, 1))),
				rad2deg(z2dec(star_ref(mo->sMin, 2))));
		fprintf(hitfid, "            max_xyz=(%lf,%lf,%lf), max_radec=(%lf,%lf),\n",
				star_ref(mo->sMax, 0), star_ref(mo->sMax, 1), star_ref(mo->sMax, 2),
				rad2deg(xy2ra(star_ref(mo->sMax, 0), star_ref(mo->sMax, 1))),
				rad2deg(z2dec(star_ref(mo->sMax, 2))));
		if (mo->code_err > 0.0) {
			fprintf(hitfid, "            code_err=%lf,\n", sqrt(mo->code_err));
		}
		fprintf(hitfid, "        ),\n");

    }
    return ;

}

int inrange(double ra, double ralow, double rahigh)
{
    if (ralow < rahigh) {
		if (ra > ralow && ra < rahigh)
            return 1;
        return 0;
    }

    /* handle wraparound properly */
    if (ra < ralow && ra > rahigh)
        return 1;
    return 0;
}

int output_good_matches() {
    int i, N;
    MatchObj* mo;
    int bestnum;
	blocklist* bestlist;
	int corresp_ok = 1;
	ivec *sortidx = NULL;
	ivec *slist = NULL;
	ivec *flist = NULL;
	blocklist* allocated_list = NULL;

	bestnum = 0;
	bestlist = NULL;
    N = blocklist_count(hitlist);
	fprintf(stderr, "radecs=[");
    for (i=0; i<N; i++) {
		int M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);

		if ((M >= min_matches_to_agree) ||
			1) {
			double ra1, dec1;
			double ra2, dec2;
			int lim = 1, j;
			if (verbose) lim=M;
			for (j=0; j<lim; j++) {
				MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, j);
				/*
				  ra1 = xy2ra(0.5 * (mo->vector[0] + mo->vector[3]), 0.5 * (mo->vector[1] + mo->vector[4]));
				  dec1 = z2dec( 0.5 * (mo->vector[2] + mo->vector[5]));
				*/
				ra1 = xy2ra(mo->vector[0], mo->vector[1]);
				dec1 = z2dec(mo->vector[2]);
				ra1  *= 180.0/M_PI;
				dec1 *= 180.0/M_PI;
				ra2 = xy2ra(mo->vector[3], mo->vector[4]);
				dec2 = z2dec(mo->vector[5]);
				ra2  *= 180.0/M_PI;
				dec2 *= 180.0/M_PI;
				fprintf(stderr, "%g,%g,%g,%g;", ra1, dec1, ra2, dec2);
				//fprintf(stderr, "Match list %i: %i hits: ra,dec (%g, %g)\n", i, M, ra1, dec1);
				//fprintf(stderr, "Match list %i: %i hits: ra,dec (%g, %g)\n", i, M, ra1, dec1);
			}
		}

		if (M > bestnum) {
			bestnum = M;
			bestlist = hits;
		}
	}
	fprintf(stderr, "];\n");

	if (bestnum < min_matches_to_agree) {

		if (!Debugging) {
			return 0;
		}

		// We're debugging.
		allocated_list = blocklist_pointer_new(256);
		bestlist = allocated_list;
		for (i=0; i<N; i++) {
			int j, M;
			blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
			M = blocklist_count(hits);
			for (j=0; j<M; j++) {
				double minra, maxra, mindec, maxdec;
				double x, y, z;
				mo = (MatchObj*)blocklist_pointer_access(hits, j);
				x = dyv_ref(mo->sMin, 0);
				y = dyv_ref(mo->sMin, 1);
				z = dyv_ref(mo->sMin, 2);
				minra = xy2ra(x, y);
				mindec = z2dec(z);
				x = dyv_ref(mo->sMax, 0);
				y = dyv_ref(mo->sMax, 1);
				z = dyv_ref(mo->sMax, 2);
				maxra = xy2ra(x, y);
				maxdec = z2dec(z);

				// convert to degrees - Debugging{RA,Dec}{Min,Max} are specified in degrees.
				minra  *= 180/M_PI;
				mindec *= 180/M_PI;
				maxra  *= 180/M_PI;
				maxdec *= 180/M_PI;

				// If any of the corners are in the range, go for it
				if ( (inrange(mindec, DebuggingDecMin, DebuggingDecMax) &&
					  inrange(minra, DebuggingRAMin, DebuggingRAMax)) ||
					 (inrange(maxdec, DebuggingDecMin, DebuggingDecMax) &&
					  inrange(maxra, DebuggingRAMin, DebuggingRAMax)) ||
					 (inrange(mindec, DebuggingDecMin, DebuggingDecMax) &&
					  inrange(maxra, DebuggingRAMin, DebuggingRAMax)) ||
					 (inrange(maxdec, DebuggingDecMin, DebuggingDecMax) &&
					  inrange(minra, DebuggingRAMin, DebuggingRAMax))) {
					blocklist_pointer_append(bestlist, mo);
				}
			}
		}
		bestnum = blocklist_count(bestlist);
	}

	fprintf(hitfid, "    # %d matches agree on resolving of the field:\n", bestnum);
	fprintf(hitfid, "    matches_agree=%d,\n", bestnum);

	slist = mk_ivec(0);
	flist = mk_ivec(0);

	fprintf(hitfid, "    quads=[\n");
	for (i=0; i<bestnum; i++) {
		mo = (MatchObj*)blocklist_pointer_access(bestlist, i);
		output_match(mo);
		corresp_ok *= add_star_correspondences(mo, slist, flist);
	}
	fprintf(hitfid, "    ],\n");

	fprintf(hitfid, "    # Field Object <--> Catalogue Object Mapping Table\n");
	if (!corresp_ok) {
		fprintf(hitfid,
				"    # warning -- some matches agree on resolve but not on mapping\n");
		fprintf(hitfid,
				"    resolve_mapping_mismatch=True,\n");
	}
	sortidx = mk_sorted_ivec_indices(flist);
    
	fprintf(hitfid, "    field2catalog={\n");
	for (i= 0 ; i<slist->size; i++)
		fprintf(hitfid, "        %lu : %lu,\n",
				(sidx)ivec_ref(flist, ivec_ref(sortidx, i)),
				(sidx)ivec_ref(slist, ivec_ref(sortidx, i)));
	fprintf(hitfid, "    },\n");
	fprintf(hitfid, "    catalog2field={\n");
	for (i=0; i<slist->size; i++)
		fprintf(hitfid, "        %lu : %lu,\n",
				(sidx)ivec_ref(slist, ivec_ref(sortidx, i)),
				(sidx)ivec_ref(flist, ivec_ref(sortidx, i)));
	fprintf(hitfid, "    },\n");

	if (allocated_list) {
		blocklist_pointer_free(allocated_list);
	}

	free_ivec(slist);
	free_ivec(flist);

    return bestnum;
}


int add_one_star_corresp(sidx ii, sidx ff, ivec *slist, ivec *flist);
int add_star_correspondences(MatchObj *mo, ivec *slist, ivec *flist)
{
    int rez = 0;
    if (mo == NULL || slist == NULL || flist == NULL)
		fprintf(stderr, "ERROR (add_star_corresp) -- NULL input\n");
    else {
		rez = 1;
		rez *= add_one_star_corresp(mo->iA, mo->fA, slist, flist);
		rez *= add_one_star_corresp(mo->iB, mo->fB, slist, flist);
		rez *= add_one_star_corresp(mo->iC, mo->fC, slist, flist);
		rez *= add_one_star_corresp(mo->iD, mo->fD, slist, flist);
    }
    return (rez);
}

int add_one_star_corresp(sidx ii, sidx ff, ivec *slist, ivec *flist)
{
    int inspos_ii, inspos_ff, listsize;
    listsize = slist->size;
    inspos_ii = add_to_ivec_unique2(slist, ii);
    inspos_ff = add_to_ivec_unique2(flist, ff);
    if (inspos_ii == inspos_ff)
		return (1);
    else {
		if (inspos_ii != listsize)
			add_to_ivec(slist, ii);
		if (inspos_ff != listsize)
			add_to_ivec(flist, ff);
		return (0);
    }
}


void free_hitlist() {
    int i, N;
    N = blocklist_count(hitlist);
    for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, j);

			free_star(mo->sMin);
			free_star(mo->sMax);
			free_MatchObj(mo);
		}
		blocklist_pointer_free(hits);
    }
    blocklist_remove_all(hitlist);
}



void find_corners(xy *thisfield, xy *cornerpix)
{
    double xxtmp, yytmp;
    qidx jj;

    // find min and max coordinates in this field

    xxtmp = xy_refx(thisfield, 0);
    yytmp = xy_refy(thisfield, 0);
    xy_setx(cornerpix, 0, xxtmp);
    xy_sety(cornerpix, 0, yytmp);
    xy_setx(cornerpix, 1, xxtmp);
    xy_sety(cornerpix, 1, yytmp);

    for (jj = 0;jj < xy_size(thisfield);jj++) {
		xxtmp = xy_refx(thisfield, jj);
		yytmp = xy_refy(thisfield, jj);
		if (xxtmp < xy_refx(cornerpix, 0))
			xy_setx(cornerpix, 0, xxtmp);
		if (yytmp < xy_refy(cornerpix, 0))
			xy_sety(cornerpix, 0, yytmp);
		if (xxtmp > xy_refx(cornerpix, 1))
			xy_setx(cornerpix, 1, xxtmp);
		if (yytmp > xy_refy(cornerpix, 1))
			xy_sety(cornerpix, 1, yytmp);
    }

    return ;

}



void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD)
{
	if (thisquad >= maxquad) {
		fprintf(stderr, "thisquad %lu >= maxquad %lu\n",
				thisquad, maxquad);
	}
    fseeko(quadfid, qposmarker + thisquad*
		   (DIM_QUADS*sizeof(iA)), SEEK_SET);
    readonequad(quadfid, iA, iB, iC, iD);
    return ;
}


void getstarcoords(star *sA, star *sB, star *sC, star *sD,
                   sidx iA, sidx iB, sidx iC, sidx iD)
{
	if (iA >= maxstar) {
		fprintf(stderr, "iA %lu > maxstar %lu\n",
				iA, maxstar);
	}
	if (iB >= maxstar) {
		fprintf(stderr, "iB %lu > maxstar %lu\n",
				iB, maxstar);
	}
	if (iC >= maxstar) {
		fprintf(stderr, "iC %lu > maxstar %lu\n",
				iC, maxstar);
	}
	if (iD >= maxstar) {
		fprintf(stderr, "iD %lu > maxstar %lu\n",
				iD, maxstar);
	}

	fseekocat(iA, cposmarker, catfid);
	freadstar(sA, catfid);
	fseekocat(iB, cposmarker, catfid);
	freadstar(sB, catfid);
	fseekocat(iC, cposmarker, catfid);
	freadstar(sC, catfid);
	fseekocat(iD, cposmarker, catfid);
	freadstar(sD, catfid);
}

