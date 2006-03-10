//#include <time.h>
#include <errno.h>
#include <string.h>

#include "blocklist.h"
#include "starutil.h"
#include "healpix.h"
#include "intmap.h"

struct pixinfo;
typedef struct pixinfo pixinfo;

// per-healpix struct
struct pixinfo {
	// list of indexes into the master MatchObj list, of MatchObjects
	// that belong to this pix.
	blocklist* matchinds;
	// list of neighbouring healpixes.
	int healpix;
	uint neighbours[8];
	int nn;
};

// a hitlist (one per field)
struct hitlist_struct {
	int Nside;

	int nbest;
	int ntotal;
	blocklist* best;
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
	blocklist* matchlist;

	// list of ints, same size as matchlist; containing indexes into
	// agreelist; the list to which the corresponding MatchObj
	// belongs.
	blocklist* memberlist;

	// list of blocklist*; the lists of agreeing matches.
	//    (which are lists of indices into "matchlist".)
	blocklist* agreelist;

	// same size as agreelist; contains "intmap" objects that hold the
	// mapping between field and catalog objects.
	// (IFF do_correspond is TRUE).
	blocklist* correspondlist;

	int do_correspond;
};
typedef struct hitlist_struct hitlist;

#define DONT_DEFINE_HITLIST
#include "hitlist_healpix.h"

