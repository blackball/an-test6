/*
 This file is part of the Astrometry.net suite.
 Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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
#include <sys/param.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

#include "ioutils.h"
#include "mathutil.h"
#include "matchobj.h"
#include "solver.h"
#include "verify.h"
#include "tic.h"
#include "solvedclient.h"
#include "solvedfile.h"
#include "svd.h"
#include "blind_wcs.h"
#include "keywords.h"
#include "log.h"

#include "kdtree.h"
#define KD_DIM 4
#include "kdtree.h"
#undef KD_DIM

static void find_field_boundaries(solver_t* solver);

static inline double getx(const double* d, uint ind)
{
	return d[ind*2];
}
static inline double gety(const double* d, uint ind)
{
	return d[ind*2 + 1];
}
static inline void setx(double* d, uint ind, double val)
{
	d[ind*2] = val;
}
static inline void sety(double* d, uint ind, double val)
{
	d[ind*2 + 1] = val;
}

static void field_getxy(solver_t* sp, int index, double* x, double* y) {
	*x = sp->field[2*index];
	*y = sp->field[2*index+1];
}

static double field_getx(solver_t* sp, int index) {
	return sp->field[2*index];
}
static double field_gety(solver_t* sp, int index) {
	return sp->field[2*index+1];
}

static void update_timeused(solver_t* sp) {
	double usertime, systime;
	get_resource_stats(&usertime, &systime, NULL);
	sp->timeused = (usertime + systime) - sp->starttime;
	if (sp->timeused < 0.0)
		sp->timeused = 0.0;
}

void solver_reset_best_match(solver_t* sp) {
	sp->best_logodds = -HUGE_VAL;
	memset(&(sp->best_match), 0, sizeof(MatchObj));
	sp->best_index = NULL;
	sp->best_index_num = 0;
	sp->best_match_solves = FALSE;
	sp->have_best_match = FALSE;
}

void solver_init(solver_t* sp)
{
	memset(sp, 0, sizeof(solver_t));
	sp->index = NULL;
	sp->indexes = pl_new(16);
	sp->num_verified = 0;
	sp->field_minx = sp->field_maxx = 0.0;
	sp->field_miny = sp->field_maxy = 0.0;
	sp->parity = DEFAULT_PARITY;
	sp->codetol = DEFAULT_CODE_TOL;
	sp->record_match_callback = NULL;
}

/*
  Reorder the field stars by placing a grid over the field, sorting by
  brightness within each grid cell, then making sweeps over the grid, taking
  the Nth brightest star in each cell in the Nth sweep.  (And sort the stars
  within a sweep by their absolute brightness.)
 */
