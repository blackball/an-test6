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

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <sys/param.h>

#include "verify.h"
#include "mathutil.h"
#include "intmap.h"
#include "keywords.h"

#include "blind_wcs.h"

void verify_init() {}
void verify_cleanup() {}

#define DEBUGVERIFY 0
#if DEBUGVERIFY
#define debug(args...) fprintf(stderr, args)
#else
#define debug(args...)
#endif

verify_field_t* verify_field_preprocess(double* field, int NF) {
    verify_field_t* vf;
    int Nleaf = 5;

    vf = malloc(sizeof(verify_field_t));
    if (!vf) {
        fprintf(stderr, "Failed to allocate space for a verify_field_t().\n");
        return NULL;
    }

    vf->NF = NF;
    vf->field = field;

    // Note: kdtree type: I tried U32 (duu) but it was marginally slower.
    // I didn't try U16 (dss) because we need a fair bit of accuracy here.

    // Make a copy of the field objects, because we're going to build a
    // kdtree out of them and that shuffles their order.
    vf->fieldcopy = malloc(NF * 2 * sizeof(double));
    if (!vf->fieldcopy) {
        fprintf(stderr, "Failed to copy the field.\n");
        free(vf);
        return NULL;
    }
    memcpy(vf->fieldcopy, field, NF * 2 * sizeof(double));

    // Build a tree out of the field objects (in pixel space)
    vf->ftree = kdtree_build(NULL, vf->fieldcopy, NF, 2, Nleaf, KDTT_DOUBLE, KD_BUILD_SPLIT);

    return vf;
}

void verify_field_free(verify_field_t* vf) {
    if (!vf)
        return;
    kdtree_free(vf->ftree);
    free(vf->fieldcopy);
    free(vf);
    return;
}

