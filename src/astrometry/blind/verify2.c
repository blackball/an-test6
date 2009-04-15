/*
 This file is part of the Astrometry.net suite.
 Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.
 Copyright 2009 Dustin Lang, David W. Hogg.

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

#include "verify2.h"
#include "permutedsort.h"
#include "mathutil.h"
#include "keywords.h"
#include "log.h"
#include "sip-utils.h"
#include "healpix.h"

#define DEBUGVERIFY 1
#if DEBUGVERIFY
//#define debug(args...) fprintf(stderr, args)
#else
#define debug(args...)
#endif

/*
 This gets called once for each field before verification begins.
 We build a kdtree out of the field stars (in pixel space) which will be
 used during verification to find nearest-neighbours.
 */
verify_field_t* verify_field_preprocess(const starxy_t* fieldxy) {
    verify_field_t* vf;
    int Nleaf = 5;

    vf = malloc(sizeof(verify_field_t));
    if (!vf) {
        fprintf(stderr, "Failed to allocate space for a verify_field_t().\n");
        return NULL;
    }

    vf->field = fieldxy;

    // Note: kdtree type: I tried U32 (duu) but it was marginally slower.
    // I didn't try U16 (dss) because we need a fair bit of accuracy here.

    // Make a copy of the field objects, because we're going to build a
    // kdtree out of them and that shuffles their order.
    vf->fieldcopy = starxy_copy_xy(fieldxy);
    if (!vf->fieldcopy) {
        fprintf(stderr, "Failed to copy the field.\n");
        free(vf);
        return NULL;
    }

    // Build a tree out of the field objects (in pixel space)
    vf->ftree = kdtree_build(NULL, vf->fieldcopy, starxy_n(vf->field),
                             2, Nleaf, KDTT_DOUBLE, KD_BUILD_SPLIT);

    return vf;
}

/*
 This gets called after all verification calls for a field are finished;
 we cleanup the data structures we created in the verify_field_preprocess()
 function.
 */
void verify_field_free(verify_field_t* vf) {
    if (!vf)
        return;
    kdtree_free(vf->ftree);
    free(vf->fieldcopy);
    free(vf);
    return;
}

void verify_get_index_stars(const double* fieldcenter, double fieldr2,
							const startree_t* skdt, const sip_t* sip, const tan_t* tan,
							double fieldW, double fieldH,
							double** p_indexradec,
							double** indexpix, int** p_starids, int* p_nindex) {
	double* indxyz;
    int i, N, NI;
    int* sweep;
    int* starid;
	int* inbounds;
	int* perm;

	assert(skdt->sweep);

	// Find all index stars within the bounding circle of the field.
	startree_search_for(skdt, fieldcenter, fieldr2, &indxyz, NULL, &starid, &N);

	// Find index stars within the rectangular field.
	inbounds = sip_filter_stars_in_field(sip, tan, indxyz, NULL, N, indexpix,
										 NULL, &NI);
	// Apply the permutation now, so that "indexpix" and "starid" stay in sync:
	// indexpix is already in the "inbounds" ordering.
	permutation_apply(inbounds, NI, starid, starid, sizeof(int));

	// Compute index RA,Decs if requested.
	if (p_indexradec) {
		double* radec = malloc(2 * NI * sizeof(double));
		for (i=0; i<NI; i++)
			// note that the "inbounds" permutation is applied to "indxyz" here.
			xyzarr2radecdegarr(indxyz + 3*inbounds[i], radec + 2*i);
		*p_indexradec = radec;
	}
	free(indxyz);
	free(inbounds);

    // Each index star has a "sweep number" assigned during index building;
    // it roughly represents a local brightness ordering.  Use this to sort the
	// index stars.
	sweep = malloc(NI * sizeof(int));
	for (i=0; i<NI; i++)
		sweep[i] = skdt->sweep[starid[i]];
    perm = permuted_sort(sweep, sizeof(int), compare_ints_asc, NULL, NI);
	free(sweep);

	if (indexpix) {
		permutation_apply(perm, NI, *indexpix, *indexpix, 2 * sizeof(double));
		*indexpix = realloc(*indexpix, NI * 2 * sizeof(double));
	}

	if (p_starids) {
		permutation_apply(perm, NI, starid, starid, sizeof(int));
		starid = realloc(starid, NI * sizeof(int));
		*p_starids = starid;
	} else
		free(starid);

	free(perm);

    *p_nindex = NI;
}

//static void trim_index_stars()

/**
 If field objects are within "sigma" of each other (where sigma depends on the
 distance from the matched quad), then they are not very useful for verification.
 We filter out field stars within sigma of each other, taking only the brightest.

 Returns an array indicating which field stars should be kept.
 */