void solver_uniformize_field(solver_t* solver, int NX, int NY) {
	il** lists;
	int i, ix, iy;
	double xstep, ystep;
	double* newfield;
	//il* templist;
	int Ntotal;

	// compute {min,max}{x,y}
	find_field_boundaries(solver);

	assert(NX > 0);
	assert(NY > 0);
	lists = malloc(NX * NY * sizeof(il*));
	for (i=0; i<(NX*NY); i++)
		lists[i] = il_new(16);
	newfield = calloc(solver->nfield * 2, sizeof(double));

	// grid cell size
	xstep = (solver->field_maxx - solver->field_minx) / (double)NX;
	ystep = (solver->field_maxy - solver->field_miny) / (double)NY;
	assert(xstep > 0.0);
	assert(ystep > 0.0);

	// place stars in grid cells.
	for (i=0; i<solver->nfield; i++) {
		ix = floor((field_getx(solver, i) - solver->field_minx) / xstep);
		iy = floor((field_gety(solver, i) - solver->field_miny) / ystep);
		assert(ix >= 0);
		assert(iy >= 0);
		assert(ix < NX);
		assert(iy < NY);
		il_append(lists[iy * NX + ix], i);
	}
	// since the stars were originally sorted by brightness, each grid
	// cell is also sorted by brightness.

	// reverse each list so that we can pop() bright stars off the back.
	for (i=0; i<(NX*NY); i++) {
		il_reverse(lists[i]);
		/*
		  int j;
		  templist = il_new(16);
		  for (j=il_size(lists[i])-1; j>=0; j--)
		  il_append(templist, il_get(lists[i], j));
		  il_free(lists[i]);
		  lists[i] = templist;
		*/
	}

	// sweep through the cells...
	Ntotal = 0;
	for (;;) {
		bool allempty = TRUE;
		il* thissweep = il_new(16);

		for (i=0; i<(NX*NY); i++) {
			if (!il_size(lists[i]))
				continue;
			allempty = FALSE;
			il_insert_ascending(thissweep, il_pop(lists[i]));
		}

		for (i=0; i<il_size(thissweep); i++) {
			int ind = il_get(thissweep, i);
			setx(newfield, Ntotal, field_getx(solver, ind));
			sety(newfield, Ntotal, field_gety(solver, ind));
			Ntotal++;
		}

		il_free(thissweep);
		if (allempty)
			break;
	}

	assert(Ntotal == solver->nfield);

	for (i=0; i<(NX*NY); i++)
		il_free(lists[i]);
	free(lists);

	memcpy(solver->field, newfield, solver->nfield * 2 * sizeof(double));
	free(newfield);
}

void solver_transform_corners(solver_t* solver, MatchObj* mo) {
	// transform the corners of the field...
	tan_pixelxy2xyzarr(&(mo->wcstan), solver->field_minx, solver->field_miny, mo->sMin);
	tan_pixelxy2xyzarr(&(mo->wcstan), solver->field_maxx, solver->field_maxy, mo->sMax);
	tan_pixelxy2xyzarr(&(mo->wcstan), solver->field_minx, solver->field_maxy, mo->sMinMax);
	tan_pixelxy2xyzarr(&(mo->wcstan), solver->field_maxx, solver->field_miny, mo->sMaxMin);
	// center and radius...
	star_midpoint(mo->center, mo->sMin, mo->sMax);
	mo->radius = sqrt(distsq(mo->center, mo->sMin, 3));
}

void solver_compute_quad_range(solver_t* sp, index_t* index,
		double* minAB, double* maxAB)
{
	double scalefudge = 0.0; // in pixels

	if (sp->funits_upper != 0.0) {
		*minAB = index->index_scale_lower / sp->funits_upper;

		// compute fudge factor for quad scale: what are the extreme
		// ranges of quad scales that should be accepted, given the
		// code tolerance?

		// -what is the maximum number of pixels a C or D star can move
		//  to singlehandedly exceed the code tolerance?
		// -largest quad
		// -smallest arcsec-per-pixel scale

		// -index_scale_upper * 1/sqrt(2) is the side length of
		//  the unit-square of code space, in arcseconds.
		// -that times the code tolerance is how far a C/D star
		//  can move before exceeding the code tolerance, in arcsec.
		// -that divided by the smallest arcsec-per-pixel scale
		//  gives the largest motion in pixels.
		scalefudge = index->index_scale_upper * M_SQRT1_2 *
		             sp->codetol / sp->funits_upper;
		*minAB -= scalefudge;
		//logverb(bp, "Scale fudge: %g pixels.\n", scalefudge);
		//logmsg("Set minAB to %g\n", *minAB);
	}
	if (sp->funits_lower != 0.0) {
		*maxAB = index->index_scale_upper / sp->funits_lower;
		*maxAB += scalefudge;
		//logmsg("Set maxAB to %g\n", *maxAB);
	}
}

struct potential_quad
{
	bool scale_ok;
	int iA, iB;
	double scale;
	double costheta, sintheta;
	bool* inbox;
	int ninbox;
	double* xy;
};
typedef struct potential_quad pquad;

static void try_all_codes(pquad* pq, double Cx, double Cy, double Dx, double Dy,
		uint iA, uint iB, uint iC, uint iD,
		double *ABCDpix, solver_t* solver);

