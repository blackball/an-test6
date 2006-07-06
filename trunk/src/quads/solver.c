#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "fileutil.h"
#include "mathutil.h"
#include "kdtree.h"
#include "matchobj.h"
#include "solver.h"
#include "solver_callbacks.h"
#include "tic.h"
#include "solvedclient.h"

static inline double getx(double* d, int ind) {
	// return d[ind*2];
	return d[ind << 1];
}
static inline double gety(double* d, int ind) {
	// return d[ind*2 + 1];
	return d[(ind << 1) | 1];
}
static inline void setx(double* d, int ind, double val) {
	// d[ind*2] = val;
	d[ind << 1] = val;
}
static inline void sety(double* d, int ind, double val) {
	// d[ind*2 + 1] = val;
	d[(ind << 1) | 1] = val;
}

void solver_default_params(solver_params* params) {
	memset(params, 0, sizeof(solver_params));
	params->maxAB = 1e300;
	params->arcsec_per_pixel_upper = 1.0e300;
}

static void find_corners(double *thisfield, int nfield, double *cornerpix);

static inline void try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
							 char* inbox, int maxind, solver_params* params);

static void try_all_codes(double Cx, double Cy, double Dx, double Dy,
						  uint iA, uint iB, uint iC, uint iD,
						  double *ABCDpix, solver_params* params);

static void resolve_matches(kdtree_qres_t* krez, double *query, double *field,
							uint fA, uint fB, uint fC, uint fD,
							solver_params* params);

static bool check_scale(int iA, int iB, solver_params* params) {
	double Ax, Ay, Bx, By, dx, dy, scale;
	Ax = getx(params->field, iA);
	Ay = gety(params->field, iA);
	Bx = getx(params->field, iB);
	By = gety(params->field, iB);
	dx = Bx - Ax;
	dy = By - Ay;
	scale = dx*dx + dy*dy;
	if ((scale < square(params->minAB)) || (scale > square(params->maxAB))) {
		//fprintf(stderr, "Quad scale %g: not in allowable range [%g, %g].\n", scale, params->minAB, params->maxAB);
		return FALSE;
	}
	return TRUE;
}

void solve_field(solver_params* params) {
    uint numxy, iA, iB, iC, iD, newpoint;
    int *iCs, *iDs;
    char *iunion;
    int ncd;
	unsigned char *scale_ok;
	double c;
	double usertime, systime;
	double lastcheck;

	get_resource_stats(&usertime, &systime, NULL);
	params->starttime = usertime + systime;
	lastcheck = params->starttime;

	numxy = params->nfield;
	if (numxy < DIM_QUADS) //if there are<4 objects in field, forget it
		return;
	if (params->endobj && (numxy > params->endobj))
		numxy = params->endobj;

	find_corners(params->field, params->nfield, params->cornerpix);

	// how many pixels from corner to corner of the field?
	c  = square(getx(params->cornerpix, 1) - getx(params->cornerpix, 0));
	c += square(gety(params->cornerpix, 1) - gety(params->cornerpix, 0));
	params->fieldscale = sqrt(c);
	// how many arcsec from corner to corner of the field?
	params->starscale_upper = arcsec2distsq(params->arcsec_per_pixel_upper * params->fieldscale);
	params->starscale_lower = arcsec2distsq(params->arcsec_per_pixel_lower * params->fieldscale);

	iCs = malloc(numxy * numxy * sizeof(int));
	iDs = malloc(numxy * numxy * sizeof(int));
	iunion = malloc(numxy * sizeof(char));

	scale_ok = calloc(numxy*numxy, sizeof(unsigned char));

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
		if (params->solvedfn && file_exists(params->solvedfn)) {
			fprintf(stderr, "  field %u: file %s exists; aborting.\n", params->fieldnum, params->solvedfn);
			break;
		}
		if (params->do_solvedserver) {
			get_resource_stats(&usertime, &systime, NULL);
			if (usertime + systime - lastcheck > 10.0) {
				if (solvedclient_get(params->fieldid, params->fieldnum)) {
					fprintf(stderr, "  field %u: field solved; aborting.\n", params->fieldnum);
					break;
				}
				lastcheck = usertime + systime;
			}
		}

		params->objsused = newpoint;
		// quads with the new star on the diagonal:
		iB = newpoint;
		for (iA=0; iA<newpoint; iA++) {
			if (!check_scale(iA, iB, params))
				continue;
			scale_ok[iA * numxy + iB] = 1;
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
				if (!scale_ok[iA * numxy + iB])
					continue;

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

		if ((params->maxtries && (params->numtries >= params->maxtries)) ||
			params->quitNow)
			break;
	}
	free(scale_ok);
	free(iCs);
	free(iDs);
	free(iunion);
}