static bool* deduplicate_field_stars(verify_field_t* vf, double* sigma2s) {
    bool* keepers = NULL;
    int i, j, N;
    kdtree_qres_t* res;

    N = starxy_n(vf->field);
    keepers = malloc(N * sizeof(bool));
    for (i=0; i<N; i++) {
        double sxy[2];
        // free parameter!
        double nsigma2 = 1.0;
        keepers[i] = TRUE;
        starxy_get(vf->field, i, sxy);
        res = kdtree_rangesearch_nosort(vf->ftree, sxy, nsigma2 * sigma2s[i]);
        for (j=0; j<res->nres; j++) {
            if (res->inds[j] < i) {
                keepers[i] = FALSE;
                // DEBUG
                /*
                 double otherxy[2];
                 starxy_get(vf->field, res->inds[j], otherxy);
                 logverb("Field star %i at %g,%g: is close to field star %i at %g,%g.  dist is %g, sigma is %g\n",
                 i, sxy[0], sxy[1], res->inds[j], otherxy[0], otherxy[1],
                 sqrt(distsq(sxy, otherxy, 2)), sqrt(nsigma2 * sigma2s[i]));
                 */
                break;
            }
        }
        kdtree_free_query(res);
    }

    return keepers;
}

static void get_quad_center(verify_field_t* vf, MatchObj* mo, double* centerpix,
							double* quadr2) {
	double Axy[2], Bxy[2];
	// Find the midpoint of AB of the quad in pixel space.
	starxy_get(vf->field, mo->field[0], Axy);
	starxy_get(vf->field, mo->field[1], Bxy);
	centerpix[0] = 0.5 * (Axy[0] + Bxy[0]);
	centerpix[1] = 0.5 * (Axy[1] + Bxy[1]);
	// Find the radius-squared of the quad = distsq(qc, A)
	*quadr2 = distsq(Axy, centerpix, 2);
}

static double get_sigma2_at_radius(double verify_pix2, double r2, double quadr2) {
	return verify_pix2 * (1.0 + r2/quadr2);
}

double* verify_compute_sigma2s(verify_field_t* vf, MatchObj* mo,
							   double verify_pix2, bool do_gamma) {
	double* sigma2s;
    int NF, i;
	double qc[2];
	double rquad2;
	double R2;

	NF = starxy_n(vf->field);
    sigma2s = malloc(NF * sizeof(double));

	if (!do_gamma) {
		for (i=0; i<NF; i++)
            sigma2s[i] = verify_pix2;
	} else {
		// If we're modelling the expected noise as a Gaussian whose variance grows
		// away from the quad center, compute the required quantities...
		get_quad_center(vf, mo, qc, &rquad2);
        debug("Quad radius = %g pixels\n", sqrt(rquad2));

		// Compute individual positional variances for every field star.
		for (i=0; i<NF; i++) {
            double sxy[2];
            starxy_get(vf->field, i, sxy);
            // Distance from the quad center of this field star:
            R2 = distsq(sxy, qc, 2);
            // Variance of a field star at that distance from the quad center:
            sigma2s[i] = get_sigma2_at_radius(verify_pix2, R2, rquad2);
        }
	}
	return sigma2s;
}

#include "fitsioutils.h"
#include "errors.h"
#include "cairoutils.h"	

static int get_bin(verify_field_t* vf, int starnum,
				   double fieldW, double fieldH,
				   int nw, int nh) {
	double fxy[2];
	int ix, iy;
	starxy_get(vf->field, starnum, fxy);
	ix = (int)floor(nw * fxy[0] / fieldW);
	ix = MAX(0, MIN(nw-1, ix));
	iy = (int)floor(nh * fxy[1] / fieldH);
	iy = MAX(0, MIN(nh-1, iy));
	return iy * nw + ix;
}


void verify_get_uniformize_scale(int cutnside, double scale, int W, int H, int* cutnw, int* cutnh) {
	double cutarcsec, cutpix;
	cutarcsec = healpix_side_length_arcmin(cutnside) * 60.0;
	cutpix = cutarcsec / scale;
	logverb("cut nside: %i\n", cutnside);
	logverb("cut scale: %g arcsec\n", cutarcsec);
	logverb("match scale: %g arcsec/pix\n", scale);
	logverb("cut scale: %g pixels\n", cutpix);
	if (cutnw)
		*cutnw = MAX(1, (int)round(W / cutpix));
	if (cutnh)
		*cutnh = MAX(1, (int)round(H / cutpix));
}


