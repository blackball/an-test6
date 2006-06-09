#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "bl.h"
#include "healpix.h"
#include "intmap.h"
#include "mathutil.h"

struct pixinfo;
typedef struct pixinfo pixinfo;

// per-healpix struct
struct pixinfo {
	// list of indexes into the master MatchObj list, of MatchObjects
	// that belong to this pix.
	il* matchinds;
	// list of neighbouring healpixes, terminated by -1.
	uint* neighbours;
};

// a hitlist (one per field)
struct hitlist_struct {
	int Nside;

	int nbest;
	int ntotal;
	il* best;
	int npix;
	pixinfo* pix;
	double agreedist2;

	/*
	  The way we store lists of agreeing matches is a bit complicated,
	  because we need to be able to rapidly merge lists that both agree
	  with a new match.

	  The "matchlist" list contains all the MatchObj (matches) we've seen:
	  eg, matchlist = [ m0, m1, m2, m3 ].

	  At each healpix, we have the "matchinds" list, which is a list of ints
	  that indexes into the "matchlist" list.
	  eg, healpix 0 might have matchinds = [ 0, 3 ], which means it own
	  matchlist[0] and [3], ie, m0 and m3.
	  healpix 1 would have [ 1, 2 ]

	  We also have a list of agreeing matches; again, these are lists of ints
	  that index into the "matchlist".
	  eg, agreelist = [ a0, a1, a2 ]
	  agreement cluster a0 = [ 0 ]
	                    a1 = [ 2, 3 ]
						a2 = [ 1 ]
	  which would mean that matchlist[0], ie m0, agrees with itself, and
	  m2 and m3 agree with each other.

	  Finally, we have the "memberlist", a list of ints, that says, for
	  each element in matchlist, which agreelist it belongs to.
	  In our running example,
	  memberlist = [ 0, 2, 1, 1 ]
	  because m0 belongs to a0, hence memberlist[0] = 0
	          m1            a2                  [1] = 2
			  m2, m3        a1                [2,3] = 1
	 */


	// master list of MatchObj*: matches
	pl* matchlist;

	// list of ints, same size as matchlist; containing indexes into
	// agreelist; the list to which the corresponding MatchObj
	// belongs.
	il* memberlist;

	// list of il*; the lists of agreeing matches.
	//    (which are lists of indices into "matchlist".)
	pl* agreelist;

	// same size as agreelist; contains "intmap*" objects that hold the
	// mapping between field and catalog objects.
	// (IFF do_correspond is TRUE).
	pl* correspondlist;

	int do_correspond;
};
typedef struct hitlist_struct hitlist;

#define DONT_DEFINE_HITLIST
#include "hitlist_healpix.h"

pl* hitlist_healpix_copy_list(hitlist* hlist, int agreelistindex) {
	il* agree;
	int i, N, ind;
	pl* copy;
	agree = (il*)pl_get(hlist->agreelist, agreelistindex);
	// shallow copy.
	copy = pl_new(32);
	N = il_size(agree);
	for (i=0; i<N; i++) {
		MatchObj* mo;
		ind = il_get(agree, i);
		mo = (MatchObj*)pl_get(hlist->matchlist, ind);
		pl_append(copy, mo);
	}
	return copy;
}

pl* hitlist_healpix_get_list_containing(hitlist* hlist, MatchObj* mo) {
	int i, N;
	int ind;
	// find the index of this MatchObj in "matchlist"...
	N = pl_size(hlist->matchlist);
	for (i=0; i<N; i++) {
		if (mo == pl_get(hlist->matchlist, i))
			break;
	}
	if (i == N)
		// not found!
		return NULL;
	// which agreelist does it belong to?
	ind = il_get(hlist->memberlist, i);
	return hitlist_healpix_copy_list(hlist, ind);
}

void hitlist_healpix_histogram_agreement_size(hitlist* hl, int* hist, int Nhist) {
	int m, M;
	int N;
	M = pl_size(hl->agreelist);
	for (m=0; m<M; m++) {
		il* lst = (il*)pl_get(hl->agreelist, m);
		if (!lst) continue;
		N = il_size(lst);
		if (N >= Nhist)
			N = Nhist-1;
		hist[N]++;
	}
}