static inline void try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
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
    double ABCDpix[8];

	// assume the scale has already been checked by the caller.
	// if (!check_scale(iA, iB, params)) return;

    Ax = getx(params->field, iA);
    Ay = gety(params->field, iA);
    Bx = getx(params->field, iB);
    By = gety(params->field, iB);

	setx(ABCDpix, 0, Ax);
	sety(ABCDpix, 0, Ay);
	setx(ABCDpix, 1, Bx);
	sety(ABCDpix, 1, By);

	dx = Bx - Ax;
	dy = By - Ay;
	scale = dx*dx + dy*dy;

    costheta = (dy + dx) / scale;
    sintheta = (dy - dx) / scale;

    // check which C, D points are inside the square/circle.
    for (i=0; i<maxind; i++) {
		if (!inbox[i]) continue;
		Cx = getx(params->field, i);
		Cy = gety(params->field, i);

		fieldxs[i] = Cx;
		fieldys[i] = Cy;

		Cx -= Ax;
		Cy -= Ay;
		xxtmp = Cx;
		Cx =     Cx * costheta + Cy * sintheta;
		Cy = -xxtmp * sintheta + Cy * costheta;
		if (params->circle) {
			// make sure it's in the circle centered at (0.5, 0.5)...
			// (x-1/2)^2 + (y-1/2)^2   <=   r^2
			// x^2-x+1/4 + y^2-y+1/4   <=   (1/sqrt(2))^2
			// x^2-x + y^2-y + 1/2     <=   1/2
			// x^2-x + y^2-y           <=   0
			double r = (Cx*Cx - Cx) + (Cy*Cy - Cy);
			if (r > 0.0) {
				inbox[i] = 0;
				continue;
			}
		} else {
			if ((Cx > 1.0) || (Cx < 0.0) ||
				(Cy > 1.0) || (Cy < 0.0)) {
				inbox[i] = 0;
				continue;
			}
		}
		xs[i] = Cx;
		ys[i] = Cy;
    }

    for (i=0; i<ncd; i++) {
		double Cfx, Cfy, Dfx, Dfy;
		iC = iCs[i];
		iD = iDs[i];
		// are both C and D in the (possibly round) box?
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

		setx(ABCDpix, 2, Cfx);
		sety(ABCDpix, 2, Cfy);
		setx(ABCDpix, 3, Dfx);
		sety(ABCDpix, 3, Dfy);

		params->numtries++;

		try_all_codes(Cx, Cy, Dx, Dy, iA, iB, iC, iD, ABCDpix, params);
		if (params->quitNow)
			break;
    }
}

static inline void set_xy(double* dest, int destind, double* src, int srcind) {
	setx(dest, destind, getx(src, srcind));
	sety(dest, destind, gety(src, srcind));
}

static void try_all_codes(double Cx, double Cy, double Dx, double Dy,
						  uint iA, uint iB, uint iC, uint iD,
						  double *ABCDpix, solver_params* params) {

    double thequery[4];
    kdtree_qres_t* result;
	double tol = square(params->codetol);
	double inorder[8];
	int A=0, B=1, C=2, D=3;
	double usertime, systime;
	get_resource_stats(&usertime, &systime, NULL);
	params->timeused = (usertime + systime) - params->starttime;
	if (params->timeused < 0.0)
		params->timeused = 0.0;

    // ABCD
    thequery[0] = Cx;
    thequery[1] = Cy;
    thequery[2] = Dx;
    thequery[3] = Dy;

	if ((params->cxdx_margin == 0.0) ||
		(thequery[0] <= (thequery[2] + params->cxdx_margin))) {

		set_xy(inorder, 0, ABCDpix, A);
		set_xy(inorder, 1, ABCDpix, B);
		set_xy(inorder, 2, ABCDpix, C);
		set_xy(inorder, 3, ABCDpix, D);

		result = kdtree_rangesearch(params->codekd, thequery, tol);
		if (result->nres)
			resolve_matches(result, thequery, inorder, iA, iB, iC, iD, params);
		kdtree_free_query(result);
		if (params->quitNow)
			return;
	} else
		params->numcxdxskipped++;

    // BACD
    thequery[0] = 1.0 - Cx;
    thequery[1] = 1.0 - Cy;
    thequery[2] = 1.0 - Dx;
    thequery[3] = 1.0 - Dy;

	if ((params->cxdx_margin == 0.0) ||
		(thequery[0] <= (thequery[2] + params->cxdx_margin))) {

		set_xy(inorder, 0, ABCDpix, B);
		set_xy(inorder, 1, ABCDpix, A);
		set_xy(inorder, 2, ABCDpix, C);
		set_xy(inorder, 3, ABCDpix, D);

		result = kdtree_rangesearch(params->codekd, thequery, tol);
		if (result->nres)
			resolve_matches(result, thequery, inorder, iB, iA, iC, iD, params);
		kdtree_free_query(result);
		if (params->quitNow)
			return;
	} else
		params->numcxdxskipped++;


    // ABDC
    thequery[0] = Dx;
    thequery[1] = Dy;
    thequery[2] = Cx;
    thequery[3] = Cy;

	if ((params->cxdx_margin == 0.0) ||
		(thequery[0] <= (thequery[2] + params->cxdx_margin))) {

		set_xy(inorder, 0, ABCDpix, A);
		set_xy(inorder, 1, ABCDpix, B);
		set_xy(inorder, 2, ABCDpix, D);
		set_xy(inorder, 3, ABCDpix, C);

		result = kdtree_rangesearch(params->codekd, thequery, tol);
		if (result->nres)
			resolve_matches(result, thequery, inorder, iA, iB, iD, iC, params);
		kdtree_free_query(result);
		if (params->quitNow)
			return;
	} else
		params->numcxdxskipped++;


    // BADC
    thequery[0] = 1.0 - Dx;
    thequery[1] = 1.0 - Dy;
    thequery[2] = 1.0 - Cx;
    thequery[3] = 1.0 - Cy;

	if ((params->cxdx_margin == 0.0) ||
		(thequery[0] <= (thequery[2] + params->cxdx_margin))) {

		set_xy(inorder, 0, ABCDpix, B);
		set_xy(inorder, 1, ABCDpix, A);
		set_xy(inorder, 2, ABCDpix, D);
		set_xy(inorder, 3, ABCDpix, C);

		result = kdtree_rangesearch(params->codekd, thequery, tol);
		if (result->nres)
			resolve_matches(result, thequery, inorder, iB, iA, iD, iC, params);
		kdtree_free_query(result);
		if (params->quitNow)
			return;
	} else
		params->numcxdxskipped++;

}

