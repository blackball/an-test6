#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define NO_KD_INCLUDES 1
#include "fileutil.h"
#include "mathutil.h"
#include "blocklist.h"
#include "kdtree/kdtree.h"

#include "solver2.h"
#include "solver2_callbacks.h"

void find_corners(xy *thisfield, xy *cornerpix);

inline void try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
					  char* inbox, int maxind, solver_params* params);

void try_all_codes(double Cx, double Cy, double Dx, double Dy,
				   sidx iA, sidx iB, sidx iC, sidx iD,
                   xy *ABCDpix, solver_params* params);

void resolve_matches(kdtree_qres_t* krez, double *query, xy *ABCDpix,
					 char order, sidx fA, sidx fB, sidx fC, sidx fD,
					 solver_params* params);

int solver_add_hit(blocklist* hitlist, MatchObj* mo, double AgreeTol);



void solve_field(solver_params* params) {
    sidx numxy, iA, iB, iC, iD, newpoint;
    int *iCs, *iDs;
    char *iunion;
    int ncd;

	numxy = xy_size(params->field);
	if (params->endobj && (numxy > params->endobj))
		numxy = params->endobj;

	find_corners(params->field, params->cornerpix);

	if (numxy < DIM_QUADS) //if there are<4 objects in field, forget it
		return;

	iCs = (int*)malloc(numxy * numxy * sizeof(int));
	iDs = (int*)malloc(numxy * numxy * sizeof(int));
	iunion = (char*)malloc(numxy * sizeof(char));

	/*
	  Each time through the "for" loop below, we consider a new
	  star ("newpoint").  First, we try building all quads that
	  have the new star on the diagonal (star B).  Then, we try
	  building all quads that have the star not on the diagonal
	  (star D).

	  In each of the loops, we first consider a pair of A, B stars,
	  and gather all the stars that could be C and D stars.  We add
	  the indices of the C and D stars to the arrays "iCs" and "iDs".
	  That is, iCs[0] and iDs[0] contain the first pair of C,D stars.
	  Both arrays can contain duplicates.

	  We also mark "iunion" of these indices to TRUE, so if index "i"
	  appears in "iCs" or "iDs", then "iunion[i]" is true.

	  We then call "try_quads" with stars A,B and the sets of stars
	  C,D.  For each element in "iunion" that is true, "try_quads"
	  checks whether that star is inside the box formed by AB; if not,
	  it marks iunion back to FALSE.  If then iterates through "iCs"
	  and "iDs", skipping any for which the corresponding "iunion"
	  element is FALSE.
	*/


	// We keep the invariants that iA < iB and iC < iD.
	// We try the A<->B and C<->D permutation in try_all_points.
	for (newpoint=params->startobj; newpoint<numxy; newpoint++) {
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
			// note: "newpoint" is used as an upper-bound on the largest
			// TRUE element in "iunion".
			try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint, params);
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
				// note: "newpoint+1" is used because "iunion[newpoint]" is TRUE.
				try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint+1, params);
			}
		}

		fprintf(stderr,
				"    using %lu of %lu objects (%i quads agree so far; %i tried, %i matched)      \r",
				newpoint+1, numxy, params->mostagree, params->numtries, params->nummatches);

		if ((params->mostagree >= params->max_matches_needed) ||
			(params->maxtries && (params->numtries >= params->maxtries)) ||
			(params->quitNow))
			break;
	}
	free(iCs);
	free(iDs);
	free(iunion);

	params->objsused = newpoint;
}

inline void try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
					  char* inbox, int maxind, solver_params* params) {
    int i;
    int iC, iD;
    double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
    double costheta, sintheta, scale, xxtmp;
    double xs[maxind];
    double ys[maxind];
	double fieldxs[maxind];
	double fieldys[maxind];
	
    xy *ABCDpix;

    ABCDpix = mk_xy(DIM_QUADS);

    Ax = xy_refx(params->field, iA);
    Ay = xy_refy(params->field, iA);
    Bx = xy_refx(params->field, iB);
    By = xy_refy(params->field, iB);

    xy_setx(ABCDpix, 0, Ax);
    xy_sety(ABCDpix, 0, Ay);
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
		Cx = xy_refx(params->field, i);
		Cy = xy_refy(params->field, i);

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
		}
    }

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

		/*
		  printf("iA=%i, iB=%i, iC=%i, iD=%i\n", iA, iB, iC, iD);
		  print_abcdpix(ABCDpix);
		*/

		params->numtries++;

		try_all_codes(Cx, Cy, Dx, Dy, iA, iB, iC, iD, ABCDpix, params);
    }

    free_xy(ABCDpix);
}

void try_all_codes(double Cx, double Cy, double Dx, double Dy,
				   sidx iA, sidx iB, sidx iC, sidx iD,
                   xy *ABCDpix, solver_params* params) {
    double thequery[4];
    kdtree_qres_t* result;
	double tol = square(params->codetol);

    // ABCD
    thequery[0] = Cx;
    thequery[1] = Cy;
    thequery[2] = Dx;
    thequery[3] = Dy;

    result = kdtree_rangesearch(params->codekd, thequery, tol);
    if (result->nres) {
		params->nummatches += result->nres;
		resolve_matches(result, thequery, ABCDpix, ABCD_ORDER, iA, iB, iC, iD, params);
    }
    kdtree_free_query(result);

    // BACD
    thequery[0] = 1.0 - Cx;
    thequery[1] = 1.0 - Cy;
    thequery[2] = 1.0 - Dx;
    thequery[3] = 1.0 - Dy;

    result = kdtree_rangesearch(params->codekd, thequery, tol);
    if (result->nres) {
		params->nummatches += result->nres;
		resolve_matches(result, thequery, ABCDpix, BACD_ORDER, iB, iA, iC, iD, params);
    }
    kdtree_free_query(result);

    // ABDC
    thequery[0] = Dx;
    thequery[1] = Dy;
    thequery[2] = Cx;
    thequery[3] = Cy;

    result = kdtree_rangesearch(params->codekd, thequery, tol);
    if (result->nres) {
		params->nummatches += result->nres;
		resolve_matches(result, thequery, ABCDpix, ABDC_ORDER, iA, iB, iD, iC, params);
    }
    kdtree_free_query(result);

    // BADC
    thequery[0] = 1.0 - Dx;
    thequery[1] = 1.0 - Dy;
    thequery[2] = 1.0 - Cx;
    thequery[3] = 1.0 - Cy;

    result = kdtree_rangesearch(params->codekd, thequery, tol);
    if (result->nres) {
		params->nummatches += result->nres;
		resolve_matches(result, thequery, ABCDpix, BADC_ORDER, iB, iA, iD, iC, params);
    }
    kdtree_free_query(result);
}

