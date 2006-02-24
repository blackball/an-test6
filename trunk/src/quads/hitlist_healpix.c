#include "blocklist.h"
#include "starutil.h"
#include "healpix.h"

#define NSIDE 128
//#define NSIDE 32
//#define AGREE_DIST 0.001

#define DEFAULT_AGREE_TOL 7.0
double AgreeArcSec = DEFAULT_AGREE_TOL;
double AgreeTol = 0.0;

struct pixinfo;
typedef struct pixinfo pixinfo;

struct pixinfo {
	// list of indexes into the master MatchObj list, of MatchObjects
	// that belong to this pix.
	blocklist* matchinds;
	// list of neighbouring healpixes.
	int healpix;
	int neighbours[8];
	int nn;
};

struct hitlist_struct {
	int nbest;
	int ntotal;
	blocklist* best;
	int npix;
	pixinfo pix[12*NSIDE*NSIDE];
	double agreedist2;

	// master list of MatchObj*: matches
	blocklist* matchlist;
	// list of ints, same size as matchlist; containing indexes into
	// agreelist; the list to which the corresponding MatchObj
	// belongs.
	blocklist* memberlist;
	// list of blocklist*; the lists of agreeing matches.
	//    (which are lists of indices into "matchlist".)
	blocklist* agreelist;
};
typedef struct hitlist_struct hitlist;

#define DONT_DEFINE_HITLIST
#include "hitlist.h"

char* hitlist_get_parameter_help() {
	return "   [-m agree_tol(arcsec)]\n";
}
char* hitlist_get_parameter_options() {
	return "m:";
}
int hitlist_process_parameter(char argchar, char* optarg) {
	switch (argchar) {
	case 'm':
		AgreeArcSec = strtod(optarg, NULL);
		AgreeTol = sqrt(2.0) * radscale2xyzscale(arcsec2rad(AgreeArcSec));
		break;
	}
	return 0;
}
void hitlist_set_default_parameters() {
	AgreeTol = sqrt(2.0) * radscale2xyzscale(arcsec2rad(AgreeArcSec));
}

void init_pixinfo(pixinfo* node, int pix) {
	node->matchinds = NULL;
	node->healpix = pix;
	node->nn = healpix_get_neighbours_nside(pix, node->neighbours,
											NSIDE);
}

hitlist* hitlist_new() {
	int p;
	hitlist* hl = (hitlist*)malloc(sizeof(hitlist));
	hl->ntotal = 0;
	hl->nbest = 0;
	hl->best = NULL;
	hl->npix = 12 * NSIDE * NSIDE;
	for (p=0; p<hl->npix; p++) {
		init_pixinfo(&(hl->pix[p]), p);
	}
	hl->agreedist2 = square(AgreeTol);

	hl->matchlist = blocklist_pointer_new(256);
	hl->memberlist = blocklist_int_new(256);
	hl->agreelist = blocklist_pointer_new(64);

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

int hitlist_add_hit(hitlist* hlist, MatchObj* match) {
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
	pix = xyztohealpix_nside(x, y, z, NSIDE);

	pinfo = hlist->pix + pix;

	// add this MatchObj to the master list
	blocklist_pointer_append(hlist->matchlist, match);
	matchind = blocklist_count(hlist->matchlist) - 1;
	// add a placeholder agreement-list-membership value.
	blocklist_int_append(hlist->memberlist, -1);
	// add this MatchObj's index to its pixel's list.
	if (!pinfo->matchinds) {
		pinfo->matchinds = blocklist_int_new(32);
	}
	blocklist_int_append(pinfo->matchinds, matchind);


	// look at all the MatchObjs in "pix" and its neighbouring pixels.
	// merge all the lists containing matches with which it agrees.

	mergelist = NULL;
	mergeind = -1;
	// for each pixel...
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
			}
		}
	}

	// If this match didn't agree with anything...
	if (mergeind == -1) {
		int agreeind;
		int a, A;
		blocklist* newlist;

		// We didn't find any agreeing matches.  Create a new agreement
		// list and add this MatchObj to it.
		newlist = blocklist_int_new(4);

		// find an available spot in agreelist:
		A = blocklist_count(hlist->agreelist);
		agreeind = -1;
		for (a=0; a<A; a++) {
			if (blocklist_pointer_access(hlist->agreelist, a))
				continue;
			// found one!
			agreeind = a;
			blocklist_pointer_set(hlist->agreelist, a, newlist);
			break;
		}
		if (agreeind == -1) {
			// add it to the end.
			agreeind = blocklist_count(hlist->agreelist);
			blocklist_pointer_append(hlist->agreelist, newlist);
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


void hitlist_clear(hitlist* hlist) {
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
		blocklist* list = (blocklist*)blocklist_pointer_access(hlist->agreelist, m);
		if (!list)
			continue;
		blocklist_free(list);
	}
	blocklist_remove_all(hlist->agreelist);
}

void hitlist_free_extra(hitlist* hl, void (*free_function)(MatchObj* mo)) {
	int m, M;
	M = blocklist_count(hl->matchlist);
	for (m=0; m<M; m++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hl->matchlist, m);
		if (!mo)
			continue;
		free_function(mo);
	}
}

void hitlist_free(hitlist* hl) {
	blocklist_free(hl->matchlist);
	blocklist_free(hl->memberlist);
	blocklist_free(hl->agreelist);
	free(hl);
}


// returns one big list containing all lists of agreers of length >= len.
blocklist* hitlist_get_all_above_size(hitlist* hl, int len) {
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


// returns a shallow copy of the best set of hits.
// you are responsible for calling blocklist_free.
blocklist* hitlist_get_best(hitlist* hl) {
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
blocklist* hitlist_get_all(hitlist* hl) {
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

void hitlist_add_hits(hitlist* hl, blocklist* hits) {
	int i, N;
	N = blocklist_count(hits);
	for (i=0; i<N; i++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, i);
		hitlist_add_hit(hl, mo);
	}
}

int hitlist_count_best(hitlist* hitlist) {
	return hitlist->nbest;
}

int hitlist_count_all(hitlist* hitlist) {
	return hitlist->ntotal;
}