void hitlist_healpix_histogram_agreement_size(hitlist* hl, int* hist, int Nhist) {
	int m, M;
	int N;
	M = blocklist_count(hl->agreelist);
	for (m=0; m<M; m++) {
		blocklist* lst = (blocklist*)blocklist_pointer_access(hl->agreelist, m);
		if (!lst) continue;
		N = blocklist_count(lst);
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
	x1 = star_ref(mo->sMin, 0);
	y1 = star_ref(mo->sMin, 1);
	z1 = star_ref(mo->sMin, 2);

	x2 = star_ref(mo->sMax, 0);
	y2 = star_ref(mo->sMax, 1);
	z2 = star_ref(mo->sMax, 2);

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
	node->healpix = pix;
	//node->nn = healpix_get_neighbours_nside(pix, node->neighbours, Nside);
	node->nn = -1;
}

void ensure_pixinfo_inited(pixinfo* node, int pix, int Nside) {
	if (node->nn == -1) {
		node->nn = healpix_get_neighbours_nside(pix, node->neighbours, Nside);
	}
	if (!node->matchinds) {
		node->matchinds = blocklist_int_new(4);
	}
}

hitlist* hitlist_healpix_new(double AgreeArcSec) {
	int p;
	double AgreeTol2;
	int Nside;

	//time_t tstart, tend;
	//tstart = time(NULL);

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
	//if (Nside > 1024)
	//Nside = 1024;
	if (Nside > 256)
		Nside = 256;
	//fprintf(stderr, "Chose Nside=%i.\n", Nside);

	hitlist* hl = (hitlist*)malloc(sizeof(hitlist));
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
	hl->matchlist = blocklist_pointer_new(16);
	hl->memberlist = blocklist_int_new(16);
	hl->agreelist = blocklist_pointer_new(8);
	hl->correspondlist = blocklist_new(8, sizeof(intmap));
	hl->do_correspond = 1;

	/*
	  tend = time(NULL);
	  fprintf(stderr, "hitlist_healpix_new took %i sec.\n",
	  (int)(tend - tstart));
	*/
	return hl;
}

int hits_agree(MatchObj* m1, MatchObj* m2, double agreedist2) {
	// old-fashioned metric:
	double vec1[6];
	double vec2[6];
	double d2;

	vec1[0] = star_ref(m1->sMin, 0);
	vec1[1] = star_ref(m1->sMin, 1);
	vec1[2] = star_ref(m1->sMin, 2);
	vec1[3] = star_ref(m1->sMax, 0);
	vec1[4] = star_ref(m1->sMax, 1);
	vec1[5] = star_ref(m1->sMax, 2);

	vec2[0] = star_ref(m2->sMin, 0);
	vec2[1] = star_ref(m2->sMin, 1);
	vec2[2] = star_ref(m2->sMin, 2);
	vec2[3] = star_ref(m2->sMax, 0);
	vec2[4] = star_ref(m2->sMax, 1);
	vec2[5] = star_ref(m2->sMax, 2);

	d2 = distsq(vec1, vec2, 6);
	if (d2 < agreedist2) {
		return 1;
	}
	return 0;
}

int hitlist_healpix_add_hit(hitlist* hlist, MatchObj* match) {
	int pix;
	int p;
	double x,y,z;
	int mergeind = -1;
	int matchind;
	pixinfo* pinfo;
	blocklist* mergelist;

	// find which healpixel this hit belongs in
	x = match->vector[0];
	y = match->vector[1];
	z = match->vector[2];
	pix = xyztohealpix_nside(x, y, z, hlist->Nside);

	pinfo = hlist->pix + pix;

	// add this MatchObj to the master list
	blocklist_pointer_append(hlist->matchlist, match);
	matchind = blocklist_count(hlist->matchlist) - 1;
	// add a placeholder agreement-list-membership value.
	blocklist_int_append(hlist->memberlist, -1);

	ensure_pixinfo_inited(pinfo, pix, hlist->Nside);

	// add this MatchObj's index to its pixel's list.
	blocklist_int_append(pinfo->matchinds, matchind);

	// look at all the MatchObjs in "pix" and its neighbouring pixels.
	// merge all the lists containing matches with which it agrees.

	mergelist = NULL;
	mergeind = -1;


	// for each neighbouring pixel...
	for (p=-1; p<pinfo->nn; p++) {
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
		M = blocklist_count(pixel->matchinds);
		for (m=0; m<M; m++) {
			int ind;
			MatchObj* mo;
			int agreeind;

			// get the index of the MO
			ind = blocklist_int_access(pixel->matchinds, m);
			// get the MO from the master list
			mo = (MatchObj*)blocklist_pointer_access(hlist->matchlist, ind);

			if (!hits_agree(match, mo, hlist->agreedist2))
				continue;

			// this match agrees.

			// to which agreement list does it belong?
			agreeind = blocklist_int_access(hlist->memberlist, ind);
			if (agreeind == -1) continue;

			// are we checking correspondence?
			if (hlist->do_correspond) {
				intmap* map = (intmap*)blocklist_access(hlist->correspondlist, agreeind);
				if (intmap_conflicts(map, match->fA, match->iA) ||
					intmap_conflicts(map, match->fB, match->iB) ||
					intmap_conflicts(map, match->fC, match->iC) ||
					intmap_conflicts(map, match->fD, match->iD)) {
					continue;
				}
			}

			if (mergeind == -1) {
				// this is the first agreeing match we've found.  Add this MatchObj to
				// that list; we'll also merge any other agree-ers into that list.
				mergeind = agreeind;
				mergelist = (blocklist*)blocklist_pointer_access(hlist->agreelist, mergeind);
				blocklist_int_append(mergelist, matchind);
			} else if (agreeind == mergeind) {
				// this agreeing match is already a member of the right agreement list.
				// do nothing.
			} else {
				// merge lists.
				blocklist* agreelist;
				int a, A;
				agreelist = (blocklist*)blocklist_pointer_access(hlist->agreelist, agreeind);
				A = blocklist_count(agreelist);
				// go through this agreement list and tell all its members that they
				// now belong to mergelist.
				for (a=0; a<A; a++) {
					int aind = blocklist_int_access(agreelist, a);
					blocklist_int_set(hlist->memberlist, aind, mergeind);
				}

				// append all the elements of this agreement list to 'mergelist';
				blocklist_append_list(mergelist, agreelist);
				blocklist_free(agreelist);
				blocklist_pointer_set(hlist->agreelist, agreeind, NULL);

				// merge the field->catalog correspondences:
				if (hlist->do_correspond) {
					intmap *map1, *map2;
					map1 = (intmap*)blocklist_access(hlist->correspondlist, mergeind);
					map2 = (intmap*)blocklist_access(hlist->correspondlist, agreeind);
					intmap_merge(map1, map2);
					intmap_clear(map2);
				}
			}
		}
	}

	// If this match didn't agree with anything...
	if (mergeind == -1) {
		int agreeind;
		int a, A;
		blocklist* newlist;
		intmap newmap;

		// We didn't find any agreeing matches.  Create a new agreement
		// list and add this MatchObj to it.
		newlist = blocklist_int_new(4);
		if (hlist->do_correspond) {
			intmap_init(&newmap);
			intmap_add(&newmap, match->fA, match->iA);
			intmap_add(&newmap, match->fB, match->iB);
			intmap_add(&newmap, match->fC, match->iC);
			intmap_add(&newmap, match->fD, match->iD);
		}

		// find an available spot in agreelist:
		A = blocklist_count(hlist->agreelist);
		agreeind = -1;
		for (a=0; a<A; a++) {
			if (blocklist_pointer_access(hlist->agreelist, a))
				continue;
			// found one!
			agreeind = a;
			blocklist_pointer_set(hlist->agreelist, a, newlist);
			if (hlist->do_correspond)
				blocklist_set(hlist->correspondlist, a, &newmap);
			break;
		}
		if (agreeind == -1) {
			// add it to the end.
			agreeind = blocklist_count(hlist->agreelist);
			blocklist_pointer_append(hlist->agreelist, newlist);
			if (hlist->do_correspond)
				blocklist_append(hlist->correspondlist, &newmap);
		}
		blocklist_int_append(newlist, matchind);

		mergeind = agreeind;
		mergelist = newlist;
	}

	// Record which agreement list this match ended up in.
	// (replacing the placeholder we put in above)
	blocklist_int_set(hlist->memberlist, matchind, mergeind);

	// See if we just created the new best list...
	if (blocklist_count(mergelist) > hlist->nbest) {
		hlist->nbest = blocklist_count(mergelist);
		hlist->best = mergelist;
	}

	// Increment the total number of matches...
	hlist->ntotal++;

	return 0;
}


void hitlist_healpix_clear(hitlist* hlist) {
	int p;
	int m, M;
	for (p=0; p<hlist->npix; p++) {
		pixinfo* pix = hlist->pix + p;
		if (pix->matchinds) {
			blocklist_free(pix->matchinds);
			pix->matchinds = NULL;
		}
	}
	M = blocklist_count(hlist->matchlist);
	for (m=0; m<M; m++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hlist->matchlist, m);
		if (!mo)
			continue;
		free_star(mo->sMin);
		free_star(mo->sMax);
		free_MatchObj(mo);
	}
	blocklist_remove_all(hlist->matchlist);
	blocklist_remove_all(hlist->memberlist);
	M = blocklist_count(hlist->agreelist);
	for (m=0; m<M; m++) {
		if (hlist->do_correspond) {
			intmap* map = (intmap*)blocklist_access(hlist->correspondlist, m);
			if (map)
				intmap_clear(map);
		}
		blocklist* list = (blocklist*)blocklist_pointer_access(hlist->agreelist, m);
		if (!list)
			continue;
		blocklist_free(list);
	}
	blocklist_remove_all(hlist->agreelist);
	blocklist_remove_all(hlist->correspondlist);
}