static void try_all_codes_2(double Cx, double Cy, double Dx, double Dy,
		uint iA, uint iB, uint iC, uint iD,
		double *ABCDpix, solver_t* solver,
		bool current_parity);

static void resolve_matches(kdtree_qres_t* krez, double *query, double *field,
		uint fA, uint fB, uint fC, uint fD,
		solver_t* solver, bool current_parity);

static int solver_handle_hit(solver_t* sp, MatchObj* mo);

static void check_scale(pquad* pq, solver_t* solver)
{
	double Ax, Ay, Bx, By, dx, dy;

	field_getxy(solver, pq->iA, &Ax, &Ay);
	field_getxy(solver, pq->iB, &Bx, &By);
	dx = Bx - Ax;
	dy = By - Ay;
	pq->scale = dx * dx + dy * dy;
	if ((pq->scale < solver->minminAB2) ||
			(pq->scale > solver->maxmaxAB2)) {
		pq->scale_ok = FALSE;
		return ;
	}
	pq->costheta = (dy + dx) / pq->scale;
	pq->sintheta = (dy - dx) / pq->scale;
	pq->scale_ok = TRUE;
}

static void check_inbox(pquad* pq, int start, solver_t* solver)
{
	int i;
	double Ax, Ay;
	field_getxy(solver, pq->iA, &Ax, &Ay);
	// check which C, D points are inside the circle.
	for (i = start; i < pq->ninbox; i++) {
		double r;
		double Cx, Cy, xxtmp;
		double tol = solver->codetol;
		if (!pq->inbox[i])
			continue;
		field_getxy(solver, i, &Cx, &Cy);
		Cx -= Ax;
		Cy -= Ay;
		xxtmp = Cx;
		Cx = Cx * pq->costheta + Cy * pq->sintheta;
		Cy = -xxtmp * pq->sintheta + Cy * pq->costheta;

		// make sure it's in the circle centered at (0.5, 0.5)
		// with radius 1/sqrt(2) (plus codetol for fudge):
		// (x-1/2)^2 + (y-1/2)^2   <=   (r + codetol)^2
		// x^2-x+1/4 + y^2-y+1/4   <=   (1/sqrt(2) + codetol)^2
		// x^2-x + y^2-y + 1/2     <=   1/2 + sqrt(2)*codetol + codetol^2
		// x^2-x + y^2-y           <=   sqrt(2)*codetol + codetol^2
		r = (Cx * Cx - Cx) + (Cy * Cy - Cy);
		if (r > (tol * (M_SQRT2 + tol))) {
			pq->inbox[i] = FALSE;
			continue;
		}
		setx(pq->xy, i, Cx);
		sety(pq->xy, i, Cy);
	}
}

#if defined DEBUGSOLVER
static void print_inbox(pquad* pq)
{
	int i;
	debug("[ ");
	for (i = 0; i < pq->ninbox; i++) {
		if (pq->inbox[i])
			debug("%i ", i);
	}
	debug("] (n %i)\n", pq->ninbox);
}
#else
static void print_inbox(pquad* pq)
{}
#endif


static void find_field_boundaries(solver_t* solver) {
	if ((solver->field_minx == solver->field_maxx) 
			|| (solver->field_miny == solver->field_maxy)) {
		int i;
		for (i = 0; i < solver->nfield; i++) {
			solver->field_minx = MIN(solver->field_minx, field_getx(solver, i));
			solver->field_maxx = MAX(solver->field_maxx, field_getx(solver, i));
			solver->field_miny = MIN(solver->field_miny, field_gety(solver, i));
			solver->field_maxy = MAX(solver->field_maxy, field_gety(solver, i));
		}
	}
	solver->field_diag = hypot(solver->field_maxy - solver->field_miny,
	                       solver->field_maxx - solver->field_minx);
}

