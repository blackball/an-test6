/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

#include "fileutil.h"
#include "mathutil.h"
#include "matchobj.h"
#include "solver.h"
#include "solver_callbacks.h"
#include "tic.h"
#include "solvedclient.h"
#include "solvedfile.h"
#include "svd.h"
#include "blind_wcs.h"
#include "keywords.h"

#include "kdtree.h"
#define KD_DIM 4
#include "kdtree.h"
#undef KD_DIM

#define DEBUGSOLVER 0

#if DEBUGSOLVER == 1
static void
ATTRIB_FORMAT(printf,1,2)
     debug(const char* format, ...) {
    va_list va;
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
}
#else
static void
ATTRIB_FORMAT(printf,1,2)
     debug(const char* format, ...) {}
#endif

static inline double getx(const double* d, uint ind) {
    return d[ind*2];
}
static inline double gety(const double* d, uint ind) {
    return d[ind*2 + 1];
}
static inline void setx(double* d, uint ind, double val) {
    d[ind*2] = val;
}
static inline void sety(double* d, uint ind, double val) {
    d[ind*2 + 1] = val;
}

void solver_default_params(solver_params* params) {
    memset(params, 0, sizeof(solver_params));
    params->maxAB = HUGE_VAL;
    params->funits_upper = HUGE_VAL;
}

static void try_all_codes(double Cx, double Cy, double Dx, double Dy,
                          uint iA, uint iB, uint iC, uint iD,
                          double *ABCDpix, solver_params* params);

static void try_all_codes_2(double Cx, double Cy, double Dx, double Dy,
                            uint iA, uint iB, uint iC, uint iD,
                            double *ABCDpix, solver_params* params,
                            bool current_parity);

static void resolve_matches(kdtree_qres_t* krez, double *query, double *field,
                            uint fA, uint fB, uint fC, uint fD,
                            solver_params* params, bool current_parity);

struct potential_quad {
    bool scale_ok;
    int iA, iB;
    double costheta, sintheta;
    bool* inbox;
    int ninbox;
    double* xy;
};
typedef struct potential_quad pquad;

static void check_scale(pquad* pq, solver_params* params) {
    double Ax, Ay, Bx, By, dx, dy, scale;
    Ax = getx(params->field, pq->iA);
    Ay = gety(params->field, pq->iA);
    Bx = getx(params->field, pq->iB);
    By = gety(params->field, pq->iB);
    dx = Bx - Ax;
    dy = By - Ay;
    scale = dx*dx + dy*dy;
    if ((scale < square(params->minAB)) ||
        (scale > square(params->maxAB))) {
        pq->scale_ok = FALSE;
        return;
    }
    pq->costheta = (dy + dx) / scale;
    pq->sintheta = (dy - dx) / scale;
    pq->scale_ok = TRUE;
}

static void check_inbox(pquad* pq, int start, solver_params* params) {
    int i;
    double Ax, Ay;
    Ax = getx(params->field, pq->iA);
    Ay = gety(params->field, pq->iA);
    // check which C, D points are inside the square/circle.
    for (i=start; i<pq->ninbox; i++) {
        double Cx, Cy, xxtmp;
        double tol = params->codetol;
        if (!pq->inbox[i]) continue;
        Cx = getx(params->field, i);
        Cy = gety(params->field, i);
        Cx -= Ax;
        Cy -= Ay;
        xxtmp = Cx;
        Cx =     Cx * pq->costheta + Cy * pq->sintheta;
        Cy = -xxtmp * pq->sintheta + Cy * pq->costheta;
        if (params->circle) {
            // make sure it's in the circle centered at (0.5, 0.5)
            // with radius 1/sqrt(2) (plus codetol for fudge):
            // (x-1/2)^2 + (y-1/2)^2   <=   (r + codetol)^2
            // x^2-x+1/4 + y^2-y+1/4   <=   (1/sqrt(2) + codetol)^2
            // x^2-x + y^2-y + 1/2     <=   1/2 + sqrt(2)*codetol + codetol^2
            // x^2-x + y^2-y           <=   sqrt(2)*codetol + codetol^2
            double r = (Cx*Cx - Cx) + (Cy*Cy - Cy);
            if (r > (tol * (M_SQRT2 + tol))) {
                pq->inbox[i] = FALSE;
                continue;
            }
        } else {
            if ((Cx > (1.0 + tol)) || (Cx < (0.0 - tol)) ||
                (Cy > (1.0 + tol)) || (Cy < (0.0 - tol))) {
                pq->inbox[i] = FALSE;
                continue;
            }
        }
        setx(pq->xy, i, Cx);
        sety(pq->xy, i, Cy);
    }
}

