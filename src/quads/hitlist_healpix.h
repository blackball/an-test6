#ifndef HITLIST_HEALPIX_H
#define HITLIST_HEALPIX_H

#include "hitlist.h"
#include "matchobj.h"

void hitlist_healpix_compute_vector(MatchObj* mo);

void hitlist_histogram_agreement_size(hitlist* hl, int* hist, int Nhist);

#endif