void solver_preprocess_field(solver_t* solver) {
	// precompute a kdtree over the field itself
	solver->vf = verify_field_preprocess(solver->field, solver->nfield);
	find_field_boundaries(solver);
}

void solver_free_field(solver_t* solver) {
	verify_field_free(solver->vf);
}

// The real deal-- this is what makes it all happen
void solver_run(solver_t* solver)
{
	uint numxy, iA, iB, iC, iD, newpoint;
	int i;
	double usertime, systime;
	time_t next_timer_callback_time = time(NULL) + 1;
	pquad* pquads;

	get_resource_stats(&usertime, &systime, NULL);
	solver->starttime = usertime + systime;

	numxy = solver->nfield;
	if (numxy < DIM_QUADS) //if there are<4 objects in field, forget it
		return ;
	if (solver->endobj && (numxy > solver->endobj))
		numxy = solver->endobj;
	if (solver->startobj >= numxy)
		return ;

	uint num_indexes = bl_size(solver->indexes);
	double minAB2s[num_indexes];
	double maxAB2s[num_indexes];
	solver->minminAB2 = HUGE_VAL;
	solver->maxmaxAB2 = -HUGE_VAL;
	for (i = 0; i < num_indexes; i++) {
		double minAB, maxAB;
		index_t* index = pl_get(solver->indexes, i);
		// The limits on the size of quads, in field coordinates (pixels),
		// Derived from index_scale_* and funits_*.
		solver_compute_quad_range(solver, index, &minAB, &maxAB);
		minAB2s[i] = square(minAB);
		maxAB2s[i] = square(maxAB);
		solver->minminAB2 = MIN(solver->minminAB2, minAB2s[i]);
		solver->maxmaxAB2 = MAX(solver->maxmaxAB2, maxAB2s[i]);

		if (index->cx_less_than_dx) {
			solver->cxdx_margin = 1.5 * solver->codetol;
			// FIXME die horribly the indexes have differing cx_less_than_dx
		}
	}
	logmsg("Quad scale range: [%g, %g] pixels\n", sqrt(solver->minminAB2), sqrt(solver->maxmaxAB2));

	pquads = calloc(numxy * numxy, sizeof(pquad));

	/* We maintain an array of "potential quads" (pquad) structs, where
	 * each struct corresponds to one choice of stars A and B; the struct
	 * at index (B * numxy + A) hold information about quads that could be
	 * created using stars A,B.
	 *
	 * (We only use above-diagonal elements of this 2D array because * A<B.)
	 *
	 * For each AB pair, we cache the scale and the rotation parameters,
	 * and we keep an array "inbox" of length "numxy" of booleans, that say
	 * whether that star is eligible to be star C or D of a quad with AB at
	 * the corners.  (Obviously A and B can't).
	 *
	 * The "ninbox" parameter is somewhat misnamed - it says that "inbox"
	 * in the range [0, ninbox) have been initialized. */

	/* (See explanatory paragraph below) If "solver->startobj" isn't zero,
	 * then we need to initialize the triangle of "pquads" up to
	 * A=startobj-2,B=startobj-1. */
	if (solver->startobj) {
		debug("startobj > 0; priming pquad arrays.\n");
		for (iB = 0; iB < solver->startobj; iB++) {
			for (iA = 0; iA < iB; iA++) {
				pquad* pq = pquads + iB * numxy + iA;
				pq->iA = iA;
				pq->iB = iB;
				debug("trying A=%i, B=%i\n", iA, iB);
				check_scale(pq, solver);
				if (!pq->scale_ok) {
					debug("  bad scale for A=%i, B=%i\n", iA, iB);
					continue;
				}
				pq->xy = malloc(numxy * 2 * sizeof(double));
				pq->inbox = malloc(numxy * sizeof(bool));
				memset(pq->inbox, TRUE, solver->startobj);
				pq->ninbox = solver->startobj;
				pq->inbox[iA] = FALSE;
				pq->inbox[iB] = FALSE;
				check_inbox(pq, 0, solver);
				debug("  inbox(A=%i, B=%i): ", iA, iB);
				print_inbox(pq);
			}
		}
	}

	/* Each time through the "for" loop below, we consider a new star
	 * ("newpoint").  First, we try building all quads that have the new
	 * star on the diagonal (star B).  Then, we try building all quads that
	 * have the star not on the diagonal (star D).
         * 
	 * For each AB pair, we have a "potential_quad" or "pquad" struct.
	 * This caches the computation we need to do: deciding whether the
	 * scale is acceptable, computing the transformation to code
	 * coordinates, and deciding which C,D stars are in the circle.
         * 
	 * We keep the invariants that iA < iB and iC < iD.  The A<->B and
	 * C<->D permutations will be tried in try_all_codes. */

	for (newpoint = solver->startobj; newpoint < numxy; newpoint++) {
		double ABCDpix[8];

		debug("Trying newpoint=%i\n", newpoint);

		// Give our caller a chance to cancel us midway. The callback
		// returns how long to wait before calling again.
		if (solver->timer_callback) {
			time_t delay;
			if (time(NULL) > next_timer_callback_time) {
				delay = solver->timer_callback(solver->userdata);
				if (delay == 0) // Canceled
					break;
				next_timer_callback_time += delay;
			}
		}

		solver->last_examined_object = newpoint;
		// quads with the new star on the diagonal:
		iB = newpoint;
		setx(ABCDpix, 1, field_getx(solver, iB));
		sety(ABCDpix, 1, field_gety(solver, iB));
		debug("Trying quads with B=%i\n", newpoint);
		for (iA = 0; iA < newpoint; iA++) {
			// initialize the "pquad" struct for this AB combo.
			pquad* pq = pquads + iB * numxy + iA;
			pq->iA = iA;
			pq->iB = iB;
			debug("  trying A=%i, B=%i\n", iA, iB);
			check_scale(pq, solver);
			if (!pq->scale_ok) {
				debug("    bad scale for A=%i, B=%i\n", iA, iB);
				continue;
			}
			// initialize the "inbox" array:
			pq->inbox = malloc(numxy * sizeof(bool));
			pq->xy = malloc(numxy * 2 * sizeof(double));
			// -try all stars up to "newpoint"...
			memset(pq->inbox, TRUE, newpoint + 1);
			pq->ninbox = newpoint + 1;
			// -except A and B.
			pq->inbox[iA] = FALSE;
			pq->inbox[iB] = FALSE;
			check_inbox(pq, 0, solver);
			debug("    inbox(A=%i, B=%i): ", iA, iB);
			print_inbox(pq);

			setx(ABCDpix, 0, field_getx(solver, iA));
			sety(ABCDpix, 0, field_gety(solver, iA));

			for (i = 0; i < num_indexes; i++) {
				index_t* index = solver->index = pl_get(solver->indexes, i);
				solver->index_num = i;
				if ((pq->scale < minAB2s[i]) ||
				        (pq->scale > maxAB2s[i]))
					continue;
				solver->index = index;

				for (iC = 0; iC < newpoint; iC++) {
					double cx, cy, dx, dy;
					if (!pq->inbox[iC])
						continue;
					setx(ABCDpix, 2, field_getx(solver, iC));
					sety(ABCDpix, 2, field_gety(solver, iC));
					cx = getx(pq->xy, iC);
					cy = gety(pq->xy, iC);
					for (iD = iC + 1; iD < newpoint; iD++) {
						if (!pq->inbox[iD])
							continue;
						setx(ABCDpix, 3, field_getx(solver, iD));
						sety(ABCDpix, 3, field_gety(solver, iD));
						dx = getx(pq->xy, iD);
						dy = gety(pq->xy, iD);
						solver->numtries++;
						debug("    trying quad [%i %i %i %i]\n", iA, iB, iC, iD);

						try_all_codes(pq, cx, cy, dx, dy, iA, iB, iC, iD, ABCDpix, solver);
						if (solver->quit_now)
							break;
					}
				}
				if (solver->quit_now)
					break;
			}
		}

		if (solver->quit_now)
			break;

		// quads with the new star not on the diagonal:
		iD = newpoint;
		setx(ABCDpix, 3, field_getx(solver, iD));
		sety(ABCDpix, 3, field_gety(solver, iD));
		debug("Trying quads with D=%i\n", newpoint);
		for (iA = 0; iA < newpoint; iA++) {
			for (iB = iA + 1; iB < newpoint; iB++) {
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
				check_inbox(pq, iD, solver);
				if (!pq->inbox[iD]) {
					debug("  D is not in the box for A=%i, B=%i\n", iA, iB);
					continue;
				}
				debug("  D is in the box for A=%i, B=%i\n", iA, iB);
				setx(ABCDpix, 0, field_getx(solver, iA));
				sety(ABCDpix, 0, field_gety(solver, iA));
				setx(ABCDpix, 1, field_getx(solver, iB));
				sety(ABCDpix, 1, field_gety(solver, iB));
				dx = getx(pq->xy, iD);
				dy = gety(pq->xy, iD);

				for (i = 0; i < pl_size(solver->indexes); i++) {
					index_t* index = solver->index = pl_get(solver->indexes, i);
					solver->index_num = i;
					if ((pq->scale < minAB2s[i]) ||
					        (pq->scale > maxAB2s[i]))
						continue;
					solver->index = index;

					for (iC = 0; iC < newpoint; iC++) {
						if (!pq->inbox[iC])
							continue;
						setx(ABCDpix, 2, field_getx(solver, iC));
						sety(ABCDpix, 2, field_gety(solver, iC));
						cx = getx(pq->xy, iC);
						cy = gety(pq->xy, iC);
						solver->numtries++;
						debug("  trying quad [%i %i %i %i]\n", iA, iB, iC, iD);
						try_all_codes(pq, cx, cy, dx, dy, iA, iB, iC, iD, ABCDpix, solver);
						if (solver->quit_now)
							break;
					}
					if (solver->quit_now)
						break;
				}
			}
		}

		logmsg("object %u of %u: %i quads tried, %i matched.\n",
					 newpoint + 1, numxy, solver->numtries, solver->nummatches);

		if ((solver->maxquads && (solver->numtries >= solver->maxquads))
				|| (solver->maxmatches && (solver->nummatches >= solver->maxmatches))
				|| solver->quit_now)
			break;
	}

	for (i = 0; i < (numxy*numxy); i++) {
		pquad* pq = pquads + i;
		free(pq->inbox);
		free(pq->xy);
	}
	free(pquads);
}