#if defined DEBUGSOLVER
static void print_inbox(pquad* pq) {
    int i;
    debug("[ ");
    for (i=0; i<pq->ninbox; i++) {
        if (pq->inbox[i])
            debug("%i ", i);
    }
    debug("] (n %i)\n", pq->ninbox);
}
#else
static void print_inbox(pquad* pq) {}
#endif

void solve_field(solver_params* params) {
    uint numxy, iA, iB, iC, iD, newpoint;
    int i;
    double usertime, systime;
    time_t lastcheck_ss;
    time_t lastcheck_sf;
    time_t lastcheck_cf;
    pquad* pquads;

    get_resource_stats(&usertime, &systime, NULL);
    params->starttime = usertime + systime;
    lastcheck_ss = lastcheck_sf = lastcheck_cf = time(NULL);

    numxy = params->nfield;
    if (numxy < DIM_QUADS) //if there are<4 objects in field, forget it
        return;
    if (params->endobj && (numxy > params->endobj))
        numxy = params->endobj;

    if (params->startobj >= numxy)
        return;

    pquads = calloc(numxy * numxy, sizeof(pquad));

    /*
      We maintain an array of "potential quads" (pquad) structs, where each
      struct corresponds to one choice of stars A and B; the struct at
      index (B * numxy + A) hold information about quads that could be created
      using stars A,B.

      (We only use the above-diagonal elements of this 2D array because A<B.)

      For each AB pair, we cache the scale and the rotation parameters,
      and we keep an array "inbox" of length "numxy" of booleans, that say
      whether that star is eligible to be star C or D of a quad with AB at
      the corners.  (Obviously A and B can't).

      The "ninbox" parameter is somewhat misnamed - it says that "inbox"
      in the range [0, ninbox) have been initialized.
    */

    /* (See explanatory paragraph below)
       If "params->startobj" isn't zero, then we need to initialize the triangle
       of "pquads" up to A=startobj-2,B=startobj-1.
    */
    if (params->startobj) {
        debug("startobj > 0; priming pquad arrays.\n");
        for (iB=0; iB<params->startobj; iB++) {
            for (iA=0; iA<iB; iA++) {
                pquad* pq = pquads + iB * numxy + iA;
                pq->iA = iA;
                pq->iB = iB;
                debug("trying A=%i, B=%i\n", iA, iB);
                check_scale(pq, params);
                if (!pq->scale_ok) {
                    debug("  bad scale for A=%i, B=%i\n", iA, iB);
                    continue;
                }
                pq->xy    = malloc(numxy * 2 * sizeof(double));
                pq->inbox = malloc(numxy * sizeof(bool));
                memset(pq->inbox, TRUE, params->startobj);
                pq->ninbox = params->startobj;
                pq->inbox[iA] = FALSE;
                pq->inbox[iB] = FALSE;
                check_inbox(pq, 0, params);
                debug("  inbox(A=%i, B=%i): ", iA, iB);
                print_inbox(pq);
            }
        }
    }

    /*
      Each time through the "for" loop below, we consider a new
      star ("newpoint").  First, we try building all quads that
      have the new star on the diagonal (star B).  Then, we try
      building all quads that have the star not on the diagonal
      (star D).

      For each AB pair, we have a "potential_quad" or "pquad" struct.
      This caches the computation we need to do: deciding whether the
      scale is acceptable, computing the transformation to code
      coordinates, and deciding which C,D stars are in the box/circle.

      We keep the invariants that iA < iB and iC < iD.
      The A<->B and C<->D permutations will be tried in try_all_codes.
    */

    for (newpoint=params->startobj; newpoint<numxy; newpoint++) {
        double ABCDpix[8];

        debug("Trying newpoint=%i\n", newpoint);

        if ((params->solved_in) ||
            (params->do_solvedserver) ||
            (params->cancelfname)) {
            // check if the field has already been solved...
            time_t t;
            t = time(NULL);

            if (params->solved_in && ((t - lastcheck_sf) > 5)) {
                if (solvedfile_get(params->solved_in, params->fieldnum)) {
                    fprintf(stderr, "  field %u: file %s indicates that the field has been solved.\n",
                            params->fieldnum, params->solved_in);
                    break;
                }
                lastcheck_sf = t;
            }
            if (params->do_solvedserver && ((t - lastcheck_ss) > 10)) {
                if (solvedclient_get(params->fieldid, params->fieldnum)) {
                    fprintf(stderr, "  field %u: field solved; aborting.\n", params->fieldnum);
                    break;
                }
                lastcheck_ss = t;
            }
            if (params->cancelfname && ((t - lastcheck_cf) > 5)) {
                if (file_exists(params->cancelfname)) {
                    params->cancelled = TRUE;
                    params->quitNow = TRUE;
                    fprintf(stderr, "File \"%s\" exists: cancelling.\n", params->cancelfname);
                    break;
                }
                lastcheck_cf = t;
            }
        }

        params->objsused = newpoint;
        // quads with the new star on the diagonal:
        iB = newpoint;
        setx(ABCDpix, 1, getx(params->field, iB));
        sety(ABCDpix, 1, gety(params->field, iB));
        debug("Trying quads with B=%i\n", newpoint);
        for (iA=0; iA<newpoint; iA++) {
            // initialize the "pquad" struct for this AB combo.
            pquad* pq = pquads + iB * numxy + iA;
            pq->iA = iA;
            pq->iB = iB;
            debug("  trying A=%i, B=%i\n", iA, iB);
            check_scale(pq, params);
            if (!pq->scale_ok) {
                debug("    bad scale for A=%i, B=%i\n", iA, iB);
                continue;
            }
            // initialize the "inbox" array:
            pq->inbox = malloc(numxy * sizeof(bool));
            pq->xy    = malloc(numxy * 2 * sizeof(double));
            // -try all stars up to "newpoint"...
            memset(pq->inbox, TRUE, newpoint+1);
            pq->ninbox = newpoint+1;
            // -except A and B.
            pq->inbox[iA] = FALSE;
            pq->inbox[iB] = FALSE;
            check_inbox(pq, 0, params);
            debug("    inbox(A=%i, B=%i): ", iA, iB);
            print_inbox(pq);

            setx(ABCDpix, 0, getx(params->field, iA));
            sety(ABCDpix, 0, gety(params->field, iA));
            for (iC=0; iC<newpoint; iC++) {
                double cx, cy, dx, dy;
                if (!pq->inbox[iC])
                    continue;
                setx(ABCDpix, 2, getx(params->field, iC));
                sety(ABCDpix, 2, gety(params->field, iC));
                cx = getx(pq->xy, iC);
                cy = gety(pq->xy, iC);
                for (iD=iC+1; iD<newpoint; iD++) {
                    if (!pq->inbox[iD])
                        continue;
                    setx(ABCDpix, 3, getx(params->field, iD));
                    sety(ABCDpix, 3, gety(params->field, iD));
                    dx = getx(pq->xy, iD);
                    dy = gety(pq->xy, iD);
                    params->numtries++;
                    debug("    trying quad [%i %i %i %i]\n", iA, iB, iC, iD);
                    try_all_codes(cx, cy, dx, dy, iA, iB, iC, iD, ABCDpix, params);
                    if (params->quitNow)
                        break;
                }
            }
        }

        if (params->quitNow)
            break;

        // quads with the new star not on the diagonal:
        iD = newpoint;
        setx(ABCDpix, 3, getx(params->field, iD));
        sety(ABCDpix, 3, gety(params->field, iD));
        debug("Trying quads with D=%i\n", newpoint);
        for (iA=0; iA<newpoint; iA++) {
            for (iB=iA+1; iB<newpoint; iB++) {
                double cx, cy, dx, dy;
                // grab the "pquad" for this AB combo
                pquad* pq = pquads + iB * numxy + iA;
                if (!pq->scale_ok) {
                    debug("  bad scale for A=%i, B=%i\n", iA, iB);
                    continue;
                }
                // test if this D is in the box:
                pq->inbox[iD] = TRUE;
                pq->ninbox = iD + 1;
                check_inbox(pq, iD, params);
                if (!pq->inbox[iD]) {
                    debug("  D is not in the box for A=%i, B=%i\n", iA, iB);
                    continue;
                }
                debug("  D is in the box for A=%i, B=%i\n", iA, iB);
                setx(ABCDpix, 0, getx(params->field, iA));
                sety(ABCDpix, 0, gety(params->field, iA));
                setx(ABCDpix, 1, getx(params->field, iB));
                sety(ABCDpix, 1, gety(params->field, iB));
                dx = getx(pq->xy, iD);
                dy = gety(pq->xy, iD);
                for (iC=0; iC<newpoint; iC++) {
                    if (!pq->inbox[iC])
                        continue;
                    setx(ABCDpix, 2, getx(params->field, iC));
                    sety(ABCDpix, 2, gety(params->field, iC));
                    cx = getx(pq->xy, iC);
                    cy = gety(pq->xy, iC);
                    params->numtries++;
                    debug("  trying quad [%i %i %i %i]\n", iA, iB, iC, iD);
                    try_all_codes(cx, cy, dx, dy, iA, iB, iC, iD, ABCDpix, params);
                    if (params->quitNow)
                        break;
                }
            }
        }

        if (!params->quiet)
            fprintf(stderr,
                    "  field %u, object %u of %u: %i quads tried, %i matched.\n",
                    params->fieldnum, newpoint+1, numxy, params->numtries, params->nummatches);

        if ((params->maxquads   && (params->numtries   >= params->maxquads)) ||
            (params->maxmatches && (params->nummatches >= params->maxmatches)) ||
            params->quitNow)
            break;
    }

    for (i=0; i<(numxy*numxy); i++) {
        pquad* pq = pquads + i;
        free(pq->inbox);
        free(pq->xy);
    }
    free(pquads);
}