void verify_hit(startree* skdt,
                MatchObj* mo,
                sip_t* sip,
                verify_field_t* vf,
                double verify_pix2,
                double distractors,
                double fieldW,
                double fieldH,
                double logratio_tobail,
                bool do_gamma,
				int dimquads) {
	int i;
	double* fieldcenter;
	double fieldr2;
	kdtree_qres_t* res;
	// number of stars in the index that are within the bounds of the field.
	int NI;
	double* indexpix;

	double* bestprob = NULL;

	// quad center and radius
	double qc[2];
	double rquad2 = 0.0;

	double logprob_distractor;
	double logprob_background;

	double logodds = 0.0;
	int nmatch, nnomatch, nconflict;

	double bestlogodds;
	int bestnmatch, bestnnomatch, bestnconflict;

	int Nmin;

	double crvalxyz[3];
	kdtree_t* startree = skdt->tree;
	uint8_t* sweeps = NULL;
	int s, maxsweep;

	double fieldr, fieldarcsec;

	assert(mo->wcs_valid || sip);
	assert(startree);
	assert(skdt->sweep);

	// center and radius of the field in xyz space:
    fieldcenter = mo->center;
    fieldr2 = square(mo->radius);

	if (DEBUGVERIFY) {
		debug("\nVerifying a match.\n");
		debug("Quad field stars: [");
		for (i=0; i<dimquads; i++)
			debug("%s%i", (i?", ":""), mo->field[i]);
		debug("]\n");
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
    if (sip)
        radecdegarr2xyzarr(sip->wcstan.crval, crvalxyz);
    else
        radecdegarr2xyzarr(mo->wcstan.crval, crvalxyz);
        
	for (i=0; i<res->nres; i++) {
		double x, y;
        if (sip) {
            if (!sip_xyzarr2pixelxy(sip, res->results.d + i*3, &x, &y))
                continue;
        } else {
            if (!tan_xyzarr2pixelxy(&(mo->wcstan), res->results.d + i*3, &x, &y))
                continue;
        }
		//debug("(x,y) = (%g,%g)", x, y);
		if ((x < 0) || (y < 0) || (x >= fieldW) || (y >= fieldH)) {
			//debug(" -> reject\n");
			continue;
		}
		//debug(" -> good\n");

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

	/*
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
	*/
	debug("Number of field stars: %i\n", vf->NF);
	debug("Number of index stars: %i\n", NI);

	if (!NI) {
		// I don't know HOW this happens - at the very least, the four stars
		// belonging to the quad that generated this hit should lie in the
		// proposed field - but I've seen it happen!
		//fprintf(stderr, "Freakishly, NI=0.\n");
		mo->nfield = 0;
		mo->noverlap = 0;
		matchobj_compute_overlap(mo);
		mo->logodds = -HUGE_VAL;
		kdtree_free_query(res);
		free(indexpix);
		return;
	}

	sweeps = malloc(NI * sizeof(uint8_t));
	maxsweep = 0;
	for (i=0; i<NI; i++) {
		sweeps[i] = skdt->sweep[res->inds[i]];
		maxsweep = MAX(maxsweep, sweeps[i]);
	}

        // Prime the array where we store conflicting-match info.
	bestprob = malloc(vf->NF * sizeof(double));
	for (i=0; i<vf->NF; i++)
		bestprob[i] = -HUGE_VAL;
	for (i=0; i<dimquads; i++) {
		assert(mo->field[i] >= 0);
		assert(mo->field[i] < vf->NF);
		bestprob[mo->field[i]] = HUGE_VAL;
	}

	if (do_gamma) {
            // Find the midpoint of AB of the quad in pixel space.
            qc[0] = 0.5 * (vf->field[2*mo->field[0]  ] + vf->field[2*mo->field[1]  ]);
            qc[1] = 0.5 * (vf->field[2*mo->field[0]+1] + vf->field[2*mo->field[1]+1]);
            // Find the radius-squared of the quad = distsq(qc, A)
            rquad2 = distsq(vf->field + 2*mo->field[0], qc, 2);
			debug("Quad radius = %g pixels\n", sqrt(rquad2));
	}

	// p(background) = 1/(W*H) of the image.
	logprob_background = -log(fieldW * fieldH);

	// p(distractor) = D / (W*H)
	logprob_distractor = log(distractors / (fieldW * fieldH));

	debug("log(p(background)) = %g\n", logprob_background);
	debug("log(p(distractor)) = %g\n", logprob_distractor);

	Nmin = MIN(NI, vf->NF);

	bestlogodds = -HUGE_VAL;
	bestnmatch = bestnnomatch = bestnconflict = -1;
	nmatch = nnomatch = nconflict = 0;

	// Add index stars, in sweeps.
	for (s=0; s<=maxsweep; s++) {
		for (i=0; i<NI; i++) {
			double bestd2;
			double sigma2;
			//double cutoffd2;
			double R2 = 0.0;
			double logprob = -HUGE_VAL;
			int ind;
			int fldind;
			int j;
			bool cont;

			if (sweeps[i] != s)
				continue;

			// Skip stars that are part of the quad:
			ind = res->inds[i];
			cont = FALSE;
			for (j=0; j<dimquads; j++)
				if (ind == mo->star[j]) {
					cont = TRUE;
					break;
				}
			if (cont)
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
			ind = kdtree_nearest_neighbour(vf->ftree, indexpix+i*2, &bestd2);

			fldind = vf->ftree->perm[ind];
			assert(fldind >= 0);
			assert(fldind < vf->NF);

			if (do_gamma) {
                            // Distance from the quad center of this field star:
                            R2 = distsq(vf->field+fldind*2, qc, 2);

                            // Variance of a field star at that distance from the 
                            // quad center:
                            // FIXME!
                            sigma2 = verify_pix2 * (1.0 + R2/rquad2);
			} else
                            sigma2 = verify_pix2;

			// don't bother computing the logprob if it's tiny...
			//if (bestd2 > 100.0 * sigma2)

			if (DEBUGVERIFY) {
				double ra,dec;
				if (do_gamma)
					debug("\nIndex star %i (sweep %i): rad %g pixels (%g quads) => sigma = %g.\n",
						  i, s, sqrt(R2), sqrt(R2/rquad2), sqrt(sigma2));
				debug("Index star is at (%g,%g) pixels.\n", indexpix[i*2], indexpix[i*2+1]);
				xyzarr2radecdeg(res->results.d + i*3, &ra, &dec);
				debug("Index star RA,Dec (%.8g,%.8g) deg\n", ra, dec);
				// debug("Peak of this Gaussian has value %g (log %g)\n", (1.0 - distractors) / (2.0 * M_PI * sigma2 * M),
				// log((1.0 - distractors) / (2.0 * M_PI * sigma2 * M)));
				debug("NN dist: %5.1f pix, %g sigmas\n", sqrt(bestd2), sqrt(bestd2/sigma2));
			}
			if (log((1.0 - distractors) / (2.0 * M_PI * sigma2 * vf->NF)) < logprob_background) {
				// what's the point?!
				debug("This Gaussian is nearly uninformative.\n");
				continue;
			}

			logprob = log((1.0 - distractors) / (2.0 * M_PI * sigma2 * vf->NF)) - (bestd2 / (2.0 * sigma2));
			if (logprob < logprob_distractor) {
				debug("Distractor.\n");
				logprob = logprob_distractor;
				nnomatch++;
			} else {
				double oldprob;
				debug("Match (field star %i), logprob %g (background=%g, distractor=%g)\n", fldind, logprob, logprob_background, logprob_distractor);
				logprob = log(exp(logprob) + distractors/(fieldW*fieldH));

				oldprob = bestprob[fldind];
				if (oldprob != -HUGE_VAL) {
					nconflict++;
					// There was a previous match to this field object.
					if (oldprob == HUGE_VAL) {
						debug("Conflicting match to one of the stars in the quad.\n");
					} else {
						debug("Conflict: odds was %g, now %g.\n", oldprob, logprob);
					}
					// Allow an improved match (except to the stars composing the quad, because they are initialized to HUGE_VAL)
					if (logprob > oldprob) {
						oldprob = logprob;
						logodds += (logprob - oldprob);
					}
					continue;
				} else {
					bestprob[fldind] = logprob;
					nmatch++;
				}
			}

			logodds += (logprob - logprob_background);
			debug("Logodds now %g\n", logodds);
		
			if (logodds < logratio_tobail) {
				break;
			}
		}

		// After each sweep, check if it's the new best logprob.
		if (logodds > bestlogodds) {
			bestlogodds = logodds;
			bestnmatch = nmatch;
			bestnnomatch = nnomatch;
			bestnconflict = nconflict;
		}
	}

	kdtree_free_query(res);
	free(bestprob);
	free(sweeps);
	free(indexpix);

	mo->logodds = bestlogodds;
	mo->noverlap = bestnmatch;
	mo->nconflict = bestnconflict;
	mo->nfield = bestnmatch + bestnnomatch + bestnconflict;
	mo->nindex = NI;
	matchobj_compute_overlap(mo);
}