static inline void set_xy(double* dest, int destind, double* src, int srcind)
{
	setx(dest, destind, getx(src, srcind));
	sety(dest, destind, gety(src, srcind));
}

static void try_all_codes(pquad* pq, double Cx, double Cy, double Dx, double Dy,
                          uint iA, uint iB, uint iC, uint iD,
                          double *ABCDpix, solver_t* solver)
{
	debug("    code=[%g,%g,%g,%g].\n", Cx, Cy, Dx, Dy);

	if (solver->parity == PARITY_NORMAL ||
	        solver->parity == PARITY_BOTH) {
		debug("    trying normal parity: code=[%g,%g,%g,%g].\n", Cx, Cy, Dx, Dy);
		try_all_codes_2(Cx, Cy, Dx, Dy, iA, iB, iC, iD, ABCDpix, solver, FALSE);
	}
	if (solver->parity == PARITY_FLIP ||
	        solver->parity == PARITY_BOTH) {
		debug("    trying reverse parity: code=[%g,%g,%g,%g].\n", Cy, Cx, Dy, Dx);
		try_all_codes_2(Cy, Cx, Dy, Dx, iA, iB, iC, iD, ABCDpix, solver, TRUE);
	}
}

static void try_all_codes_2(double Cx, double Cy, double Dx, double Dy,
                            uint iA, uint iB, uint iC, uint iD,
                            double *ABCDpix, solver_t* solver,
                            bool current_parity)
{
	int i,j;
	kdtree_qres_t* result = NULL;
	double tol = square(solver->codetol);
	double inorder[8];
	int A = 0, B = 1, C = 2, D = 3;
	int options = KD_OPTIONS_SMALL_RADIUS | KD_OPTIONS_COMPUTE_DISTS |
	              KD_OPTIONS_NO_RESIZE_RESULTS;

	update_timeused(solver);

	double thequeries[4][4] = {
		{   Cx,    Cy,    Dx,    Dy}, 
		{1.-Cx, 1.-Cy, 1.-Dx, 1.-Dy}, 
		{   Dx,    Dy,    Cx,    Cy}, 
		{1.-Dx, 1.-Dy, 1.-Cx, 1.-Cy}
	};
	int inds[4] = {iA, iB, iC, iD};
	// maps 0-3 to our current permutation for this iteration
	int perm[4][4] = {
		{A,B,C,D},
		{B,A,C,D},
		{A,B,D,C},
		{B,A,D,C}
	};

	for (i=0; i<4; i++) {
		if ((!solver->index->cx_less_than_dx) ||
				(thequeries[i][0] <= (thequeries[i][2] + solver->cxdx_margin))) {

			for (j=0; j<4; j++)
				set_xy(inorder, j, ABCDpix, perm[i][j]);

			result = kdtree_rangesearch_options_reuse(solver->index->codekd->tree, result, thequeries[i], tol, options);

			debug("      trying ABCD = [%i %i %i %i]: %i results.\n",
					inds[perm[i][A]], inds[perm[i][B]], inds[perm[i][C]], inds[perm[i][D]],
					result->nres);

			if (result->nres)
				resolve_matches(result, thequeries[i], inorder,
						inds[perm[i][A]], inds[perm[i][B]], inds[perm[i][C]], inds[perm[i][D]],
						solver, current_parity);
			if (solver->quit_now)
				goto quit_now;
		} else
			solver->num_cxdx_skipped++;
	}

quit_now:
	kdtree_free_query(result);
}

