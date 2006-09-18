#include <assert.h>
#include <math.h>

#include "verify.h"
#include "mathutil.h"
#include "intmap.h"

/*
  #include "kdtree.h"
  #define KD_DIM 3
  #include "kdtree.h"
  #undef KD_DIM
*/

void verify_hit(kdtree_t* startree,
				MatchObj* mo,
				double* field,
				int NF,
				double verify_dist2,
				int* pmatches,
				int* punmatches,
				int* pconflicts,
				il* indexstars,
				dl* bestd2s) {
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
	int levels;
	int Nleaf = 5;
	double* dptr;
	int Nmin;

	assert(mo->transform_valid);
	assert(startree);

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
			l1 += (res->results[j*3 + i] - mo->sMin[i]) * vec1[i];
			l2 += (res->results[j*3 + i] - mo->sMin[i]) * vec2[i];
		}
		if ((l1 >= 0.0) && (l1 <= len1) &&
			(l2 >= 0.0) && (l2 <= len2)) {
			if (j != NI)
				memmove(res->results + NI * 3,
						res->results +  j * 3,
						3 * sizeof(double));
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

	/*
	  Comment for the picky: in the rangesearch below, we grab a range
	  around each field star (at the moment this range is typically set to
	  3 arcsec).  This could grab an index star that isn't actually within
	  the bounds of the field that we computed above.  Since SDSS fields
	  are ~600 arcsec on a side, this is a pretty small extra area and we
	  cavalierly decide to ignore it.
	*/

	Nmin = imin(NI, NF);

	fieldstars = malloc(3 * Nmin * sizeof(double));
	dptr = field;
	for (i=0; i<Nmin; i++) {
		double u, v;
		u = *dptr;
		dptr++;
		v = *dptr;
		dptr++;
		image_to_xyz(u, v, fieldstars + 3*i, mo->transform);
	}

	/*
	  A counteracting note to the picky: if we build a kdtree over the
	  index stars within range, the above comment is no longer true.
	*/
    levels = kdtree_compute_levels(NI, Nleaf);
	itree = kdtree_build(res->results, NI, 3, levels);

	matches = unmatches = conflicts = 0;
	map = intmap_new(INTMAP_ONE_TO_ONE);

	// insert dual-tree here :)
	for (i=0; i<Nmin; i++) {
		double bestd2;
		int ind = kdtree_nearest_neighbour_within(itree, fieldstars + 3*i, verify_dist2, &bestd2);
		if (ind == -1) {
			unmatches++;
		} else {
			if (intmap_add(map, ind, i) == -1)
				// a field object already selected star 'ind' as its nearest neighbour.
				conflicts++;
			else
				matches++;
		}
		if (ind != -1 && bestd2s)
			dl_append(bestd2s, bestd2);
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
