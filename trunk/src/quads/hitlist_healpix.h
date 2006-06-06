#ifndef HITLIST_HEALPIX_H
#define HITLIST_HEALPIX_H

#include "matchobj.h"

#ifndef DONT_DEFINE_HITLIST
typedef void hitlist;
#endif

void hitlist_healpix_compute_vector(MatchObj* mo);

void hitlist_healpix_histogram_agreement_size(hitlist* hl, int* hist, int Nhist);

hitlist* hitlist_healpix_new(double AgreeTolArcSec);

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

#endif