// "field" contains the xy pixel coordinates of stars A,B,C,D.
static void resolve_matches(kdtree_qres_t* krez, double *query, double *field,
                            uint fA, uint fB, uint fC, uint fD,
                            solver_t* solver, bool current_parity)
{
	uint jj, thisquadno;
	uint iA, iB, iC, iD;
	MatchObj mo;

	for (jj = 0; jj < krez->nres; jj++) {
		double star[12];
		double scale;
		double arcsecperpix;
		tan_t wcs;

		solver->nummatches++;

		thisquadno = (uint)krez->inds[jj];

		quadfile_get_starids(solver->index->quads, thisquadno, &iA, &iB, &iC, &iD);
		startree_get(solver->index->starkd, iA, star + 0*3);
		startree_get(solver->index->starkd, iB, star + 1*3);
		startree_get(solver->index->starkd, iC, star + 2*3);
		startree_get(solver->index->starkd, iD, star + 3*3);

		debug("        stars [%i %i %i %i]\n", iA, iB, iC, iD);

		// compute TAN projection from the matching quad alone.
		blind_wcs_compute_2(star, field, 4, &wcs, &scale);
		arcsecperpix = scale * 3600.0;

		// FIXME - should there be scale fudge here?
		if (arcsecperpix > solver->funits_upper ||
		        arcsecperpix < solver->funits_lower) {
			debug("          bad scale.\n");
			continue;
		}

		solver->numscaleok++;

		if (solver->mo_template)
			memcpy(&mo, solver->mo_template, sizeof(MatchObj));

		memcpy(&(mo.wcstan), &wcs, sizeof(tan_t));
		mo.wcs_valid = TRUE;
		mo.code_err = krez->sdists[jj];
		mo.scale = arcsecperpix;
		mo.parity = current_parity;
		mo.quads_tried = solver->numtries;
		mo.quads_matched = solver->nummatches;
		mo.quads_scaleok = solver->numscaleok;
        mo.quad_npeers = krez->nres;
		mo.timeused = solver->timeused;
		mo.quadno = thisquadno;
		mo.star[0] = iA;
		mo.star[1] = iB;
		mo.star[2] = iC;
		mo.star[3] = iD;
		mo.field[0] = fA;
		mo.field[1] = fB;
		mo.field[2] = fC;
		mo.field[3] = fD;

		memcpy(mo.quadpix, field, 8 * sizeof(double));
		memcpy(mo.quadxyz, star, 12 * sizeof(double));

		if (solver->index->idfile) {
			mo.ids[0] = idfile_get_anid(solver->index->idfile, iA);
			mo.ids[1] = idfile_get_anid(solver->index->idfile, iB);
			mo.ids[2] = idfile_get_anid(solver->index->idfile, iC);
			mo.ids[3] = idfile_get_anid(solver->index->idfile, iD);
		} else {
			mo.ids[0] = 0;
			mo.ids[1] = 0;
			mo.ids[2] = 0;
			mo.ids[3] = 0;
		}

		solver_transform_corners(solver, &mo);

		if (solver_handle_hit(solver, &mo))
			solver->quit_now = TRUE;

		if (solver->quit_now)
			return ;
	}
}

