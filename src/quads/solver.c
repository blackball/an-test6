#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "fileutil.h"
#include "mathutil.h"
#include "kdtree.h"
#include "matchobj.h"
#include "solver.h"
#include "solver_callbacks.h"

void solver_default_params(solver_params* params) {
	params->field = NULL;
	params->codekd = NULL;
	params->startobj = 0;
	params->endobj = 0;
	params->maxtries = 0;
	params->max_matches_needed = 0;
	params->codetol = 0.0;
	params->quitNow = FALSE;
	params->minAB = 0.0;
	params->maxAB = 1e300;
	params->arcsec_per_pixel_lower = 0.0;
	params->arcsec_per_pixel_upper = 1.0e300;
	params->maxAB = 1e300;
	params->cornerpix = NULL;
	params->handlehit = NULL;
	params->numtries = 0;
	params->nummatches = 0;
	params->mostagree = 0;
	params->objsused = 0;
}

void find_corners(xy *thisfield, xy *cornerpix);

inline void try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
					  char* inbox, int maxind, solver_params* params);

void try_all_codes(double Cx, double Cy, double Dx, double Dy,
				   uint iA, uint iB, uint iC, uint iD,
                   xy *ABCDpix, solver_params* params);

void resolve_matches(kdtree_qres_t* krez, double *query, xy *ABCDpix,
					 char order, uint fA, uint fB, uint fC, uint fD,
					 solver_params* params);

void solve_field(solver_params* params) {
    uint numxy, iA, iB, iC, iD, newpoint;
    int *iCs, *iDs;
    char *iunion;
    int ncd;

	numxy = xy_size(params->field);
	if (params->endobj && (numxy > params->endobj))
		numxy = params->endobj;

	find_corners(params->field, params->cornerpix);

	if (numxy < DIM_QUADS) //if there are<4 objects in field, forget it
		return;

	iCs = malloc(numxy * numxy * sizeof(int));
	iDs = malloc(numxy * numxy * sizeof(int));
	iunion = malloc(numxy * sizeof(char));

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
				"  field %u, object %u of %u: %i agree, %i tried, %i matched.\n",
				params->fieldnum, newpoint+1, numxy, params->mostagree, params->numtries, params->nummatches);

		if ((params->max_matches_needed && (params->mostagree >= params->max_matches_needed)) ||
			(params->maxtries && (params->numtries >= params->maxtries)) ||
			params->quitNow)
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
	double dx, dy;
    double costheta, sintheta, scale, xxtmp;
    double xs[maxind];
    double ys[maxind];
	double fieldxs[maxind];
	double fieldys[maxind];
    xy *ABCDpix;

    Ax = xy_refx(params->field, iA);
    Ay = xy_refy(params->field, iA);
    Bx = xy_refx(params->field, iB);
    By = xy_refy(params->field, iB);

	dx = Bx - Ax;
	dy = By - Ay;
	scale = dx*dx + dy*dy;

	if ((scale < square(params->minAB)) || (scale > square(params->maxAB))) {
		//fprintf(stderr, "Quad scale %g: not in allowable range [%g, %g].\n", scale, params->minAB, params->maxAB);
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

		params->numtries++;

		try_all_codes(Cx, Cy, Dx, Dy, iA, iB, iC, iD, ABCDpix, params);
		if (params->quitNow)
			break;
    }

    free_xy(ABCDpix);
}

void try_all_codes(double Cx, double Cy, double Dx, double Dy,
				   uint iA, uint iB, uint iC, uint iD,
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
	if (params->quitNow)
		return;

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
	if (params->quitNow)
		return;

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
	if (params->quitNow)
		return;

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
	if (params->quitNow)
		return;
}

void resolve_matches(kdtree_qres_t* krez, double *query, xy *ABCDpix,
					 char order, uint fA, uint fB, uint fC, uint fD,
					 solver_params* params) {
    uint jj, thisquadno;
    uint iA, iB, iC, iD;
    double transform[9];
    MatchObj *mo;
	double sA[3], sB[3], sC[3], sD[3], sMin[3], sMax[3];

    for (jj=0; jj<krez->nres; jj++) {
		int nagree;
		//uint64_t idA, idB, idC, idD;

		thisquadno = (uint)krez->inds[jj];
		getquadids(thisquadno, &iA, &iB, &iC, &iD);
		getstarcoord(iA, sA);
		getstarcoord(iB, sB);
		getstarcoord(iC, sC);
		getstarcoord(iD, sD);

		fit_transform(ABCDpix, order, sA, sB, sC, sD, transform);
		image_to_xyz(xy_refx(params->cornerpix, 0), xy_refy(params->cornerpix, 0), sMin, transform);
		image_to_xyz(xy_refx(params->cornerpix, 1), xy_refy(params->cornerpix, 1), sMax, transform);

		// scale checking
		// fieldunitsupper/lower is in arcseconds/pixel
		{
			double d, c;
			d  = square(sMax[0] - sMin[0]);
			d += square(sMax[1] - sMin[1]);
			d += square(sMax[2] - sMin[2]);
			// convert 'd' from squared distance on the unit sphere
			// to arcseconds...
			d = distsq2arc(d);
			d = rad2deg(d) * 60.0 * 60.0;

			c  = square(xy_refy(params->cornerpix, 1) - xy_refy(params->cornerpix, 0));
			c += square(xy_refx(params->cornerpix, 1) - xy_refx(params->cornerpix, 0));
			c = sqrt(c);

			if ((d/c > params->arcsec_per_pixel_upper) || (d/c < params->arcsec_per_pixel_lower)) {
				// this quad has invalid scale.
				continue;
			}
		}


		mo = mk_MatchObj();

		memcpy(mo->transform, transform, sizeof(mo->transform));
		mo->transform_valid = TRUE;

		mo->quadno = thisquadno;
		mo->star[0] = iA;
		mo->star[1] = iB;
		mo->star[2] = iC;
		mo->star[3] = iD;
		mo->field[0] = fA;
		mo->field[1] = fB;
		mo->field[2] = fC;
		mo->field[3] = fD;

		mo->sMin[0] = sMin[0];
		mo->sMin[1] = sMin[1];
		mo->sMin[2] = sMin[2];
		mo->sMax[0] = sMax[0];
		mo->sMax[1] = sMax[1];
		mo->sMax[2] = sMax[2];

		mo->vector[0] = sMin[0];
		mo->vector[1] = sMin[1];
		mo->vector[2] = sMin[2];
		mo->vector[3] = sMax[0];
		mo->vector[4] = sMax[1];
		mo->vector[5] = sMax[2];

		mo->code_err = krez->sdists[jj];

		/*
		  idA = getstarid(iA);
		  idB = getstarid(iB);
		  idC = getstarid(iC);
		  idD = getstarid(iD);
		*/

		/*

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

		//nagree = hitlist_add_hit(params->hits, mo);
		nagree = params->handlehit(params, mo);
		// Note - after this call returns, the "mo" may
		// have been freed!

		if (nagree > params->mostagree) {
			params->mostagree = nagree;
		}
		if (params->quitNow)
			return;
    }
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