static void resolve_matches(kdtree_qres_t* krez, double *query, double *field,
							uint fA, uint fB, uint fC, uint fD,
							solver_params* params) {
    uint jj, thisquadno;
    uint iA, iB, iC, iD;
    double transform[9];
    MatchObj *mo;
	double sMin[3], sMax[3], sMinMax[3], sMaxMin[3];

    for (jj=0; jj<krez->nres; jj++) {
		int nagree;
		//uint64_t idA, idB, idC, idD;
		double star[12];
		double starscale;

		params->nummatches++;

		thisquadno = (uint)krez->inds[jj];
		getquadids(thisquadno, &iA, &iB, &iC, &iD);
		getstarcoord(iA, star + 0*3);
		getstarcoord(iB, star + 1*3);
		getstarcoord(iC, star + 2*3);
		getstarcoord(iD, star + 3*3);

		fit_transform(star, field, 4, transform);
		image_to_xyz(getx(params->cornerpix, 0), gety(params->cornerpix, 0), sMin, transform);
		image_to_xyz(getx(params->cornerpix, 1), gety(params->cornerpix, 1), sMax, transform);

		// check scale
		starscale  = square(sMax[0] - sMin[0]);
		starscale += square(sMax[1] - sMin[1]);
		starscale += square(sMax[2] - sMin[2]);
		if ((starscale > params->starscale_upper) ||
			(starscale < params->starscale_lower))
			// this quad has invalid scale.
			continue;

		params->numscaleok++;

		image_to_xyz(getx(params->cornerpix, 2), gety(params->cornerpix, 2), sMinMax, transform);
		image_to_xyz(getx(params->cornerpix, 3), gety(params->cornerpix, 3), sMaxMin, transform);

		mo = mk_MatchObj();
		if (params->mo_template)
			memcpy(mo, params->mo_template, sizeof(MatchObj));

		mo->quads_tried   = params->numtries;
		mo->quads_matched = params->nummatches;
		mo->quads_scaleok = params->numscaleok;
		mo->timeused = params->timeused;

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

		memcpy(mo->sMin,    sMin,    3 * sizeof(double));
		memcpy(mo->sMax,    sMax,    3 * sizeof(double));
		memcpy(mo->sMinMax, sMinMax, 3 * sizeof(double));
		memcpy(mo->sMaxMin, sMaxMin, 3 * sizeof(double));

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
static void find_corners(double *thisfield, int nfield, double *cornerpix) {
	double minx, maxx, miny, maxy;
	double x, y;
	uint i;

	minx = miny = 1e308;
	maxx = maxy = -1e308;
	
	for (i=0; i<nfield; i++) {
		x = getx(thisfield, i);
		y = gety(thisfield, i);
		if (x < minx) minx = x;
		if (x > maxx) maxx = x;
		if (y < miny) miny = y;
		if (y > maxy) maxy = y;
	}

    setx(cornerpix, 0, minx);
    sety(cornerpix, 0, miny);
    setx(cornerpix, 1, maxx);
    sety(cornerpix, 1, maxy);

    setx(cornerpix, 2, minx);
    sety(cornerpix, 2, maxy);
    setx(cornerpix, 3, maxx);
    sety(cornerpix, 3, miny);
}
