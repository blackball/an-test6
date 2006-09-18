#ifndef HITLIST_HEALPIX_H
#define HITLIST_HEALPIX_H

#include "matchobj.h"

// per-healpix struct
struct pixinfo {
	// list of indexes into the master MatchObj list, of MatchObjects
	// that belong to this pix.
	il* matchinds;
	// list of neighbouring healpixes, terminated by -1.
	uint* neighbours;
};
typedef struct pixinfo pixinfo;

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


int hitlist_hits_agree(MatchObj* m1, MatchObj* m2, double maxagreedist2, double* p_agreedist2);


int hitlist_healpix_count_lists(hitlist* hl);

void hitlist_healpix_compute_vector(MatchObj* mo);

void hitlist_healpix_histogram_agreement_size(hitlist* hl, int* hist, int Nhist);

hitlist* hitlist_healpix_new(double AgreeTolArcSec);

void hitlist_healpix_remove_all(hitlist* hlist);

void hitlist_healpix_clear(hitlist* hlist);

void hitlist_healpix_free_extra(hitlist* hlist, void (*free_function)(MatchObj* mo));

void hitlist_healpix_free(hitlist* hlist);

pl* hitlist_healpix_get_list_containing(hitlist* hlist, MatchObj* mo);

// returns a shallow copy of the best set of hits.
// you are responsible for calling pl_free.
pl* hitlist_healpix_get_best(hitlist* hlist);

pl* hitlist_healpix_get_all_best(hitlist* hlist);

// returns a shallow copy of the whole set of results.
// you are responsible for calling pl_free.
pl* hitlist_healpix_get_all(hitlist* hlist);

pl* hitlist_healpix_copy_list(hitlist* hlist, int agreelistindex);

int hitlist_healpix_add_hit(hitlist* hlist, MatchObj* mo,
							int* p_agreelistindex);

int hitlist_healpix_count_best(hitlist* hitlist);

int hitlist_healpix_count_all(hitlist* hitlist);

void hitlist_healpix_print_dists_to_lists(hitlist* hl, MatchObj* mo);

double hitlist_healpix_closest_dist_to_lists(hitlist* hl, MatchObj* mo);

#endif