void verify_uniformize_field(verify_field_t* vf,
							 double fieldW, double fieldH,
							 int nw, int nh,
							 int** p_perm,
							 int** p_bincounts,
							 double** p_bincenters,
							 int** p_binids) {
	il** lists;
	int i,j,k,p;
	int* perm;
	int* bincounts = NULL;
    int NF;
	int* binids = NULL;

	NF = starxy_n(vf->field);
	perm = malloc(NF * sizeof(int));
	if (p_binids) {
		binids = malloc(NF * sizeof(int));
		*p_binids = binids;
	}

	lists = malloc(nw * nh * sizeof(il*));
	for (i=0; i<(nw*nh); i++)
		lists[i] = il_new(16);

	if (p_bincenters) {
		double* bxy = malloc(nw * nh * 2 * sizeof(double));
		for (j=0; j<nh; j++)
			for (i=0; i<nw; i++) {
				bxy[(j * nw + i)*2 +0] = (i + 0.5) * fieldW / (double)nw;
				bxy[(j * nw + i)*2 +1] = (j + 0.5) * fieldH / (double)nh;
			}
		*p_bincenters = bxy;
	}

	// put the stars in the appropriate bins.
	for (i=0; i<NF; i++) {
		int bin = get_bin(vf, i, fieldW, fieldH, nw, nh);
		il_append(lists[bin], i);
	}

	if (p_bincounts) {
		// note the bin occupancies.
		bincounts = malloc(nw * nh * sizeof(int));
		for (i=0; i<(nw*nh); i++) {
			bincounts[i] = il_size(lists[i]);
			logverb("bin %i has %i stars\n", i, bincounts[i]);
		}
		*p_bincounts = bincounts;
	}

	// make sweeps through the bins, grabbing one star from each.
	p=0;
	for (k=0;; k++) {
		for (j=0; j<nh; j++) {
			for (i=0; i<nw; i++) {
				il* lst = lists[j*nw + i];
				if (k >= il_size(lst))
					continue;
				perm[p] = il_get(lst, k);
				if (binids)
					binids[p] = j*nw + i;
				p++;
			}
		}
		if (p == NF)
			break;
	}

	for (i=0; i<(nw*nh); i++)
		il_free(lists[i]);
	free(lists);

	*p_perm = perm;
}

static void add_gaussian_to_image(double* img, int W, int H,
								  double cx, double cy, double sigma,
								  double scale,
								  double nsigma, int boundary) {
	int x, y;
	if (boundary == 0) {
		// truncate.
		for (y = MAX(0, cy - nsigma*sigma); y <= MIN(H-1, cy + nsigma * sigma); y++) {
			for (x = MAX(0, cx - nsigma*sigma); x <= MIN(W-1, cx + nsigma * sigma); x++) {
				img[y*W + x] += scale * exp(-(square(y-cy)+square(x-cx)) / (2.0 * square(sigma)));
			}
		}

	} else if (boundary == 1) {
		// mirror.
		int mx, my;
		for (y=MAX(-(H-1), floor(cy - nsigma * sigma));
			 y<=MIN(2*H-1, ceil(cy + nsigma * sigma)); y++) {
			if (y < 0)
				my = -1 - y;
			else if (y >= H)
				my = 2*H - 1 - y;
			else
				my = y;
			for (x=MAX(-(W-1), floor(cx - nsigma * sigma));
				 x<=MIN(2*W-1, ceil(cx + nsigma * sigma)); x++) {
				if (x < 0)
					mx = -1 - x;
				else if (x >= W)
					mx = 2*W - 1 - x;
				else
					mx = x;
				img[my*W + mx] += scale * exp(-(square(y-cy)+square(x-cx)) / (2.0 * square(sigma)));
			}
		}

	}
	
}

