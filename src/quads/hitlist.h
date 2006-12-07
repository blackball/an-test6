/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef HITLIST_H
#define HITLIST_H

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
	double agreedist2;

	// 12*Nside^2
	int npix;
	// per-pixel structs
	pixinfo* pix;

	/*
	  The way we store matches uses a level of indirection.

	  The "matchlist" list contains all the MatchObj (matches) we've seen:
	  eg, matchlist = [ m0, m1, m2, m3 ].

	  At each healpix, we have the "matchinds" list, which is a list of ints
	  that indexes into the "matchlist" list.
	  eg, healpix 0 might have matchinds = [ 0, 3 ], which means it own
	  matchlist[0] and [3], ie, m0 and m3.
	*/

	// master list of MatchObj*: matches
	pl* matchlist;
};
typedef struct hitlist_struct hitlist;


int hitlist_hits_agree(MatchObj* m1, MatchObj* m2, double maxagreedist2, double* p_agreedist2);

void hitlist_compute_vector(MatchObj* mo);

hitlist* hitlist_new(double AgreeTolArcSec, int maxNside);

void hitlist_remove_all(hitlist* hlist);

void hitlist_clear(hitlist* hlist);

void hitlist_free(hitlist* hlist);

//pl* hitlist_healpix_copy_list(hitlist* hlist, int agreelistindex);
//int hitlist_healpix_add_hit(hitlist* hlist, MatchObj* mo, int* p_agreelistindex);

int hitlist_add_hit(hitlist* hlist, MatchObj* mo);

pl* hitlist_get_agreeing(hitlist* hlist, int moindex, pl* alist);

#endif
