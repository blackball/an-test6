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

#include <assert.h>
#include <math.h>
#include <string.h>

#include "verify.h"
#include "mathutil.h"
#include "intmap.h"
#include "pnpoly.h"
#include "keywords.h"

#define min(a,b) (((a)<(b))?(a):(b))

#include "solver_callbacks.h"
#include "blind_wcs.h"

// FOR TESTING
static void write_prob_terrain(kdtree_t* itree, int NF, int NI,
							   double fieldarea, double logprob_background,
							   double logprob_distractor, double* qc,
							   double verify_pix2, double rquad2,
							   double* field);

#define DEBUG 0
#if DEBUG
#define debug(args...) fprintf(stderr, args)
#else
#define debug(args...)
#endif

void verify_hit(kdtree_t* startree,
				MatchObj* mo,
				double* field,
				int NF,
				double verify_pix2,
				double distractors,
				double fieldW,
				double fieldH,
				double logratio_tobail,
				int min_nfield) {
	int i, j;
	double fieldcenter[3];
	double fieldr2;
	kdtree_qres_t* res;
	// number of stars in the index that are within the bounds of the field.
	int NI;
	kdtree_t* itree;
	int Nleaf = 5;
	double* indexpix;

	intmap* map;

	// quad center and radius
	double qc[2];
	double rquad2;

	double logprob_distractor;
	double logprob_background;

	double logodds = 0.0;
	int nmatch, nnomatch, nconflict;

	double bestlogodds;
	int bestnmatch, bestnnomatch, bestnconflict;

	// FIXME - the value 1.5 is simulation-based.
	double gamma2 = square(1.5);

	int Nmin;

	double crvalxyz[3];

	assert(mo->wcs_valid);
	assert(startree);

	// find all the index stars that are inside the circle that bounds
	// the field.
	star_midpoint(fieldcenter, mo->sMin, mo->sMax);
	fieldr2 = distsq(fieldcenter, mo->sMin, 3);

	// 1.01 is a little safety factor.
	res = kdtree_rangesearch_options(startree, fieldcenter, fieldr2 * 1.01,
									 KD_OPTIONS_SMALL_RADIUS);
	assert(res);

	// Project index stars into pixel space.
	indexpix = malloc(res->nres * 2 * sizeof(double));
	NI = 0;
	radecdeg2xyzarr(mo->wcstan.crval[0], mo->wcstan.crval[1], crvalxyz);
	for (i=0; i<res->nres; i++) {
		double x, y;
		double dot = dot_product_3(crvalxyz, res->results.d + i*3);
		if (dot < 0.0)
			continue;
		tan_xyzarr2pixelxy(&(mo->wcstan), res->results.d + i*3, &x, &y);
		if ((x < 0) || (y < 0) || (x >= fieldW) || (y >= fieldH))
			continue;

		res->inds[NI] = res->inds[i];

		if (DEBUG)
			memmove(res->results.d + NI*3,
					res->results.d + i*3,
					3*sizeof(double));

		indexpix[NI*2  ] = x;
		indexpix[NI*2+1] = y;
		NI++;
	}
	indexpix = realloc(indexpix, NI * 2 * sizeof(double));

	debug("Number of field stars: %i\n", NF);
	debug("Number of index stars: %i\n", NI);

	if (!NI) {
		// I don't know HOW this happens - at the very least, the four stars
		// belonging to the quad that generated this hit should lie in the
		// proposed field - but I've seen it happen!
		fprintf(stderr, "Freakishly, NI=0.\n");
		mo->nfield = 0;
		mo->noverlap = 0;
		matchobj_compute_overlap(mo);
		mo->logodds = -HUGE_VAL;
		kdtree_free_query(res);
		free(indexpix);
		return;
	}

	// Build a tree out of the index stars in pixel space...
	itree = kdtree_build(NULL, indexpix, NI, 2, Nleaf, KDTT_DOUBLE, KD_BUILD_BBOX);

	// Find the center of the quad in pixel space. 
	// FIXME why not take the average of AB? then the radius will be right?
	// -the WCS tan projection is computed wrt the center of mass, so I
	//  thought I would use that here as well... it seemed like a good idea
	//  at the time, but maybe you're right :)
	qc[0] = qc[1] = 0.0;
	for (i=0; i<4; i++) {
		qc[0] += field[2*mo->field[i]];
		qc[1] += field[2*mo->field[i]+1];
	}
	qc[0] *= 0.25;
	qc[1] *= 0.25;

	// Find the radius-squared of the quad ~= distsq(qc, fA)
	rquad2 = distsq(field + 2*mo->field[0], qc, 2);

	// p(background) = 1/(W*H) of the image.
	logprob_background = -log(fieldW * fieldH);

	logprob_distractor = logprob_background + log(distractors);

	debug("log(p(background)) = %g\n", logprob_background);
	debug("log(p(distractor)) = %g\n", logprob_distractor);

	if (0) {
		write_prob_terrain(itree, NF, NI, fieldW*fieldH, logprob_background,
						   logprob_distractor, qc, verify_pix2, rquad2,
						   field);
	}

	nmatch = 0;
	nnomatch = 0;
	nconflict = 0;
	map = intmap_new(INTMAP_ONE_TO_ONE);

	// "prime" the intmap with the matched quad so that no other field objs
	// can claim them.  (This is to prevent "lucky donuts")
	for (j=0; j<4; j++) {
		intmap_add(map, mo->star[j], mo->field[j]);
		debug("Priming: %i -> %i.\n", mo->star[j], mo->field[j]);
		if (DEBUG) {
			int k;
			int* invperm = malloc(NI * sizeof(int));
			int ind = -1;
			double xy[2];
			kdtree_inverse_permutation(itree, invperm);
			for (k=0; k<NI; k++) {
				if (res->inds[k] == mo->star[j]) {
					tan_xyzarr2pixelxy(&(mo->wcstan), res->results.d + k*3, xy, xy+1);
					ind = invperm[k];
					break;
				}
			}
			if (ind != -1) {
				Unused double d2 = distsq(field + mo->field[j]*2, indexpix + ind*2, 2);
				Unused double d2b = distsq(field + mo->field[j]*2, xy, 2);
				debug("Dist: %g, %g\n", sqrt(d2), sqrt(d2b));
			}
			free(invperm);
		}
	}

	Nmin = min(NI, NF);

	bestlogodds = -HUGE_VAL;
	bestnmatch = bestnnomatch = bestnconflict = -1;

	// For each field star, find the nearest index star.
	//for (i=0; i<NF; i++) {
	for (i=0; i<Nmin; i++) {
		double bestd2;
		double sigma2;
		double cutoffd2;
		double R2;
		double logprob = -HUGE_VAL;
		int ind;

		// Skip stars that are part of the quad:
		if (i == mo->field[0] || i == mo->field[1] || i == mo->field[2] || i == mo->field[3])
			continue;

		// Distance from the quad center of this field star:
		R2 = distsq(field+i*2, qc, 2);

		// Variance of a field star at that distance from the quad center:
		sigma2 = verify_pix2 * (gamma2 + R2/rquad2);

		// Cutoff distance to nearest neighbour star: p(fg) == p(bg)
		// OK, I think that should be cutoffd2= -2*sigma2*(logbg - log(2*pi) - .5*log(sigma2))
		// FIXME - just set it to 10 sigmas.
		cutoffd2 = 100.0 * sigma2;

		ind = kdtree_nearest_neighbour_within(itree, field+i*2, cutoffd2, &bestd2);

		if (ind != -1) {
			// p(foreground):
			logprob = log((1.0 - distractors) / (2.0 * M_PI * sigma2 * NI)) - (bestd2 / (2.0 * sigma2));
			//logprob = log((1.0 - distractors) / (2.0 * M_PI * sigma2 * Nmin)) - (bestd2 / (2.0 * sigma2));
		}

		debug("\nField obj %i/%i: rad %g quads, sigma %g.\n", i+1, NF, sqrt(R2/rquad2), sqrt(sigma2));
		debug("NN dist: %5.1f pix, %g sigmas\n", sqrt(bestd2), sqrt(bestd2/sigma2));

 		if ((ind == -1) || (logprob < logprob_distractor)) {
			debug("Distractor.\n");
			logprob = logprob_distractor;
			nnomatch++;
		} else {
			int resind = itree->perm[ind];
			int starkdind = res->inds[resind];
			debug("NN: %i -> %i\n", starkdind, i);
			if (intmap_add(map, starkdind, i) == -1) {
				// a field object already selected star 'ind' as its nearest neighbour.
				double oldd2;
				bool partofquad;
				int oldfieldi = intmap_get(map, starkdind, -1);
				bool update = FALSE;
				assert(oldfieldi != -1);

				debug("Conflict (ind %i, resind %i, starkdind %i)\n",
					  ind, resind, starkdind);

				partofquad = (starkdind==mo->star[0] ||
							  starkdind==mo->star[1] ||
							  starkdind==mo->star[2] ||
							  starkdind==mo->star[3]);

				// Allow a poor match to be replaced by a better one!
				// (except for conflicts with the quad!)
				if (partofquad) {
					debug("Conflict with matched quad.\n");
				} else {
					oldd2 = distsq(field + oldfieldi*2, indexpix + ind*2, 2);
					debug("Old dist %g, New dist %g.\n", sqrt(oldd2), sqrt(bestd2));
					if (bestd2 < oldd2) {
						double oldR2;
						double oldsigma2;
						double oldlogprob;
						oldR2 = distsq(field+oldfieldi*2, qc, 2);
						oldsigma2 = verify_pix2 * (gamma2 + oldR2/rquad2);
						oldlogprob = log((1.0 - distractors) / (2.0 * M_PI * oldsigma2 * NI)) - (oldd2 / (2.0 * oldsigma2));
						//oldlogprob = log((1.0 - distractors) / (2.0 * M_PI * oldsigma2 * Nmin)) - (oldd2 / (2.0 * oldsigma2));
						debug("Updating logprob from %g to %g.\n", oldlogprob, logprob);
						logodds -= (oldlogprob - logprob_background);
						intmap_update(map, starkdind, i);
						update = TRUE;
					}
				}
				nconflict++;
				if (!update)
					continue;
			} else {
				debug("Match (index star %i), logprob %g\n", ind, logprob);
				nmatch++;
			}
		}

		logodds += (logprob - logprob_background);
		debug("Logodds now %g\n", logodds);
		
		if (logodds < logratio_tobail) {
			break;
		}

		if ((logodds > bestlogodds) &&
			((i-nconflict) >= min_nfield)) {
			bestlogodds = logodds;
			bestnmatch = nmatch;
			bestnnomatch = nnomatch;
			bestnconflict = nconflict;
		}

	}

	/*
	  fprintf(stderr, "Final logodds: %g\n", logodds);
	  fprintf(stderr, "Best logodds: %g (%i field objs), ", bestlogodds, nbest+1);
	  fprintf(stderr, "CRVAL: %g, %g\n", mo->wcstan.crval[0], mo->wcstan.crval[1]);
	  fprintf(stderr, "%g,%i,%g,%g; %%%%%%\n", bestlogodds, nbest+1, mo->wcstan.crval[0], mo->wcstan.crval[1]);
	*/

	kdtree_free_query(res);
	intmap_free(map);
	kdtree_free(itree);
	free(indexpix);

	mo->logodds = bestlogodds;
	mo->noverlap = bestnmatch;
	mo->nconflict = bestnconflict;
	mo->nfield = bestnmatch + bestnconflict + bestnnomatch;
	/*
	  mo->logodds = logodds;
	  mo->noverlap = nmatch;
	  mo->nconflict = nconflict;
	  mo->nfield = NF;
	*/
	mo->nindex = NI;
	matchobj_compute_overlap(mo);
}