double verify_star_lists(const double* refxys, int NR,
						 const double* testxys, const double* testsigma2s, int NT,
						 double effective_area,
						 double distractors,
						 double logodds_bail, //double logodds_accept,
						 int** p_matches, int* p_besti) {
	int i;
	double bestlogodds;
	int besti;
	double logodds;
	double logbg;
	double logd;
	//double matchnsigma = 5.0;
	double* refcopy;
	kdtree_t* rtree;
	int Nleaf = 10;
	int* rmatches;
	double* rprobs;

	// Build a tree out of the index stars in pixel space...
	// kdtree scrambles the data array so make a copy first.
	refcopy = malloc(2 * NR * sizeof(double));
	memcpy(refcopy, refxys, 2 * NR * sizeof(double));
	rtree = kdtree_build(NULL, refcopy, NR, 2, Nleaf, KDTT_DOUBLE, KD_BUILD_SPLIT);

	rmatches = malloc(NR * sizeof(int));
	for (i=0; i<NR; i++)
		rmatches[i] = -1;

	rprobs = malloc(NR * sizeof(double));
	for (i=0; i<NR; i++)
		rprobs[i] = -HUGE_VAL;

	logbg = log(1.0 / effective_area);
	logd  = log(distractors / effective_area);

	bestlogodds = -HUGE_VAL;
	besti = -1;

	logodds = 0.0;
	for (i=0; i<NT; i++) {
		const double* testxy;
		double sig2;
		int refi;
		int tmpi;
		double d2;
		double logfg;

		testxy = testxys + 2*i;
		sig2 = testsigma2s[i];

		logverb("\n");
		logverb("test star %i: (%.1f,%.1f), sigma: %.1f\n", i, testxy[0], testxy[1], sqrt(sig2));

		// find nearest ref star (within 5 sigma)
        tmpi = kdtree_nearest_neighbour_within(rtree, testxy, sig2 * 25.0, &d2);
		if (tmpi == -1) {
			// no nearest neighbour within range.
			logverb("  No nearest neighbour.\n");
			refi = -1;
			logfg = -HUGE_VAL;
		} else {
			double loggmax;

			refi = kdtree_permute(rtree, tmpi);

			// peak value of the Gaussian
			loggmax = log((1.0 - distractors) / (2.0 * M_PI * sig2 * NR));

			// FIXME - uninformative?
			if (loggmax < logbg)
				logverb("  This star is uninformative: peak %.1f, bg %.1f.\n", loggmax, logbg);

			// value of the Gaussian
			logfg = loggmax - d2 / (2.0 * sig2);
			
			logverb("  NN: ref star %i, dist %.2f, sigmas: %.3f, logfg: %.1f (%.1f above distractor, %.1f above bg)\n",
				  refi, sqrt(d2), sqrt(d2 / sig2), logfg, logfg - logd, logfg - logbg);
		}

		if (logfg < logd) {
			logfg = logd;
			logverb("  Distractor.\n");
		} else {
			// duplicate match?
			if (rmatches[refi] != -1) {
				double oldfg = rprobs[refi];

				//logverb("Conflict: odds was %g, now %g.\n", oldfg, logfg);

				if (logfg > oldfg) {
					// upgrade: old match becomes a distractor.
					logverb("  Conflict: upgrading.  logprob was %.1f, now %.1f.\n", oldfg, logfg);
                    logodds += (logd - oldfg);
					logverb("  Switching old match to distractor: logodds change %.1f, now %.1f\n",
							(logd - oldfg), logodds);
					// record this new match.
					rmatches[refi] = i;
					rprobs[refi] = logfg;
				} else {
					// old match was better: this match becomes a distractor.
					logverb("  Conflict: not upgrading.  logprob was %.1f, now %.1f.\n", oldfg, logfg);

					logfg = logd;
				}

			} else {
				// new match.
				rmatches[refi] = i;
				rprobs[refi] = logfg;
			}
		}

        logodds += (logfg - logbg);
        logverb("  Logodds: change %.1f, now %.1f\n", (logfg - logbg), logodds);

        if (logodds < logodds_bail)
            break;

        if (logodds > bestlogodds) {
			bestlogodds = logodds;
			besti = i;
		}
	}

	if (p_matches)
		*p_matches = rmatches;
	else
		free(rmatches);

	if (p_besti)
		*p_besti = besti;

	free(rprobs);

	kdtree_free(rtree);
	free(refcopy);

	return bestlogodds;
}