void resolve_matches(kdtree_qres_t* krez, double *query, xy *ABCDpix,
					 char order, sidx fA, sidx fB, sidx fC, sidx fD,
					 solver_params* params) {
    qidx jj, thisquadno;
    sidx iA, iB, iC, iD;
    double *transform;
    star *sA, *sB, *sC, *sD, *sMin, *sMax;
    MatchObj *mo;

    sA = mk_star();
    sB = mk_star();
    sC = mk_star();
    sD = mk_star();

    // This should normally only loop once; only one match should be found
    for (jj=0; jj<krez->nres; jj++) {
		int nagree;

		mo = mk_MatchObj();

		thisquadno = (qidx)krez->inds[jj];
		getquadids(thisquadno, &iA, &iB, &iC, &iD);
		getstarcoords(sA, sB, sC, sD, iA, iB, iC, iD);
		transform = fit_transform(ABCDpix, order, sA, sB, sC, sD);
		sMin = mk_star();
		sMax = mk_star();
		image_to_xyz(xy_refx(params->cornerpix, 0), xy_refy(params->cornerpix, 0), sMin, transform);
		image_to_xyz(xy_refx(params->cornerpix, 1), xy_refy(params->cornerpix, 1), sMax, transform);

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


		/*
		  mo->transform = transform;

		  mo->corners[0] = xy_refx(cornerpix, 0);
		  mo->corners[1] = xy_refy(cornerpix, 0);
		  mo->corners[2] = xy_refx(cornerpix, 1);
		  mo->corners[3] = xy_refy(cornerpix, 1);

		  mo->starA[0] = star_ref(sA, 0);
		  mo->starA[1] = star_ref(sA, 1);
		  mo->starA[2] = star_ref(sA, 2);
		  mo->starB[0] = star_ref(sB, 0);
		  mo->starB[1] = star_ref(sB, 1);
		  mo->starB[2] = star_ref(sB, 2);
		  mo->starC[0] = star_ref(sC, 0);
		  mo->starC[1] = star_ref(sC, 1);
		  mo->starC[2] = star_ref(sC, 2);
		  mo->starD[0] = star_ref(sD, 0);
		  mo->starD[1] = star_ref(sD, 1);
		  mo->starD[2] = star_ref(sD, 2);

		  mo->fieldA[0] = xy_refx(ABCDpix, 0);
		  mo->fieldA[1] = xy_refy(ABCDpix, 0);
		  mo->fieldB[0] = xy_refx(ABCDpix, 1);
		  mo->fieldB[1] = xy_refy(ABCDpix, 1);
		  mo->fieldC[0] = xy_refx(ABCDpix, 2);
		  mo->fieldC[1] = xy_refy(ABCDpix, 2);
		  mo->fieldD[0] = xy_refx(ABCDpix, 3);
		  mo->fieldD[1] = xy_refy(ABCDpix, 3);

		  mo->abcdorder = ABCD_ORDER;
		*/

		nagree = solver_add_hit(params->hitlist, mo, params->agreetol);

		if (nagree > params->mostagree) {
			params->mostagree = nagree;
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


int solver_add_hit(blocklist* hitlist, MatchObj* mo, double AgreeTol) {
    int i, N;

    N = blocklist_count(hitlist);

	/*
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
	*/

    for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hitlist, i);
		M = blocklist_count(hits);
		/*
		  if (verbose)
		  fprintf(stderr, "  hit %i: %i elements.\n", i, M);
		*/
		for (j=0; j<M; j++) {
			double d2;
			//double arcsec;
			MatchObj* m = (MatchObj*)blocklist_pointer_access(hits, j);
			d2 = distsq(mo->vector, m->vector, MATCH_VECTOR_SIZE);
			// DEBUG
			//arcsec = sqrt(d2) / sqrt(2.0)
			/*
			  if (verbose)
			  fprintf(stderr, "    el %i: dist %g (thresh %g)\n", j, sqrt(d2), AgreeTol);
			*/
			if (d2 < square(AgreeTol)) {
				blocklist_pointer_append(hits, mo);
				/*
				  if (verbose)
				  fprintf(stderr, "match!  (now %i agree)\n",
				  blocklist_count(hits));
				*/
				return blocklist_count(hits);
			}
		}
    }

	/*
	  if (verbose)
	  fprintf(stderr, "no agreement.\n");
	*/

    // no agreement - create new list.
    blocklist* newlist = blocklist_pointer_new(10);
    blocklist_pointer_append(newlist, mo);
    blocklist_pointer_append(hitlist, newlist);

    return 1;
}

// find min and max coordinates in this field;
// place them in "cornerpix"
void find_corners(xy *thisfield, xy *cornerpix) {
	double minx, maxx, miny, maxy;
	double x, y;
	qidx i;

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