void hitlist_healpix_compute_vector(MatchObj* mo) {
	double x1,y1,z1;
	double x2,y2,z2;
	double xc,yc,zc;
	double dx, dy, dz;
	double xn, yn, zn;
	double xe, ye, ze;
	double pn, pe;
	double rac, decc, arc;
	double r;
	double rotation;

	// we don't have to normalize because it's already done by the solver.
	x1 = mo->sMin[0];
	y1 = mo->sMin[1];
	z1 = mo->sMin[2];

	x2 = mo->sMax[0];
	y2 = mo->sMax[1];
	z2 = mo->sMax[2];

	xc = (x1 + x2) / 2.0;
	yc = (y1 + y2) / 2.0;
	zc = (z1 + z2) / 2.0;
	r = sqrt(square(xc) + square(yc) + square(zc));
	xc /= r;
	yc /= r;
	zc /= r;

	arc  = 60.0 * rad2deg(distsq2arc(square(x2-x1)+square(y2-y1)+square(z2-z1)));

	// we will characterise the rotation of the field with
	// respect to the "North" vector - the vector tangent
	// to the field pointing in the direction of increasing
	// Dec.  We take the derivatives of (x,y,z) wrt Dec,
	// at the center of the field.

	// {x,y,z}(dec) = {cos(ra)cos(dec), sin(ra)cos(dec), sin(dec)}
	// d{x,y,z}/ddec = {-cos(ra)sin(dec), -sin(ra)sin(dec), cos(dec)}

	// center of the field...
	rac  = xy2ra(xc, yc);
	decc = z2dec(zc);

	// North
	xn = -cos(rac) * sin(decc);
	yn = -sin(rac) * sin(decc);
	zn =             cos(decc);

	// "East" (or maybe West - direction of increasing RA, whatever that is.)
	xe = -sin(rac) * cos(decc);
	ye =  cos(rac) * cos(decc);
	ze = 0.0;

	// Now we look at the vector from the center to sMin
	dx = x1 - xc;
	dy = y1 - yc;
	dz = z1 - zc;

	// Project that onto N, E: dot products
	pn = (dx * xn) + (dy * yn) + (dz * zn);
	pe = (dx * xe) + (dy * ye) + (dz * ze);

	rotation = atan2(pn, pe);

	r = sqrt(square(pn) + square(pe));
	pn /= r;
	pe /= r;

	mo->vector[0] = xc;
	mo->vector[1] = yc;
	mo->vector[2] = zc;
	mo->vector[3] = arc;
	mo->vector[4] = pn;
	mo->vector[5] = pe;
}

void init_pixinfo(pixinfo* node, int pix, int Nside) {
	node->matchinds = NULL;
	//node->healpix = pix;
	//node->nn = healpix_get_neighbours_nside(pix, node->neighbours, Nside);
	//node->nn = -1;
	node->neighbours = NULL;
}

void ensure_pixinfo_inited(pixinfo* node, int pix, int Nside) {
	uint neigh[8];
	uint nn;
	if (!node->neighbours) {
		nn = healpix_get_neighbours_nside(pix, neigh, Nside);
		node->neighbours = malloc((nn+1) * sizeof(int));
		memcpy(node->neighbours, neigh, nn * sizeof(int));
		node->neighbours[nn] = -1;
	}
	/*
	  if (node->nn == -1) {
	  node->nn = healpix_get_neighbours_nside(pix, node->neighbours, Nside);
	  }
	*/
	if (!node->matchinds) {
		node->matchinds = il_new(4);
	}
}

void clear_pixinfo(pixinfo* node) {
	if (node->neighbours) {
		free(node->neighbours);
		node->neighbours = NULL;
	}
	if (node->matchinds) {
		il_free(node->matchinds);
		node->matchinds = NULL;
	}
}