static inline void set_xy(double* dest, int destind, double* src, int srcind) {
    setx(dest, destind, getx(src, srcind));
    sety(dest, destind, gety(src, srcind));
}

static void try_all_codes(double Cx, double Cy, double Dx, double Dy,
                          uint iA, uint iB, uint iC, uint iD,
                          double *ABCDpix, solver_params* params) {
    debug("    code=[%g,%g,%g,%g].\n", Cx, Cy, Dx, Dy);
    if (params->parity == PARITY_NORMAL ||
        params->parity == PARITY_BOTH) {
        debug("    trying normal parity: code=[%g,%g,%g,%g].\n", Cx, Cy, Dx, Dy);
        try_all_codes_2(Cx, Cy, Dx, Dy, iA, iB, iC, iD, ABCDpix, params, FALSE);
    }
    if (params->parity == PARITY_FLIP ||
        params->parity == PARITY_BOTH) {
        debug("    trying reverse parity: code=[%g,%g,%g,%g].\n", Cy, Cx, Dy, Dx);
        try_all_codes_2(Cy, Cx, Dy, Dx, iA, iB, iC, iD, ABCDpix, params, TRUE);
    }
}

static void try_all_codes_2(double Cx, double Cy, double Dx, double Dy,
                            uint iA, uint iB, uint iC, uint iD,
                            double *ABCDpix, solver_params* params,
                            bool current_parity) {

    double thequery[4];
    kdtree_qres_t* result = NULL;
    double tol = square(params->codetol);
    double inorder[8];
    int A=0, B=1, C=2, D=3;
    double usertime, systime;
    int options = KD_OPTIONS_SMALL_RADIUS | KD_OPTIONS_COMPUTE_DISTS |
        KD_OPTIONS_NO_RESIZE_RESULTS;

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

        result = kdtree_rangesearch_options_reuse(params->codekd, result, thequery, tol, options);

        debug("      trying ABCD = [%i %i %i %i]: %i results.\n", iA, iB, iC, iD, result->nres);

        if (result->nres)
            resolve_matches(result, thequery, inorder, iA, iB, iC, iD, params, current_parity);
        if (params->quitNow)
            goto quitnow;
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

        result = kdtree_rangesearch_options_reuse(params->codekd, result, thequery, tol, options);

        debug("      trying BACD = [%i %i %i %i]: %i results.\n", iB, iA, iC, iD, result->nres);

        if (result->nres)
            resolve_matches(result, thequery, inorder, iB, iA, iC, iD, params, current_parity);
        if (params->quitNow)
            goto quitnow;
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

        result = kdtree_rangesearch_options_reuse(params->codekd, result, thequery, tol, options);

        debug("      trying ABDC = [%i %i %i %i]: %i results.\n", iA, iB, iD, iC, result->nres);

        if (result->nres)
            resolve_matches(result, thequery, inorder, iA, iB, iD, iC, params, current_parity);
        if (params->quitNow)
            goto quitnow;
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

        result = kdtree_rangesearch_options_reuse(params->codekd, result, thequery, tol, options);

        debug("      trying BADC = [%i %i %i %i]: %i results.\n", iB, iA, iD, iC, result->nres);

        if (result->nres)
            resolve_matches(result, thequery, inorder, iB, iA, iD, iC, params, current_parity);
        if (params->quitNow)
            goto quitnow;
    } else
        params->numcxdxskipped++;

 quitnow:
    kdtree_free_query(result);
}