void hitlist_healpix_free_extra(hitlist* hl, void (*free_function)(MatchObj* mo)) {
	int m, M;
	M = blocklist_count(hl->matchlist);
	for (m=0; m<M; m++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hl->matchlist, m);
		if (!mo)
			continue;
		free_function(mo);
	}
}

void hitlist_healpix_free(hitlist* hl) {
	blocklist_free(hl->matchlist);
	blocklist_free(hl->memberlist);
	blocklist_free(hl->agreelist);
	blocklist_free(hl->correspondlist);
	free(hl->pix);
	free(hl);
}


// returns one big list containing all lists of agreers of length >= len.
blocklist* hitlist_healpix_get_all_above_size(hitlist* hl, int len) {
	blocklist* copy;
	int m, M;
	int i, N;
	copy = blocklist_pointer_new(16);
	M = blocklist_count(hl->agreelist);
	for (m=0; m<M; m++) {
		blocklist* lst = (blocklist*)blocklist_pointer_access(hl->agreelist, m);
		if (!lst) continue;
		N = blocklist_count(lst);
		if (N < len) continue;
		for (i=0; i<N; i++) {
			MatchObj* mo;
			int ind = blocklist_int_access(lst, i);
			mo = (MatchObj*)blocklist_pointer_access(hl->matchlist, ind);
			blocklist_pointer_append(copy, mo);
		}
	}
	return copy;
}

// returns shallow copies of all sets of hits with size
// equal to the best.
blocklist* hitlist_healpix_get_all_best(hitlist* hl) {
	return hitlist_healpix_get_all_above_size(hl, hl->nbest);
}

// returns a shallow copy of the best set of hits.
// you are responsible for calling blocklist_free.
blocklist* hitlist_healpix_get_best(hitlist* hl) {
	blocklist* copy;
	int m, M;

	copy = blocklist_pointer_new(16);
	if (!hl->best) return copy;
	M = blocklist_count(hl->best);
	for (m=0; m<M; m++) {
		int ind;
		MatchObj* mo;
		ind = blocklist_int_access(hl->best, m);
		mo = (MatchObj*)blocklist_pointer_access(hl->matchlist, ind);
		blocklist_pointer_append(copy, mo);
	}
	return copy;
}

// returns a shallow copy of the whole set of results.
// you are responsible for calling blocklist_free.
blocklist* hitlist_healpix_get_all(hitlist* hl) {
	blocklist* copy;
	int m, M;

	copy = blocklist_pointer_new(1024);
	M = blocklist_count(hl->matchlist);
	for (m=0; m<M; m++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hl->matchlist, m);
		blocklist_pointer_append(copy, mo);
	}
	return copy;
}

int hitlist_healpix_count_best(hitlist* hitlist) {
	return hitlist->nbest;
}

int hitlist_healpix_count_all(hitlist* hitlist) {
	return hitlist->ntotal;
}