hitlist* hitlist_healpix_new(double AgreeArcSec) {
	int p;
	double AgreeTol2;
	int Nside;
	hitlist* hl;

	AgreeTol2 = arcsec2distsq(AgreeArcSec);

	// We want to choose Nside such that the small healpixes
	// have side length about AgreeArcSec.  We do this by
	// noting that there are 12 * Nside^2 healpixes which
	// have equal area, and the surface area of a sphere
	// is 4 pi radians^2.

	/*
	  fprintf(stderr, "agree arcsec=%g, rad=%g, square rad=%g, pix per hp=%g, nside=%g.\n",
	  AgreeArcSec, arcsec2rad(AgreeArcSec), square(arcsec2rad(AgreeArcSec)), (4.0 * M_PI / 12.0) / square(arcsec2rad(AgreeArcSec)),
	  sqrt((4.0 * M_PI / 12.0) / square(arcsec2rad(AgreeArcSec))));
	*/

	Nside = (int)sqrt((4.0 * M_PI / 12.0) / square(arcsec2rad(AgreeArcSec)));
	if (!Nside)
		Nside = 1;
	if (Nside > 128)
		Nside = 128;

	hl = malloc(sizeof(hitlist));
	hl->Nside = Nside;
	hl->ntotal = 0;
	hl->nbest = 0;
	hl->best = NULL;
	hl->npix = 12 * Nside * Nside;
	hl->pix = malloc(hl->npix * sizeof(pixinfo));
	if (!hl->pix) {
		fprintf(stderr, "hitlist_healpix: failed to malloc the pixel array: %i bytes: %s.\n",
				hl->npix * sizeof(pixinfo), strerror(errno));
		return NULL;
	}

	for (p=0; p<hl->npix; p++) {
		init_pixinfo(hl->pix + p, p, Nside);
	}
	hl->agreedist2 = AgreeTol2;
	hl->matchlist = pl_new(16);
	hl->memberlist = il_new(16);
	hl->agreelist = pl_new(8);
	hl->correspondlist = pl_new(8);
	hl->do_correspond = 1;

	return hl;
}

int hits_agree(MatchObj* m1, MatchObj* m2, double agreedist2) {
	// old-fashioned metric:
	double vec1[6];
	double vec2[6];

	vec1[0] = m1->sMin[0];
	vec1[1] = m1->sMin[1];
	vec1[2] = m1->sMin[2];
	vec1[3] = m1->sMax[0];
	vec1[4] = m1->sMax[1];
	vec1[5] = m1->sMax[2];

	vec2[0] = m2->sMin[0];
	vec2[1] = m2->sMin[1];
	vec2[2] = m2->sMin[2];
	vec2[3] = m2->sMax[0];
	vec2[4] = m2->sMax[1];
	vec2[5] = m2->sMax[2];

	if (distsq_exceeds(vec1, vec2, 6, agreedist2))
		return 0;
	return 1;
}

