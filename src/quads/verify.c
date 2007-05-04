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
#include "keywords.h"

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#include "solver_callbacks.h"
#include "blind_wcs.h"

// FOR TESTING
/*
static void write_prob_terrain(kdtree_t* itree, int NF, int NI,
							   double fieldarea, double logprob_background,
							   double logprob_distractor, double* qc,
							   double verify_pix2, double rquad2,
							   double* field);
*/

#define DEBUGVERIFY 0
#if DEBUGVERIFY
#define debug(args...) fprintf(stderr, args)
#else
#define debug(args...)
#endif

void verify_hit(startree* skdt,
				MatchObj* mo,
				double* field,
				int NF,
				double verify_pix2,
				double distractors,
				double fieldW,
				double fieldH,
				double logratio_tobail,
				int min_nfield) {
	int i;
	double fieldcenter[3];
	double fieldr2;
	kdtree_qres_t* res;
	// number of stars in the index that are within the bounds of the field.
	int NI;
	kdtree_t* ftree;
	int Nleaf = 5;
	double* indexpix;

	// quad center and radius
	double qc[2];
	double rquad2;

	double logprob_distractor;
	double logprob_background;

	double logodds = 0.0;
	int nmatch, nnomatch;

	double bestlogodds;
	int bestnmatch, bestnnomatch;

	// FIXME - the value 1.5 is simulation-based.
	//double gamma2 = square(1.5);

	int Nmin;

	double crvalxyz[3];
	kdtree_t* startree = skdt->tree;
	unsigned char* sweeps = NULL;
	int s, maxsweep;

	double* fieldcopy;
	int K;

	assert(mo->wcs_valid);
	assert(startree);
	assert(skdt->sweep);

	// find the center and radius of the field in xyz space.
	star_midpoint(fieldcenter, mo->sMin, mo->sMax);
	fieldr2 = distsq(fieldcenter, mo->sMin, 3);

	double fieldr, fieldarcsec;
	if (DEBUGVERIFY) {
		fieldr = sqrt(fieldr2);
		fieldarcsec = distsq2arcsec(fieldr2);
		debug("%g, %g\n", fieldr, fieldarcsec);
	}

	// find all the index stars that are inside the circle that bounds
	// the field.
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
		debug("(x,y) = (%g,%g)", x, y);
		if ((x < 0) || (y < 0) || (x >= fieldW) || (y >= fieldH)) {
			debug(" -> reject\n");
			continue;
		}
		debug(" -> good\n");

		// Here we compact the "res" array(s) so that when we're done,
		// the NI indices that are inside the field are in the bottom
		// NI elements of the res->inds array.
		res->inds[NI] = res->inds[i];
		if (DEBUGVERIFY)
			memmove(res->results.d + NI*3,
					res->results.d + i*3,
					3*sizeof(double));

		indexpix[NI*2  ] = x;
		indexpix[NI*2+1] = y;
		NI++;
	}
	indexpix = realloc(indexpix, NI * 2 * sizeof(double));

	if (DEBUGVERIFY) {
		double minx,maxx,miny,maxy;
		miny = minx = HUGE_VAL;
		maxx = maxy = -HUGE_VAL;
		for (i=0; i<NI; i++) {
			minx = min(indexpix[i*2  ], minx);
			maxx = max(indexpix[i*2  ], maxx);
			miny = min(indexpix[i*2+1], miny);
			maxy = max(indexpix[i*2+1], maxy);
		}
		debug("Range of index objs: x:[%g,%g], y:[%g,%g]\n",
			  minx, maxx, miny, maxy);
	}
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

	sweeps = malloc(NI * sizeof(unsigned char));
	maxsweep = 0;
	for (i=0; i<NI; i++) {
		sweeps[i] = skdt->sweep[res->inds[i]];
		maxsweep = max(maxsweep, sweeps[i]);
	}

	// K: number of field objects to use.
	//K = min(NF, 2*NI);
	K = NF;

	// Make a copy of the field objects, because we're going to build a
	// kdtree out of them and that shuffles their order.
	fieldcopy = malloc(K * 2 * sizeof(double));
	memcpy(fieldcopy, field, K * 2 * sizeof(double));

	// Build a tree out of the field objects (in pixel space)
	ftree = kdtree_build(NULL, fieldcopy, K, 2, Nleaf, KDTT_DOUBLE, KD_BUILD_BBOX);

	// Find the midpoint of AB of the quad in pixel space.
	qc[0] = 0.5 * (field[2*mo->field[0]  ] + field[2*mo->field[1]  ]);
	qc[1] = 0.5 * (field[2*mo->field[0]+1] + field[2*mo->field[1]+1]);
	// Find the radius-squared of the quad = distsq(qc, A)
	rquad2 = distsq(field + 2*mo->field[0], qc, 2);

	// p(background) = 1/(W*H) of the image.
	logprob_background = -log(fieldW * fieldH);

	// p(distractor) = D / (W*H)
	logprob_distractor = log(distractors / (fieldW * fieldH));

	debug("log(p(background)) = %g\n", logprob_background);
	debug("log(p(distractor)) = %g\n", logprob_distractor);

	/*
	  if (0) {
	  write_prob_terrain(itree, NF, NI, fieldW*fieldH, logprob_background,
	  logprob_distractor, qc, verify_pix2, rquad2,
	  field);
	  }
	*/

	nmatch = 0;
	nnomatch = 0;

	Nmin = min(NI, NF);

	bestlogodds = -HUGE_VAL;
	bestnmatch = bestnnomatch = -1;

	// Add index stars, in sweeps.
	for (s=0; s<=maxsweep; s++) {
		for (i=0; i<NI; i++) {
			double bestd2;
			double sigma2;
			//double cutoffd2;
			double R2;
			double logprob = -HUGE_VAL;
			int ind;

			if (sweeps[i] != s)
				continue;

			// Skip stars that are part of the quad:
			ind = res->inds[i];
			if (ind == mo->star[0] || ind == mo->star[1] ||
				ind == mo->star[2] || ind == mo->star[3])
				continue;

			// Distance from the quad center of this index star:
			// (we just use this to estimate sigma2 to estimate the cutoff
			//  distance)
			//R2 = distsq(field+i*2, qc, 2);
			// Variance of a field star at that distance from the quad center:
			//sigma2 = verify_pix2 * (gamma2 + R2/rquad2);
			// Cutoff distance to nearest neighbour star: p(fg) == p(bg)
			// OK, I think that should be cutoffd2= -2*sigma2*(logbg - log(2*pi) - .5*log(sigma2))
			// FIXME - just set it to 10 sigmas.
			//cutoffd2 = 100.0 * sigma2;
			//ind = kdtree_nearest_neighbour_within(itree, field+i*2, cutoffd2, &bestd2);
			//if (ind != -1) {
			// p(foreground):
			//logprob = log((1.0 - distractors) / (2.0 * M_PI * sigma2 * NI)) - (bestd2 / (2.0 * sigma2));
			//}


			// Find nearest field star.
			ind = kdtree_nearest_neighbour(ftree, indexpix+i*2, &bestd2);

			// Distance from the quad center of this field star:
			R2 = distsq(field+ftree->perm[i]*2, qc, 2);

			// Variance of a field star at that distance from the 
			// quad center:
			// FIXME!
			sigma2 = verify_pix2 * (1.0 + R2/rquad2);

			// don't bother computing the logprob if it's tiny...
			//if (bestd2 > 100.0 * sigma2)

			debug("\nIndex star %i (sweep %i): rad %g quads, sigma %g.\n", i, s, sqrt(R2/rquad2), sqrt(sigma2));
			debug("NN dist: %5.1f pix, %g sigmas\n", sqrt(bestd2), sqrt(bestd2/sigma2));
				
			logprob = log((1.0 - distractors) / (2.0 * M_PI * sigma2 * K)) - (bestd2 / (2.0 * sigma2));
			if (logprob < logprob_distractor) {
				debug("Distractor.\n");
				logprob = logprob_distractor;
				nnomatch++;
			} else {
				debug("Match (field star %i), logprob %g\n", ftree->perm[ind], logprob);
				nmatch++;
			}

			logodds += (logprob - logprob_background);
			debug("Logodds now %g\n", logodds);
		
			if (logodds < logratio_tobail) {
				break;
			}
		}

		// After each sweep, check if it's the new best logprob.
		//if ((logodds > bestlogodds) &&
		//(i >= min_nfield)) {
		if (logodds > bestlogodds) {
			bestlogodds = logodds;
			bestnmatch = nmatch;
			bestnnomatch = nnomatch;
		}
	}

	kdtree_free_query(res);
	kdtree_free(ftree);
	free(fieldcopy);
	free(sweeps);
	free(indexpix);

	mo->logodds = bestlogodds;
	mo->noverlap = bestnmatch;
	mo->nconflict = 0;
	mo->nfield = bestnmatch + bestnnomatch;
	mo->nindex = NI;
	matchobj_compute_overlap(mo);
}





// TEST
/*
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
*/