void solver_inject_match(solver_t* solver, MatchObj* mo) {
	solver_handle_hit(solver, mo);
}

static int solver_handle_hit(solver_t* sp, MatchObj* mo)
{
	double match_distance_in_pixels2;

	mo->indexid = sp->index->indexid;
	mo->healpix = sp->index->healpix;

	match_distance_in_pixels2 = square(sp->verify_pix) +
		square(sp->index->index_jitter / mo->scale);

	verify_hit(sp->index->starkd, mo, sp->vf, match_distance_in_pixels2,
	           sp->distractor_ratio, sp->field_maxx, sp->field_maxy,
	           sp->logratio_bail_threshold, sp->distance_from_quad_bonus);
	mo->nverified = sp->num_verified++;

	if (mo->logodds >= sp->best_logodds) {
		sp->best_logodds = mo->logodds;
	}

	if (!sp->have_best_match || (mo->logodds > sp->best_match.logodds)) {
		logmsg("Got a new best match: logodds %g.\n", mo->logodds);
		//print_match(bp, mo);
		memcpy(&(sp->best_match), mo, sizeof(MatchObj));
		sp->have_best_match = TRUE;
		sp->best_index = sp->index;
		sp->best_index_num = sp->index_num;
	}

	if (mo->logodds < sp->logratio_record_threshold) {
		return FALSE;
	}

	update_timeused(sp);
	mo->timeused = sp->timeused;

	bool bail = sp->record_match_callback(mo, sp->userdata);

	if (bail) {
		sp->best_match_solves = TRUE;
		return TRUE;
	}
	return FALSE;
}

solver_t* solver_new() {
	solver_t* solver = calloc(1, sizeof(solver_t));
	solver_set_default_values(solver);
	return solver;
}

void solver_set_default_values(solver_t* solver) {
//	if (solver->indexes) {
//		int i;
//		for (i=0; i<pl_size(solver->indexes); i++)
//			index_close(pl_get(solver->indexes, i));
//		pl_clear(solver->indexes);
//	} else {
//	}
	memset(solver, 0, sizeof(solver_t));
	solver->indexes = pl_new(16);
	solver->funits_upper = HUGE_VAL;
	solver->logratio_bail_threshold = log(1e-100);
	solver->parity = DEFAULT_PARITY;
	solver->codetol = DEFAULT_CODE_TOL;
}