// "field" contains the xy pixel coordinates of stars A,B,C,D.
static void resolve_matches(kdtree_qres_t* krez, double *query, double *field,
                            uint fA, uint fB, uint fC, uint fD,
                            solver_params* params, bool current_parity) {
    uint jj, thisquadno;
    uint iA, iB, iC, iD;
    MatchObj mo;

    for (jj=0; jj<krez->nres; jj++) {
        double star[12];
        double scale;
        double arcsecperpix;
        tan_t wcs;

        params->nummatches++;

        thisquadno = (uint)krez->inds[jj];
        getquadids(thisquadno, &iA, &iB, &iC, &iD);
        getstarcoord(iA, star + 0*3);
        getstarcoord(iB, star + 1*3);
        getstarcoord(iC, star + 2*3);
        getstarcoord(iD, star + 3*3);

        debug("        stars [%i %i %i %i]\n", iA, iB, iC, iD);

        // compute TAN projection from the matching quad alone.
        blind_wcs_compute_2(star, field, 4, &wcs, &scale);

        // FIXME - should there be scale fudge here?
        arcsecperpix = scale * 3600.0;
        if (arcsecperpix > params->funits_upper ||
            arcsecperpix < params->funits_lower) {
            debug("          bad scale.\n");
            continue;
        }

        params->numscaleok++;

        if (params->mo_template)
            memcpy(&mo, params->mo_template, sizeof(MatchObj));

        memcpy(&(mo.wcstan), &wcs, sizeof(tan_t));
        mo.wcs_valid = TRUE;
        mo.code_err = krez->sdists[jj];
        mo.scale = arcsecperpix;
        mo.parity = current_parity;
        mo.quads_tried   = params->numtries;
        mo.quads_matched = params->nummatches;
        mo.quads_scaleok = params->numscaleok;
        mo.timeused = params->timeused;
        mo.quadno = thisquadno;
        mo.star[0] = iA;
        mo.star[1] = iB;
        mo.star[2] = iC;
        mo.star[3] = iD;
        mo.field[0] = fA;
        mo.field[1] = fB;
        mo.field[2] = fC;
        mo.field[3] = fD;
        mo.ids[0] = getstarid(iA);
        mo.ids[1] = getstarid(iB);
        mo.ids[2] = getstarid(iC);
        mo.ids[3] = getstarid(iD);

        // transform the corners of the field...
        tan_pixelxy2xyzarr(&(mo.wcstan), params->field_minx, params->field_miny, mo.sMin);
        tan_pixelxy2xyzarr(&(mo.wcstan), params->field_maxx, params->field_maxy, mo.sMax);
        tan_pixelxy2xyzarr(&(mo.wcstan), params->field_minx, params->field_maxy, mo.sMinMax);
        tan_pixelxy2xyzarr(&(mo.wcstan), params->field_maxx, params->field_miny, mo.sMaxMin);

        if (params->handlehit(params, &mo))
            params->quitNow = TRUE;
        // Note - after this call returns, the "mo" may
        // have been freed!

        if (params->quitNow)
            return;
    }
}

