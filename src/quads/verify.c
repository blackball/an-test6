/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <assert.h>
#include <math.h>
#include <string.h>

#include "verify.h"
#include "mathutil.h"
#include "intmap.h"
#include "pnpoly.h"

/*
  #include "kdtree.h"
  #define KD_DIM 3
  #include "kdtree.h"
  #undef KD_DIM
*/

#include "solver_callbacks.h"
#include "blind_wcs.h"

void verify_iterate_wcs(kdtree_t* startree, MatchObj* mo,
						double* field, int nfield,
						double verify_dist2,
						int* corr_IGNORE,
						int nsteps,
						double* rads) {

	kdtree_qres_t* res;
	double vec1[3], vec2[3], len1, len2;
	double polyx[4], polyy[4];
	double fieldcenter[3];
	double fieldr2;
	// number of stars in the index that are within the bounds of the field.
	int NI;
	kdtree_t* itree;
	int Nleaf = 5;
	int s;
	int i, j;
	//double* fieldstars;
	//double* fieldxy;
	double quadcenter[3];
	double quadr2;

	// Find all index stars within range.
	// compute vec1 and vec2, two vectors parallel to the two edges of the
	// field.
	for (i=0; i<3; i++) {
		vec1[i] = mo->sMinMax[i] - mo->sMin[i];
		vec2[i] = mo->sMaxMin[i] - mo->sMin[i];
	}
	// compute len1 and len2, the coordinates of the far corner of the
	// field when projected on vec1 and vec2.  These define the bounds of
	// the field.
	len1 = len2 = 0.0;
	for (i=0; i<3; i++) {
		len1 += (mo->sMax[i] - mo->sMin[i]) * vec1[i];
		len2 += (mo->sMax[i] - mo->sMin[i]) * vec2[i];
	}

	// compute the corners of the field polygon:
	// sMin
	polyx[0] = 0.0;
	polyy[0] = 0.0;
	// sMinMax
	polyx[1] = vec1[0]*vec1[0] + vec1[1]*vec1[1] + vec1[2]*vec1[2];
	polyy[1] = vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
	// sMax
	polyx[2] = len1;
	polyy[2] = len2;
	// sMaxMin
	polyx[3] = vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
	polyy[3] = vec2[0]*vec2[0] + vec2[1]*vec2[1] + vec2[2]*vec2[2];

	// find all the index stars that are inside the circle that bounds
	// the field.
	for (i=0; i<3; i++)
		fieldcenter[i] = (mo->sMin[i] + mo->sMax[i]) / 2.0;
	fieldr2 = distsq(fieldcenter, mo->sMin, 3);
	// 1.01 is a little safety factor.
	res = kdtree_rangesearch_options(startree, fieldcenter, fieldr2 * 1.01,
									 KD_OPTIONS_SMALL_RADIUS);
	assert(res);

	// for each of the index stars, project them into field coordinates
	// and check if they lie inside the field bounding box.
	NI = 0;
	for (j=0; j<res->nres; j++) {
		double l1 = 0.0, l2 = 0.0;
		for (i=0; i<3; i++) {
			l1 += (res->results.d[j*3 + i] - mo->sMin[i]) * vec1[i];
			l2 += (res->results.d[j*3 + i] - mo->sMin[i]) * vec2[i];
		}
		if (point_in_poly(polyx, polyy, 4, l1, l2)) {
			if (j != NI) {
				memmove(res->results.d + NI * 3,
						res->results.d +  j * 3,
						3 * sizeof(double));
				res->inds[NI] = res->inds[j];
			}
			NI++;
		}
	}

	// Build a tree out of the index stars...
	itree = kdtree_build(NULL, res->results.d, NI, 3, Nleaf, KDTT_DOUBLE, KD_BUILD_BBOX);

	//fieldstars = malloc(3 * nfield * sizeof(double));
	/*
	  for (i=0; i<4; i++) {
	  double xyz[3];
	  if (!i)
	  getstarcoord(mo->star[i], quadcenter);
	  else {
	  getstarcoord(mo->star[i], xyz);
	  for (j=0; j<3; j++)
	  quadcenter[j] += xyz[j];
	  }
	  }
	  normalize_3(quadcenter);
	*/

	{
		double starA[3], starB[3];
		getstarcoord(mo->star[0], starA);
		getstarcoord(mo->star[1], starB);
		star_midpoint(quadcenter, starA, starB);
		quadr2 = 0.25 * distsq(starA, starB, 3);
	}

	/*
	  fprintf(stderr, "Quad radius: %f arcsec.\n",
	  distsq2arcsec(quadr2));
	*/

	fprintf(stderr, "Step 0: overlap %.2f.\n", mo->overlap*100.0);

	for (s=0; s<nsteps; s++) {
		double maxd2;
		int ncorr = 0;
		double starxyz[3*nfield];
		double fieldxy[2*nfield];

		// Find stars within rads[s] fraction of the field size...
		//maxd2 = square(rads[s]) * distsq(mo->sMin, mo->sMax, 3);

		// Find stars within rads[s] quad radiuses of the quad center...
		maxd2 = square(rads[s]) * quadr2;

		// Project the field stars into star space...
		for (i=0; i<nfield; i++) {
			double u, v;
			int nn;
			double fxyz[3];
			u = field[i*2];
			v = field[i*2+1];
			//fxyz = fieldxyz + 3*ncorr;
			tan_pixelxy2xyzarr(&(mo->wcstan), u, v, fxyz);

			/*
			  fprintf(stderr, "  field obj %i: quad dist %f, arcsec %f.\n",
			  i, sqrt(distsq(quadcenter, fxyz, 3) / quadr2),
			  distsq2arcsec(distsq(quadcenter, fxyz, 3)));
			*/

			// If the star is not within range of the quad center, skip it.
			if (distsq(quadcenter, fxyz, 3) > maxd2)
				continue;

			// Find the nearest index star...
			nn = kdtree_nearest_neighbour_within(itree, fxyz, verify_dist2, NULL);
			if (nn == -1)
				continue;

			kdtree_copy_data_double(itree, nn, 1, starxyz + 3*ncorr);

			/*
			  fprintf(stderr, "  nn ind %i: dist %f arcsec.\n",
			  nn, distsq2arcsec(distsq(fxyz, starxyz+3*ncorr, 3)));
			*/

			fieldxy[ncorr*2  ] = u;
			fieldxy[ncorr*2+1] = v;
			ncorr++;
		}

		blind_wcs_compute_2(starxyz, fieldxy, ncorr, &(mo->wcstan));

		verify_hit(startree, mo, field, nfield, verify_dist2,
				   NULL, NULL, NULL, NULL, NULL, NULL);

		fprintf(stderr, "Step %i: rad %f, ncorr %i, overlap %.2f\n",
				s+1, rads[s], ncorr, mo->overlap*100.0);
	}

	kdtree_free(itree);
	kdtree_free_query(res);
}


