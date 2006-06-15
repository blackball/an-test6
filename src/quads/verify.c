#include <assert.h>
#include <math.h>

#include "verify.h"
#include "mathutil.h"
#include "intmap.h"

void verify_hit(kdtree_t* startree,
				MatchObj* mo,
				xy* field,
				double verify_dist2,
				int* pmatches,
				int* punmatches,
				int* pconflicts) {
	int i, j, NF;
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
	res = kdtree_rangesearch(startree, fieldcenter, fieldr2 * 1.01);
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
			memmove(res->results+NI*3, res->results+j*3, 3*sizeof(double));
			NI++;
		}
	}

	kdtree_free_query(res);

	/*
	  Comment for the picky: in the rangesearch below, we grab an range
	  around each field star (at the moment this range is typically set to
	  3 arcsec).  This could grab an index star that isn't actually within
	  the bounds of the field that we computed above.  Since SDSS fields
	  are ~600 arcsec on a side, this is a pretty small extra area and we
	  cavalierly decide to ignore it.
	*/

	NF = xy_size(field);
	fieldstars = malloc(3 * NF * sizeof(double));
	for (i=0; i<NF; i++) {
		double u, v;
		u = xy_refx(field, i);
		v = xy_refy(field, i);
		image_to_xyz(u, v, fieldstars + 3*i, mo->transform);
	}

	/*
	  A counteracting note to the picky: if we build a kdtree over the
	  index stars within range, the above comment is no longer true.
	*/
    levels = (int)ceil(log(NI / (double)Nleaf) * M_LOG2E);
    if (levels < 1)
        levels = 1;
	itree = kdtree_build(res->results, NI, 3, levels);

	matches = unmatches = conflicts = 0;
	map = intmap_new(INTMAP_ONE_TO_ONE);
	for (i=0; i<NF; i++) {
		// HACK - will it be faster to do NF (~300) kdtree searches, or
		// NF * NI distance computations?  Or build a kdtree over NI and
		// do nearest-neighbour search in that!

		double bestd2;
		//int ind = kdtree_nearest_neighbour(startree, fieldstars + 3*i, &bestd2);
		int ind = kdtree_nearest_neighbour(itree, fieldstars + 3*i, &bestd2);
		if (bestd2 <= verify_dist2) {
			if (intmap_add(map, ind, i) == -1)
				// a field object already selected star 'ind' as its nearest neighbour.
				conflicts++;
			else
				matches++;
		} else
			unmatches++;
	}

	kdtree_free(itree);

	mo->noverlap = matches - conflicts;
	mo->ninfield = NI;
	mo->overlap = (matches - conflicts) / (double)NI;

	if (pmatches)
		*pmatches = matches;
	if (punmatches)
		*punmatches = unmatches;
	if (pconflicts)
		*pconflicts = conflicts;

	intmap_free(map);
	free(fieldstars);
}