int hitlist_healpix_add_hit(hitlist* hlist, MatchObj* match,
							int* p_agreelistind) {
	int pix;
	int p;
	double x,y,z;
	int mergeind = -1;
	int matchind;
	int nn;
	pixinfo* pinfo;
	il* mergelist;

	// find which healpixel this hit belongs in
	x = match->vector[0];
	y = match->vector[1];
	z = match->vector[2];
	pix = xyztohealpix_nside(x, y, z, hlist->Nside);

	pinfo = hlist->pix + pix;

	// add this MatchObj to the master list
	pl_append(hlist->matchlist, match);
	matchind = pl_size(hlist->matchlist) - 1;
	// add a placeholder agreement-list-membership value.
	il_append(hlist->memberlist, -1);

	ensure_pixinfo_inited(pinfo, pix, hlist->Nside);

	// add this MatchObj's index to its pixel's list.
	il_append(pinfo->matchinds, matchind);

	// look at all the MatchObjs in "pix" and its neighbouring pixels.
	// merge all the lists containing matches with which it agrees.

	mergelist = NULL;
	mergeind = -1;

	for (nn=0;; nn++)
		if (pinfo->neighbours[nn] == -1)
			break;

	// for each neighbouring pixel...
	for (p=-1; p<nn; p++) {
		int m, M;
		pixinfo* pixel;

		if (p == -1)
			// look at this pixel...
			pixel = pinfo;
		else
			// look at the neighbours...
			pixel = hlist->pix + pinfo->neighbours[p];

		if (!pixel->matchinds) continue;

		// for each match in the pixel...
		M = il_size(pixel->matchinds);
		for (m=0; m<M; m++) {
			int ind;
			MatchObj* mo;
			int agreeind;

			// get the index of the MO
			ind = il_get(pixel->matchinds, m);
			// get the MO from the master list
			mo = (MatchObj*)pl_get(hlist->matchlist, ind);

			if (!hits_agree(match, mo, hlist->agreedist2))
				continue;

			// this match agrees.

			// to which agreement list does it belong?
			agreeind = il_get(hlist->memberlist, ind);
			if (agreeind == -1) continue;

			// are we checking correspondence?
			if (hlist->do_correspond) {
				intmap* map = (intmap*)pl_get(hlist->correspondlist, agreeind);
				if (intmap_conflicts(map, match->field[0], match->star[0]) ||
					intmap_conflicts(map, match->field[1], match->star[1]) ||
					intmap_conflicts(map, match->field[2], match->star[2]) ||
					intmap_conflicts(map, match->field[3], match->star[3])) {
					continue;
				}
			}

			if (mergeind == -1) {
				// this is the first agreeing match we've found.  Add this MatchObj to
				// that list; we'll also merge any other agree-ers into that list.
				mergeind = agreeind;
				mergelist = (il*)pl_get(hlist->agreelist, mergeind);
				il_append(mergelist, matchind);
			} else if (agreeind == mergeind) {
				// this agreeing match is already a member of the right agreement list.
				// do nothing.
			} else {
				// merge lists.
				il* agreelist;
				int a, A;
				agreelist = (il*)pl_get(hlist->agreelist, agreeind);
				A = il_size(agreelist);
				// go through this agreement list and tell all its members that they
				// now belong to mergelist.
				for (a=0; a<A; a++) {
					int aind = il_get(agreelist, a);
					il_set(hlist->memberlist, aind, mergeind);
				}

				// append all the elements of this agreement list to 'mergelist';
				il_merge_lists(mergelist, agreelist);
				il_free(agreelist);
				pl_set(hlist->agreelist, agreeind, NULL);

				// merge the field->catalog correspondences:
				if (hlist->do_correspond) {
					intmap *map1, *map2;
					map1 = (intmap*)pl_get(hlist->correspondlist, mergeind);
					map2 = (intmap*)pl_get(hlist->correspondlist, agreeind);
					intmap_merge(map1, map2);
					intmap_free(map2);
					pl_set(hlist->correspondlist, agreeind, NULL);
				}
			}
		}
	}

	// If this match didn't agree with anything...
	if (mergeind == -1) {
		int agreeind;
		int a, A;
		il* newlist;
		intmap* newmap = NULL;

		// We didn't find any agreeing matches.  Create a new agreement
		// list and add this MatchObj to it.
		newlist = il_new(4);
		if (hlist->do_correspond) {
			newmap = intmap_new();
			intmap_add(newmap, match->field[0], match->star[0]);
			intmap_add(newmap, match->field[1], match->star[1]);
			intmap_add(newmap, match->field[2], match->star[2]);
			intmap_add(newmap, match->field[3], match->star[3]);
		}

		// find an available spot in agreelist:
		A = pl_size(hlist->agreelist);
		agreeind = -1;
		for (a=0; a<A; a++) {
			if (pl_get(hlist->agreelist, a))
				continue;
			// found one!
			agreeind = a;
			pl_set(hlist->agreelist, a, newlist);
			if (hlist->do_correspond)
				pl_set(hlist->correspondlist, a, newmap);
			break;
		}
		if (agreeind == -1) {
			// add it to the end.
			agreeind = pl_size(hlist->agreelist);
			pl_append(hlist->agreelist, newlist);
			if (hlist->do_correspond)
				pl_append(hlist->correspondlist, newmap);
		}
		il_append(newlist, matchind);

		mergeind = agreeind;
		mergelist = newlist;
	}

	// Record which agreement list this match ended up in.
	// (replacing the placeholder we put in above)
	il_set(hlist->memberlist, matchind, mergeind);

	// See if we just created the new best list...
	if (il_size(mergelist) > hlist->nbest) {
		hlist->nbest = il_size(mergelist);
		hlist->best = mergelist;
	}

	// Increment the total number of matches...
	hlist->ntotal++;

	if (p_agreelistind)
		*p_agreelistind = mergeind;

	return il_size(mergelist);
}