void verify_hit(kdtree_t* startree,
				MatchObj* mo,
				double* field,
				int NF,
				double verify_dist2,
				int* pmatches,
				int* punmatches,
				int* pconflicts,
				il* indexstars,
				dl* bestd2s,
				int* correspondences) {
	int i, j;
	double* fieldstars;
	intmap* map;
	int matches;
	int unmatches;
	int conflicts;
	double fieldcenter[3];
	double fieldr2;
	kdtree_qres_t* res;
	double vec1[3], vec2[3], len1, len2;
	// number of stars in the index that are within the bounds of the field.
	int NI;
	kdtree_t* itree;
	int Nleaf = 5;
	double* dptr;
	int Nmin;
	double polyx[4], polyy[4];

	assert(mo->transform_valid || mo->wcs_valid);
	assert(startree);

	/* We project the points into a 2D space whose origin is "sMin"
	   and whose "unit vectors" are:
	   .  vec1 = sMinMax - sMin
	   .  vec2 = sMaxMin - sMin
	   Note that these are neither unit length nor orthogonal, but that's
	   okay.
	   We project the corners of the field, and also the stars into this
	   space and do a point-in-polygon test.
	*/

	// compute vec1 and vec2, two vectors parallel to the two edges of the
	// field.
	for (i=0; i<3; i++) {
		vec1[i] = mo->sMinMax[i] - mo->sMin[i];
		vec2[i] = mo->sMaxMin[i] - mo->sMin[i];
	}
	// compute len1 and len2, the coordinates of the far corner of the
	// field when projected on vec1 and vec2.  These define the bounds of
	// the field.
	len1 = len2 = 0.0;
	for (i=0; i<3; i++) {
		len1 += (mo->sMax[i] - mo->sMin[i]) * vec1[i];
		len2 += (mo->sMax[i] - mo->sMin[i]) * vec2[i];
	}

	// compute the corners of the field polygon:
	// sMin
	polyx[0] = 0.0;
	polyy[0] = 0.0;
	// sMinMax
	polyx[1] = vec1[0]*vec1[0] + vec1[1]*vec1[1] + vec1[2]*vec1[2];
	polyy[1] = vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
	// sMax
	polyx[2] = len1;
	polyy[2] = len2;
	// sMaxMin
	polyx[3] = vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
	polyy[3] = vec2[0]*vec2[0] + vec2[1]*vec2[1] + vec2[2]*vec2[2];

	// find all the index stars that are inside the circle that bounds
	// the field.
	for (i=0; i<3; i++)
		fieldcenter[i] = (mo->sMin[i] + mo->sMax[i]) / 2.0;
	fieldr2 = distsq(fieldcenter, mo->sMin, 3);
	// 1.01 is a little safety factor.
	res = kdtree_rangesearch_options(startree, fieldcenter, fieldr2 * 1.01,
									 KD_OPTIONS_SMALL_RADIUS);
	assert(res);

	// for each of the index stars, project them into field coordinates
	// and check if they lie inside the field bounding box.
	NI = 0;
	for (j=0; j<res->nres; j++) {
		double l1 = 0.0, l2 = 0.0;
		for (i=0; i<3; i++) {
			l1 += (res->results.d[j*3 + i] - mo->sMin[i]) * vec1[i];
			l2 += (res->results.d[j*3 + i] - mo->sMin[i]) * vec2[i];
		}
		if (point_in_poly(polyx, polyy, 4, l1, l2)) {
			if (j != NI) {
				memmove(res->results.d + NI * 3,
						res->results.d +  j * 3,
						3 * sizeof(double));
				res->inds[NI] = res->inds[j];
			}
			if (indexstars)
				il_append(indexstars, res->inds[j]);
			NI++;
		}
	}

	if (!NI) {
		// I don't know HOW this happens - at the very least, the four stars
		// belonging to the quad that generated this hit should lie in the
		// proposed field - but I've seen it happen!
		fprintf(stderr, "Freakishly, NI=0.\n");
		mo->ninfield = 0;
		mo->noverlap = 0;
		matchobj_compute_overlap(mo);
		if (pmatches)
			*pmatches = 0;
		if (punmatches)
			*punmatches = NF;
		if (pconflicts)
			*pconflicts = 0;
		kdtree_free_query(res);
		return;
	}

	// We look at the minimum of the number of stars in the field and the number
	// of stars in the index in that region.
	Nmin = imin(NI, NF);

	// Project the field stars into star space...
	fieldstars = malloc(3 * Nmin * sizeof(double));
	dptr = field;
	for (i=0; i<Nmin; i++) {
		double u, v;
		u = *dptr;
		dptr++;
		v = *dptr;
		dptr++;
		if (mo->wcs_valid)
			tan_pixelxy2xyzarr(&(mo->wcstan), u, v, fieldstars + 3*i);
		else if (mo->transform_valid)
			image_to_xyz(u, v, fieldstars + 3*i, mo->transform);
	}

	// Build a tree out of the index stars...
	itree = kdtree_build(NULL, res->results.d, NI, 3, Nleaf, KDTT_DOUBLE, KD_BUILD_BBOX);

	matches = unmatches = conflicts = 0;
	map = intmap_new(INTMAP_ONE_TO_ONE);

	// insert dual-tree here :)
	// For each field star, find the nearest neighbour (within the verify_dist) among the field
	// stars.
	// If two field stars try to claim the same index star, it's counted as a conflict.
	for (i=0; i<Nmin; i++) {
		double bestd2;
		int ind = kdtree_nearest_neighbour_within(itree, fieldstars + 3*i, verify_dist2, &bestd2);
		if (ind == -1) {
			unmatches++;
		} else {
			if (intmap_add(map, ind, i) == -1)
				// a field object already selected star 'ind' as its nearest neighbour.
				conflicts++;
			else {
				matches++;
				if (correspondences)
					correspondences[i] = res->inds[ind];
			}
			if (bestd2s)
				dl_append(bestd2s, bestd2);
		}
	}

	kdtree_free(itree);
	kdtree_free_query(res);

	mo->noverlap = matches;
	mo->nconflict = conflicts;
	mo->ninfield = Nmin;
	matchobj_compute_overlap(mo);

	if (pmatches)
		*pmatches = matches;
	if (punmatches)
		*punmatches = unmatches;
	if (pconflicts)
		*pconflicts = conflicts;

	intmap_free(map);
	free(fieldstars);
}