// TEST
static void writeFloat(FILE* fid, float f) {
	if (fwrite(&f, sizeof(float), 1, fid) != 1) {
		fprintf(stderr, "failed to write float.\n");
		exit(-1);
	}
}
static void write_prob_terrain(kdtree_t* itree, int NF, int NI,
							   double fieldarea, double logprob_background,
							   double logprob_distractor, double* qc,
							   double verify_pix2, double rquad2,
							   double* field) {
	int x1, x2, y1, y2;
	int x,y;
	double treebblo[2], treebbhi[2];
	static int nv = 0;
	char fn[256];
	FILE* fout;
	int i;

	sprintf(fn, "ver%04i.dat", nv);
	nv++;
	fprintf(stderr, "Writing %s...\n", fn);
	fout = fopen(fn, "wb");
	if (!fout) {
		fprintf(stderr, "failed to open verify data file.\n");
		exit(-1);
	}
	kdtree_get_bboxes(itree, 0, treebblo, treebbhi);
	x1 = y1 = 0;
	x2 = (int)treebbhi[0] + 10;
	y2 = (int)treebbhi[1] + 10;
	writeFloat(fout, x1);
	writeFloat(fout, x2);
	writeFloat(fout, y1);
	writeFloat(fout, y2);
	writeFloat(fout, NF);
	writeFloat(fout, NI);
	writeFloat(fout, fieldarea);
	writeFloat(fout, logprob_background);
	writeFloat(fout, logprob_distractor);
	for (y=y1; y<=y2; y++) {
		for (x=x1; x<=x2; x++) {
			double bestd2;
			double sigma2;
			double cutoffd2;
			double R2;
			double logprob;
			int ind;
			double pt[2];
			pt[0] = x;
			pt[1] = y;
			// Distance from the quad center of this field star:
			R2 = distsq(pt, qc, 2);
			// Variance of a field star at that distance from the quad center:
			sigma2 = verify_pix2 * ((1.5*1.5) + R2/rquad2);
			cutoffd2 = HUGE_VAL;
			ind = kdtree_nearest_neighbour_within(itree, pt, cutoffd2, &bestd2);
			// p(foreground):
			logprob = -log(2.0 * M_PI * sigma2 * NI) -(bestd2 / (2.0 * sigma2));
			writeFloat(fout, logprob);
		}
	}
	for (i=0; i<NF; i++) {
		writeFloat(fout, field[i*2]);
		writeFloat(fout, field[i*2+1]);
	}
	fclose(fout);
}