void verify_hit(index_t* index,
				MatchObj* mo, sip_t* sip, verify_field_t* vf,
                double verify_pix2, double distractors,
                double fieldW, double fieldH,
                double logratio_tobail,
                bool do_gamma, int dimquads, bool fake_match,
				double logodds_tokeep) {
	int i, j, k;
	double* fieldcenter;
	double fieldr2;
	// number of stars in the index that are within the bounds of the field.
	int NI;
	double* indexpix;
    int* starids;
    int NF;
    double* sigma2s;
	startree_t* skdt;

	skdt = index->starkd;

	assert(mo->wcs_valid || sip);

	// center and radius of the field in xyz space:
    fieldcenter = mo->center;
    fieldr2 = square(mo->radius);
    debug("Field center %g,%g,%g, radius2 %g\n", fieldcenter[0], fieldcenter[1], fieldcenter[2], fieldr2);

    // find index stars and project them into pixel coordinates.
    verify_get_index_stars(fieldcenter, fieldr2, skdt, sip, &(mo->wcstan),
						   fieldW, fieldH, NULL, &indexpix, &starids, &NI);
	if (!NI) {
		// I don't know HOW this happens - at the very least, the four stars
		// belonging to the quad that generated this hit should lie in the
		// proposed field - but I've seen it happen!
		mo->nfield = 0;
		mo->noverlap = 0;
		matchobj_compute_derived(mo);
		mo->logodds = -HUGE_VAL;
		return;
    }

    NF = starxy_n(vf->field);
	debug("Number of field stars: %i\n", NF);
	debug("Number of index stars: %i\n", NI);

	// If we're verifying an existing WCS solution, then don't increase the variance
	// away from the center of the matched quad.
    if (fake_match)
        do_gamma = FALSE;

	sigma2s = verify_compute_sigma2s(vf, mo, verify_pix2, do_gamma);

	// Uniformize field stars.
	// FIXME - can do this (possibly at several scales) in preprocessing.
	int cutnw, cutnh;

	verify_get_uniformize_scale(index->meta.cutnside, mo->scale, fieldW, fieldH, &cutnw, &cutnh);
	logverb("cut blocks: %i x %i\n", cutnw, cutnh);
	
	int* cutperm;
	int* bincounts;
	verify_uniformize_field(vf, fieldW, fieldH, cutnw, cutnh, &cutperm, &bincounts, NULL, NULL);

	int W, H;

	W = fieldW;
	H = fieldH;

	/*
	unsigned char* img = malloc(4*W*H);

	// draw images of index and field densities.
	double* idensity = calloc(W * H, sizeof(double));
	double* fdensity = calloc(W * H, sizeof(double));

	double iscale = 2. * sqrt((double)(W * H) / (NI * M_PI));
	double fscale = 2. * sqrt((double)(W * H) / (NF * M_PI));
	logverb("NI = %i; iscale = %g\n", NI, iscale);
	logverb("NF = %i; fscale = %g\n", NF, fscale);
	logverb("computing density images...\n");
	for (i=0; i<NI; i++)
		add_gaussian_to_image(idensity, W, H, indexpix[i*2 + 0], indexpix[i*2 + 1], iscale, 1.0, 3.0, 1);
	for (i=0; i<NF; i++)
		add_gaussian_to_image(fdensity, W, H, starxy_getx(vf->field, i), starxy_gety(vf->field, i), fscale, 1.0, 3.0, 1);

	double idmax=0, fdmax=0;
	for (i=0; i<(W*H); i++) {
		idmax = MAX(idmax, idensity[i]);
		fdmax = MAX(fdmax, fdensity[i]);
	}
	for (i=0; i<(W*H); i++) {
		unsigned char val = 255.5 * idensity[i] / idmax;
		img[i*4+0] = val;
		img[i*4+1] = val;
		img[i*4+2] = val;
		img[i*4+3] = 255;
	}
	cairoutils_write_png("idensity.png", img, W, H);
	for (i=0; i<(W*H); i++) {
		unsigned char val = 255.5 * fdensity[i] / fdmax;
		img[i*4+0] = val;
		img[i*4+1] = val;
		img[i*4+2] = val;
		img[i*4+3] = 255;
	}
	cairoutils_write_png("fdensity.png", img, W, H);

	free(idensity);
	free(fdensity);
	 */


	/*
	 // index star weights.
	 double* iweights = malloc(NI * sizeof(double));
	 for (i=0; i<NI; i++)
	 iweights[i] = 1.0;
	 // don't count index stars that are part of the matched quad.
	 for (i=0; i<NI; i++)
	 for (j=0; j<dimquads; j++)
	 if (starids[i] == mo->star[j])
	 iweights[i] = 0.0;

	 double ranksigma = 20.0;
	 int Nrankprobs = (int)(ranksigma * 5);
	 double* rankprobs = malloc(Nrankprobs * sizeof(double));
	 for (i=0; i<Nrankprobs; i++)
	 rankprobs[i] = exp(-(double)(i*i) / (2. * ranksigma * ranksigma));

	 double qc[2], qr2;

	 get_quad_center(vf, mo, qc, &qr2);
	 */


	// Remove reference and test stars that are part of the quad
	// (via "cutperm" for test stars)
	// -- OR, make the test star sigmas = infty.

	// remove ref stars that are part of the matched quad.
	k = 0;
	for (i=0; i<NI; i++) {
		if (!fake_match) {
			bool inquad = FALSE;
			for (j=0; j<dimquads; j++)
				if (starids[i] == mo->star[j]) {
					inquad = TRUE;
					break;
				}
			if (inquad)
				continue;
		}
		if (i != k) {
			memcpy(indexpix + 2*k, indexpix + 2*i, 2*sizeof(double));
			starids[k] = starids[i];
		}
		k++;
	}
	NI = k;

	// remove test stars that are part of the quad.
	double* testxy = malloc(2 * NF * sizeof(double));
	// ... but record their original indices so that we can look-back
	//int* testinds = malloc(NF * sizeof(int));

	k = 0;
	for (i=0; i<NF; i++) {
		int starindex = cutperm[i];
		if (!fake_match) {
			bool inquad = FALSE;
			for (j=0; j<dimquads; j++)
				if (starindex == mo->field[j]) {
					inquad = TRUE;
					break;
				}
			if (inquad)
				continue;
		}
		starxy_get(vf->field, starindex, testxy + 2*k);
		sigma2s[k] = sigma2s[starindex];
		// store their original indices in cutperm so we can look-back.
		cutperm[k] = starindex;
		k++;
	}
	NF = k;

	double logodds;
	int besti;

	logodds = verify_star_lists(indexpix, NI, testxy, sigma2s, NF,
								fieldW*fieldH, distractors, logratio_tobail,
								NULL, &besti);
	mo->logodds = logodds;

	if (logodds > logodds_tokeep) {
		int* rmatches;
		// Run again, saving results.
		// Stop after "besti" test objects.
		verify_star_lists(indexpix, NI, testxy, sigma2s, besti + 1,
						  fieldW*fieldH, distractors, logratio_tobail,
						  &rmatches, NULL);

		// FIXME - save mo->corr_*
		// (using 'rmatches', 'cutperm' and 'starids')
		for (i=0; i<NF; i++) {
		}

		free(rmatches);
	}

	free(cutperm);
	free(bincounts);

	free(testxy);

	free(sigma2s);
	free(indexpix);
	free(starids);

}


	/*
		// create the probability distribution map for this field star.
		double* pmap = malloc(W * H * sizeof(double));
		// background rate...
		for (j=0; j<(W*H); j++)
			pmap[j] = distractors / (fieldW*fieldH);

		double normrankprob;
		int dr;
		normrankprob = 0.0;
		for (j=0; j<NI; j++) {
			dr = abs(i - j);
			if (dr >= Nrankprobs)
				continue;
			normrankprob += rankprobs[dr];
		}
		for (j=0; j<NI; j++) {
			double r2, sig2, sig;

			dr = abs(i - j);
			if (dr >= Nrankprobs)
				continue;

			r2 = distsq(indexpix + j*2, qc, 2);
			sig2 = get_sigma2_at_radius(verify_pix2, r2, qr2);
			sig = sqrt(sig2);

			for (y = MAX(0, indexpix[j*2 + 1] - 5.0*sig);
				 y <= MIN(H-1, indexpix[j*2 + 1] + 5.0 * sig); y++) {
				for (x = MAX(0, indexpix[j*2 + 0] - 5.0*sig);
					 x <= MIN(W-1, indexpix[j*2 + 0] + 5.0 * sig); x++) {
					pmap[y*W + x] += iweights[j] * (rankprobs[dr] / normrankprob) *
						exp(-(square(indexpix[j*2+0]-x) + square(indexpix[j*2+1]-y)) / (2.0 * sig2)) *
						(1.0 / (2.0 * M_PI * sig2)) * (1.0 - distractors);
				}
			}
		}

		double maxval = 0.0;
		for (j=0; j<(W*H); j++)
			maxval = MAX(maxval, pmap[j]);

		double psum = 0.0;
		for (j=0; j<(W*H); j++)
			psum += pmap[j];
		logverb("Probability sum: %g\n", psum);

		char* fn;

		printf("Writing probability image %i\n", i);
		printf("maxval = %g\n", maxval);

		for (j=0; j<(W*H); j++) {
			unsigned char val = (unsigned char)MAX(0, MIN(255, 255.5 * sqrt(pmap[j] / maxval)));
			img[4*j + 0] = val;
			img[4*j + 1] = val;
			img[4*j + 2] = val;
			img[4*j + 3] = 255;
		}

		free(pmap);

		cairo_t* cairo;
		cairo_surface_t* target;

		target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);
		cairo = cairo_create(target);
		cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);

		// draw the quad.
		cairo_set_source_rgba(cairo, 0.0, 0.0, 1.0, 0.5);

		double starxy[DQMAX*2];
		double angles[DQMAX];
		int perm[DQMAX];
		int DQ = dimquads;

		for (k=0; k<DQ; k++)
            starxy_get(vf->field, mo->field[k], starxy + k*2);
		for (k=0; k<DQ; k++)
			angles[k] = atan2(starxy[k*2 + 1] - qc[1], starxy[k*2 + 0] - qc[0]);
		permutation_init(perm, DQ);
		permuted_sort(angles, sizeof(double), compare_doubles_asc, perm, DQ);
		for (k=0; k<DQ; k++) {
			double px, py;
			px = starxy[perm[k]*2+0];
			py = starxy[perm[k]*2+1];
			if (k == 0)
				cairo_move_to(cairo, px, py);
			else
				cairo_line_to(cairo, px, py);
		}
		cairo_close_path(cairo);
		cairo_stroke(cairo);

		// draw the source point.
		cairo_set_source_rgb(cairo, 1.0, 0.0, 0.0);
		cairoutils_draw_marker(cairo, CAIROUTIL_MARKER_CROSSHAIR, fxy[0], fxy[1], 3.0);
		cairo_stroke(cairo);

	 */


	/*
		// find index stars within n sigma...
		double nsigma = 5.0;
		double r2 = sigma2s[i] * nsigma*nsigma;
		int opts = KD_OPTIONS_COMPUTE_DISTS;

		kdtree_qres_t* res = kdtree_rangesearch_options(itree, fxy, r2, opts);

		logverb("found %i index stars within range.\n", res->nres);

		if (res->nres == 0)
			goto nextstar;

		double bestprob = 0.0;
		int bestarg = -1;

		for (j=0; j<res->nres; j++) {
			int ii = res->inds[j];
			dr = abs(i - ii);
			logverb("  index star %i: rank diff %i\n", ii, i-ii);
			if (dr >= Nrankprobs)
				continue;
			double prob = iweights[ii] * rankprobs[dr] *
				exp(-res->sdists[j] / (2.0 * sigma2s[i]));
			logverb("  -> prob %g\n", prob);
			if (prob > bestprob) {
				bestprob = prob;
				bestarg = j;
			}
		}

		if (bestarg == -1) {
			// FIXME -- distractor?
			goto nextstar;
		}

		int besti = res->inds[bestarg];
		double lostweight = bestprob / iweights[besti]; 
		// exp(-res->sdists[bestarg] / (2.0 * sigma2s[i]));
		iweights[besti] = MAX(0.0, iweights[besti] - lostweight);
		logverb("matched index star %i: dropping weight by %g; new weight %g.\n", besti, lostweight, iweights[besti]);
		
		bestprob *= (1.0 / (2.0 * M_PI * sigma2s[i])) / normrankprob * (1.0 - distractors);
		logverb("bestprob %g, vs background %g\n", bestprob, (1.0 / (double)(W * H)));
		
		kdtree_free_query(res);

		cairo_set_source_rgb(cairo, 0.0, 1.0, 0.0);
		cairoutils_draw_marker(cairo, CAIROUTIL_MARKER_CIRCLE, indexpix[2*besti+0], indexpix[2*besti+1], 3.0);
		cairo_stroke(cairo);


	nextstar:
		cairoutils_argb32_to_rgba(img, W, H);
		cairo_surface_destroy(target);
		cairo_destroy(cairo);
		asprintf(&fn, "logprob-%04i.png", i);
		cairoutils_write_png(fn, img, W, H);
		free(img);


		if (i >= 9)
			break;
	}

	 kdtree_free(itree);
	 free(ipixcopy);

	 */









	// Reduce the number of index stars so that the "radius of relevance" is bigger
	// than the field.
	//trim_index_stars(&indexpix, &starids, &NI);

	// Deduplicate field stars based on positional variance and rank.
	/*
	 bool* keepers = NULL;
	 keepers = deduplicate_field_stars(vf, sigma2s);
	 free(keepers);
	 */

	/*

	// Prime the array where we store conflicting-match info:
	// any match is an improvement, except for stars that form the matched quad.
	bestprob = malloc(NF * sizeof(double));
	for (i=0; i<NF; i++)
		bestprob[i] = -HUGE_VAL;
	// If we're verifying an existing WCS solution, then there is no match quad.
	if (!fake_match) {
		for (i=0; i<dimquads; i++) {
			assert(mo->field[i] >= 0);
			assert(mo->field[i] < NF);
			bestprob[mo->field[i]] = HUGE_VAL;
		}
	}

	// p(background) = 1/(W*H) of the image.
	logprob_background = -log(fieldW * fieldH);

	// p(distractor) = D / (W*H)
	logprob_distractor = log(distractors / (fieldW * fieldH));

	debug("log(p(background)) = %g\n", logprob_background);
	debug("log(p(distractor)) = %g\n", logprob_distractor);

	// add correspondences for the matched quad.
	corr_field = il_new(16);
	corr_index = il_new(16);
	if (!fake_match) {
		for (i=0; i<dimquads; i++) {
			il_append(corr_field, mo->field[i]);
			il_append(corr_index, mo->star[i]);
		}
	}

	bestlogodds = -HUGE_VAL;
	bestnmatch = bestnnomatch = bestnconflict = -1;
	nmatch = nnomatch = nconflict = 0;

	dl* probs = dl_new(16);

	// Add index stars.
	for (i=0; i<NI; i++) {
		double bestd2;
		double sigma2;
		double logprob = -HUGE_VAL;
		int ind;
		int starid;
		int fldind;
		bool cont;

		bool OLD = FALSE;

		logverb("\n\n");

		logverb("Throwing index star %i.\n", i);

		dl_append(probs, logodds);

		// Skip stars that are part of the quad:
		starid = starids[i];
		if (!fake_match) {
			cont = FALSE;
			for (j=0; j<dimquads; j++)
				if (starid == mo->star[j]) {
					cont = TRUE;
					logverb("Part of the quad: skipping.\n");
					break;
				}
			if (cont)
				continue;
		}

		// Find nearest field star.
		ind = kdtree_nearest_neighbour(vf->ftree, indexpix+i*2, &bestd2);
		fldind = vf->ftree->perm[ind];
		assert(fldind >= 0);
		assert(fldind < NF);

		sigma2 = sigma2s[fldind];

		logverb("Nearest field star: %i; dist %g; sigma %g; dist = %g sigmas.\n",
				fldind, sqrt(bestd2), sqrt(sigma2), sqrt(bestd2 / sigma2));
	 //logverb("Gaussian height = %g, vs background %g.\n", (1.0 - distractors) / (2.0 * M_PI * sigma2 * NF), exp(logprob_background));
		if (log((1.0 - distractors) / (2.0 * M_PI * sigma2 * NF)) < logprob_background) {
			logverb("This Gaussian is uninformative.\n");
			continue;
		}

		logprob = log((1.0 - distractors) / (2.0 * M_PI * sigma2 * NF)) - (bestd2 / (2.0 * sigma2));
	 //logverb("Logprob %g, vs distractor logprob %g (difference %g)\n",
	 //logprob, logprob_distractor, logprob - logprob_distractor);
		if (logprob < logprob_distractor) {
			logverb("Distractor.\n");
			logprob = logprob_distractor;
			nnomatch++;
		} else {
			double oldprob;
			logverb("Match (field star %i), logprob %g (background=%g, distractor=%g)\n", fldind, logprob, logprob_background, logprob_distractor);

			// DEBUG
			if (OLD) {
				logprob = log(exp(logprob) + distractors/(fieldW*fieldH));
			}

			oldprob = bestprob[fldind];
			if (oldprob != -HUGE_VAL) {
				nconflict++;
				// There was a previous match to this field object.
				if (oldprob == HUGE_VAL) {
					logverb("Conflicting match to one of the stars in the quad.\n");
				} else {
					logverb("Conflict: odds was %g, now %g.\n", oldprob, logprob);
				}
				// Allow an improved match (except to the stars composing the quad, because they are initialized to HUGE_VAL)
				if (logprob > oldprob) {
					int ind;
					logverb("The new match is better.\n");
					//oldprob = logprob;
					//logodds += (logprob - oldprob);
					// The old match must now be counted as a distractor.
					//logodds += (logprob_distractor - logprob_background);
					logverb("Switching the old match to a distractor: change of %g logprob.\n", (logprob_distractor - oldprob));

					// DEBUG
					if (OLD) {
						oldprob = logprob;
						logodds += (logprob - oldprob);
					} else {
						logodds += (logprob_distractor - oldprob);
					}

					bestprob[fldind] = logprob;
					// Replace the correspondence.
					ind = il_index_of(corr_field, fldind);
					il_set(corr_index, ind, starid);
				} else {
					logverb("The old match is better.\n");
					// We must count this as a distractor.

					// DEBUG
					if (OLD) {
					} else {
						logprob = logprob_distractor;
					}

				}
				// DEBUG
				if (OLD) {
					continue;
				}

			} else {
				bestprob[fldind] = logprob;
				nmatch++;
				il_append(corr_field, fldind);
				il_append(corr_index, starid);
			}
		}

		logodds += (logprob - logprob_background);
		logverb("Logodds now %g\n", logodds);

		if (logodds < logratio_tobail)
			break;

		if (logodds > bestlogodds) {
			bestlogodds = logodds;
			bestnmatch = nmatch;
			bestnnomatch = nnomatch;
			bestnconflict = nconflict;

			if (best_corr_field)
				il_free(best_corr_field);
			if (best_corr_index)
				il_free(best_corr_index);
			best_corr_field = il_dupe(corr_field);
			best_corr_index = il_dupe(corr_index);
		}
	}

	printf("probs = array([");
	for (i=0; i<dl_size(probs); i++)
		printf("%g,", dl_get(probs, i));
	printf("])\n");



	free(bestprob);

	mo->corr_field = best_corr_field;
	mo->corr_index = best_corr_index;

	il_free(corr_field);
	il_free(corr_index);

	mo->logodds = bestlogodds;
	mo->noverlap = bestnmatch;
	mo->nconflict = bestnconflict;
	mo->nfield = bestnmatch + bestnnomatch + bestnconflict;
	mo->nindex = NI;
	matchobj_compute_derived(mo);

	if (mo->logodds > log(1e9)) {
		logverb("breakpoint ahoy!\n");
	}
	 */

