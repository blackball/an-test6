/**
 *   Solve fields (using Keir's kdtrees)
 *
 * Inputs: .ckdt2 .objs .quad
 * Output: .hits
 */

#include <sys/mman.h>
#include <stdio.h>

#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"

#define OPTIONS "hpef:o:t:m:n:x:H:F:T:"
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

qidx solve_fields(xyarray *thefields, int maxfieldobjs, int maxtries,
		  kdtree_t *codekd, double codetol);

qidx try_all_codes(double Cx, double Cy, double Dx, double Dy, xy *cornerpix,
                   xy *ABCDpix, sidx iA, sidx iB, sidx iC, sidx iD,
		   kdtree_t *codekd, double codetol);
void resolve_matches(xy *cornerpix, kdtree_qres_t* krez, kdtree_t* codekd, double *query,
		     xy *ABCDpix, char order, sidx fA, sidx fB, sidx fC, sidx fD);
ivec *add_transformed_corners(star *sMin, star *sMax, qidx thisquad);
void find_corners(xy *thisfield, xy *cornerpix);

void output_match(MatchObj *mo);
int output_good_matches(MatchObj *first, MatchObj *last);
int add_star_correspondences(MatchObj *mo, ivec *slist, ivec *flist);
void free_matchlist(MatchObj *first);

void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD);
void getstarcoords(star *sA, star *sB, star *sC, star *sD,
                   sidx iA, sidx iB, sidx iC, sidx iD);


char *fieldfname = NULL, *treefname = NULL, *hitfname = NULL;
char *quadfname = NULL, *catfname = NULL;
FILE *hitfid = NULL, *quadfid = NULL, *catfid = NULL;
off_t qposmarker, cposmarker;
char buff[100], maxstarWidth, oneobjWidth;

//kdtree *hitkd = NULL;
//kquery *nearquery;
ivec *qlist = NULL;
MatchObj *lastMatch = NULL;
MatchObj *firstMatch = NULL;
double AgreeArcSec = DEFAULT_AGREE_TOL;
double AgreeTol;
qidx mostAgree;
char ParityFlip = DEFAULT_PARITY_FLIP;
unsigned int min_matches_to_agree = DEFAULT_MIN_MATCHES_TO_AGREE;
unsigned int max_matches_needed = DEFAULT_MAX_MATCHES_NEEDED;

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

    if (argc <= 4) {
	fprintf(stderr, HelpString);
	return (OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
	switch (argchar) {
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
    fclose(treefid);
    if (!codekd)
	return (2);
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

    fopenin(catfname, catfid);
    free_fn(catfname);
    readStatus = read_objs_header(catfid, &numstars, &Dim_Stars,
				  &ramin, &ramax, &decmin, &decmax);
    if (readStatus == READ_FAIL)
	return (4);
    cposmarker = ftello(catfid);

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

    numsolved = solve_fields(thefields, fieldobjs, fieldtries,
			     codekd, codetol);

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
    fprintf(stderr, "Received signal SIGINT - stopping ASAP...\n");
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

    /*
      kquery *kq = mk_kquery("rangesearch", "", KD_UNDEF, codetol, kdtree_rmin(codekd));
      nearquery = mk_kquery("rangesearch", "", KD_UNDEF, AgreeTol, DEFAULT_KDRMIN);
    */
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

	numgood = output_good_matches(firstMatch, lastMatch);

	fprintf(hitfid, "),\n");
	fflush(hitfid);
	if (numgood == 0)
	    numsolved--;

	fprintf(stderr, "    field %lu: tried %lu quads, matched %lu codes, "
		"%lu agree\n", ii, numtries, nummatches, numgood);

	/*
	  if (hitkd != NULL) {
	  free_kdtree(hitkd);
	  hitkd = NULL;
	  }
	*/
	if (qlist != NULL) {
	    free_ivec(qlist);
	    qlist = NULL;
	}
	free_matchlist(firstMatch);
	firstMatch = NULL;
	lastMatch = NULL;

    }

    fprintf(hitfid, "] \n");
    fprintf(hitfid, "# END OF RESULTS\n");
    fprintf(hitfid, "############################################################\n");

    /*
      free_kquery(kq);
      free_kquery(nearquery);
    */
    free_xy(cornerpix);

    return numsolved;
}


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

    result = kdtree_rangesearch(codekd, thequery, codetol);
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

    result = kdtree_rangesearch(codekd, thequery, codetol);
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

    result = kdtree_rangesearch(codekd, thequery, codetol);
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

    result = kdtree_rangesearch(codekd, thequery, codetol);
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
      mo = mk_MatchObj();
      mo->next = NULL;
      if (firstMatch == NULL) {
	  mo->idx = 0;
	  firstMatch = mo;
      } else {
	  mo->idx = (lastMatch->idx) + 1;
	  lastMatch->next = mo;
      }
      lastMatch = mo;

      thisquadno = (qidx)krez->inds[jj];

      getquadids(thisquadno, &iA, &iB, &iC, &iD);
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
      //mo->nearlist = add_transformed_corners(sMin, sMax, thisquadno, &hitkd);
      mo->nearlist = add_transformed_corners(sMin, sMax, thisquadno);
      if (mo->nearlist != NULL && mo->nearlist->size >= min_matches_to_agree
	  && mo->nearlist->size > mostAgree)
	  mostAgree = mo->nearlist->size;

      mo->code_err = krez->sdists[jj];

      free(transform);
      //free_star(sMin); free_star(sMax); // will be freed with MatchObj
  }

  free_star(sA);
  free_star(sB);
  free_star(sC);
  free_star(sD);
  return ;
}