void hitlist_healpix_clear(hitlist* hlist) {
	int p;
	int m, M;
	for (p=0; p<hlist->npix; p++) {
		pixinfo* pix = hlist->pix + p;
		clear_pixinfo(pix);
	}
	M = pl_size(hlist->matchlist);
	for (m=0; m<M; m++) {
		MatchObj* mo = (MatchObj*)pl_get(hlist->matchlist, m);
		if (!mo)
			continue;
		free_MatchObj(mo);
	}
	pl_remove_all(hlist->matchlist);
	il_remove_all(hlist->memberlist);
	M = pl_size(hlist->agreelist);
	for (m=0; m<M; m++) {
		il* list;
		if (hlist->do_correspond) {
			intmap* map = (intmap*)pl_get(hlist->correspondlist, m);
			if (map)
				intmap_free(map);
		}
		list = (il*)pl_get(hlist->agreelist, m);
		if (list)
			il_free(list);
	}
	pl_remove_all(hlist->agreelist);
	pl_remove_all(hlist->correspondlist);
}

void hitlist_healpix_free_extra(hitlist* hl, void (*free_function)(MatchObj* mo)) {
	int m, M;
	M = pl_size(hl->matchlist);
	for (m=0; m<M; m++) {
		MatchObj* mo = (MatchObj*)pl_get(hl->matchlist, m);
		if (!mo)
			continue;
		free_function(mo);
	}
}

void hitlist_healpix_free(hitlist* hl) {
	pl_free(hl->matchlist);
	il_free(hl->memberlist);
	pl_free(hl->agreelist);
	pl_free(hl->correspondlist);
	free(hl->pix);
	free(hl);
}


// returns one big list containing all lists of agreers of length >= len.
pl* hitlist_healpix_get_all_above_size(hitlist* hl, int len) {
	pl* copy;
	int m, M;
	int i, N;
	copy = pl_new(16);
	M = pl_size(hl->agreelist);
	for (m=0; m<M; m++) {
		il* lst = (il*)pl_get(hl->agreelist, m);
		if (!lst) continue;
		N = il_size(lst);
		if (N < len) continue;
		for (i=0; i<N; i++) {
			MatchObj* mo;
			int ind = il_get(lst, i);
			mo = (MatchObj*)pl_get(hl->matchlist, ind);
			pl_append(copy, mo);
		}
	}
	return copy;
}

// returns shallow copies of all sets of hits with size
// equal to the best.
pl* hitlist_healpix_get_all_best(hitlist* hl) {
	return hitlist_healpix_get_all_above_size(hl, hl->nbest);
}

// returns a shallow copy of the best set of hits.
// you are responsible for calling pl_free.
pl* hitlist_healpix_get_best(struct hitlist_struct* hl) {
	pl* copy;
	int m, M;

	copy = pl_new(16);
	if (!hl->best) return copy;
	M = il_size(hl->best);
	for (m=0; m<M; m++) {
		int ind;
		MatchObj* mo;
		ind = il_get(hl->best, m);
		mo = (MatchObj*)pl_get(hl->matchlist, ind);
		pl_append(copy, mo);
	}
	return copy;
}

// returns a shallow copy of the whole set of results.
// you are responsible for calling pl_free.
pl* hitlist_healpix_get_all(hitlist* hl) {
	pl* copy;
	int m, M;

	copy = pl_new(1024);
	M = pl_size(hl->matchlist);
	for (m=0; m<M; m++) {
		MatchObj* mo = (MatchObj*)pl_get(hl->matchlist, m);
		pl_append(copy, mo);
	}
	return copy;
}

int hitlist_healpix_count_best(hitlist* hitlist) {
	return hitlist->nbest;
}

int hitlist_healpix_count_all(hitlist* hitlist) {
	return hitlist->ntotal;
}