ivec *add_transformed_corners(star *sMin, star *sMax, qidx thisquad) {
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
    return NULL;
}


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

int output_good_matches(MatchObj *first, MatchObj *last)
{
    MatchObj *mo, *bestone, **plist;
    int ii, bestidx, bestnum = 0;
    int nummatches;
    if (first == NULL || last == NULL)
	return (0);
    nummatches = last->idx + 1;

    plist = (MatchObj **)malloc(nummatches * sizeof(MatchObj *));
    mo = first;
    ii = 0;
    bestone = NULL;
    while (mo != NULL) {
	*(plist + ii) = mo;
	if (mo->nearlist != NULL && (mo->nearlist->size) > bestnum) {
	    bestnum = mo->nearlist->size;
	    bestidx = ii;
	    bestone = mo;
	}
	mo = mo->next;
	ii++;
    }

    if (bestnum < min_matches_to_agree) {
	bestnum = 0;
	bestone = NULL;
    }

    if (bestone == NULL)
	output_match(NULL);
    else {
	int corresp_ok = 1;
	ivec *sortidx = NULL;
	ivec *slist = mk_ivec(0);
	ivec *flist = mk_ivec(0);
	ivec *bestlist = mk_copy_ivec(bestone->nearlist);
	for (ii = 0;ii < bestnum;ii++)
	    if ((*(plist + ivec_ref(bestone->nearlist, ii)))->nearlist != NULL)
		ivec_union((*(plist + ivec_ref(bestone->nearlist, ii)))->nearlist, bestlist);

	bestnum = bestlist->size;
	fprintf(hitfid, "    # %d matches agree on resolving of the field:\n", bestnum);
	fprintf(hitfid, "    matches_agree=%d,\n", bestnum);

	// Output quad data
	fprintf(hitfid, "    quads=[\n");
	for (ii = 0;ii < bestnum;ii++) {
	    output_match(*(plist + ivec_ref(bestlist, ii)));
	    corresp_ok *=
		add_star_correspondences(*(plist + ivec_ref(bestlist, ii)), slist, flist);
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
	for (ii = 0;ii < slist->size;ii++)
	    fprintf(hitfid, "        %lu : %lu,\n",
		    (sidx)ivec_ref(flist, ivec_ref(sortidx, ii)),
		    (sidx)ivec_ref(slist, ivec_ref(sortidx, ii)));
	fprintf(hitfid, "    },\n");
	fprintf(hitfid, "    catalog2field={\n");
	for (ii = 0;ii < slist->size;ii++)
	    fprintf(hitfid, "        %lu : %lu,\n",
		    (sidx)ivec_ref(slist, ivec_ref(sortidx, ii)),
		    (sidx)ivec_ref(flist, ivec_ref(sortidx, ii)));
	fprintf(hitfid, "    },\n");

	free_ivec(slist);
	free_ivec(flist);
	free_ivec(sortidx);
	free_ivec(bestlist);
    }

    free(plist);

    return (bestnum);
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



void free_matchlist(MatchObj *first)
{
    MatchObj *mo, *tmp;
    mo = first;
    while (mo != NULL) {
	free_star(mo->sMin);
	free_star(mo->sMax);
	if (mo->nearlist != NULL)
	    free_ivec(mo->nearlist);
	tmp = mo->next;
	free_MatchObj(mo);
	mo = tmp;
    }
    return ;
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
    fseeko(quadfid, qposmarker + thisquad*
	   (DIM_QUADS*sizeof(iA)), SEEK_SET);
    readonequad(quadfid, iA, iB, iC, iD);
    return ;
}


void getstarcoords(star *sA, star *sB, star *sC, star *sD,
                   sidx iA, sidx iB, sidx iC, sidx iD)
{
    fseekocat(iA, cposmarker, catfid);
    freadstar(sA, catfid);
    fseekocat(iB, cposmarker, catfid);
    freadstar(sB, catfid);
    fseekocat(iC, cposmarker, catfid);
    freadstar(sC, catfid);
    fseekocat(iD, cposmarker, catfid);
    freadstar(sD, catfid);
    return ;
}

