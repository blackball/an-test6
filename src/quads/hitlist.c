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

#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "hitlist.h"
#include "bl.h"
#include "healpix.h"
#include "mathutil.h"

static void ensure_pixinfo_inited(pixinfo* node, int pix, int Nside) {
	uint neigh[8];
	uint nn;
	if (!node->neighbours) {
		nn = healpix_get_neighbours(pix, neigh, Nside);
		node->neighbours = malloc((nn+1) * sizeof(int));
		memcpy(node->neighbours, neigh, nn * sizeof(int));
		node->neighbours[nn] = -1;
	}
	if (!node->matchinds)
		node->matchinds = il_new(4);
}

static void clear_pixinfo(pixinfo* node) {
	if (node->neighbours) {
		free(node->neighbours);
		node->neighbours = NULL;
	}
	if (node->matchinds) {
		il_free(node->matchinds);
		node->matchinds = NULL;
	}
}

void hitlist_compute_vector(MatchObj* mo) {
	mo->center[0] = (mo->sMin[0] + mo->sMax[0]);
	mo->center[1] = (mo->sMin[1] + mo->sMax[1]);
	mo->center[2] = (mo->sMin[2] + mo->sMax[2]);
	normalize_3(mo->center);
}

hitlist* hitlist_new(double AgreeArcSec, int maxNside) {
	double AgreeTol2;
	int Nside;
	hitlist* hl;

	if (!maxNside)
		maxNside = 100;

	AgreeTol2 = arcsec2distsq(AgreeArcSec);

	/*
	  We want to choose Nside such that the healpixes have side length about
	  "AgreeArcSec".  We do this by noting that there are 12 * Nside^2
	  healpixes which have equal area, and the surface area of a sphere is
	  4 pi radians^2.  If this implies that "Nside" should be larger than
	  "maxNside", it is clamped to "maxNside".
	*/
	Nside = (int)sqrt((4.0 * M_PI / 12.0) / square(arcsec2rad(AgreeArcSec)));
	if (!Nside)
		Nside = 1;
	if (Nside > maxNside)
		Nside = maxNside;

	hl = calloc(1, sizeof(hitlist));
	hl->Nside = Nside;
	hl->npix = 12 * Nside * Nside;
	hl->pix = calloc(hl->npix, sizeof(pixinfo));
	if (!hl->pix) {
		fprintf(stderr, "hitlist: failed to malloc the pixel array: %i bytes: %s.\n",
				hl->npix * sizeof(pixinfo), strerror(errno));
		free(hl);
		return NULL;
	}
	hl->agreedist2 = AgreeTol2;
	hl->matchlist = pl_new(256);
	return hl;
}

// distance between proposed field centers.
int hitlist_hits_agree(MatchObj* m1, MatchObj* m2, double maxagreedist2, double* p_agreedist2) {
	double d2;

	// two matches to the same quad don't supply any additional evidence -
	// it's probably donuts or double-images or something.
	if (m1->quadno == m2->quadno) {
		if (p_agreedist2)
			*p_agreedist2 = -1.0;
		return 0;
	}

	d2 = distsq(m1->center, m2->center, 3);
	if (p_agreedist2)
		*p_agreedist2 = d2;
	if (d2 > maxagreedist2)
		return 0;
	return 1;
}

pl* hitlist_get_agreeing(hitlist* hlist, int moindex, pl* alist,
						 il* ilist) {
	MatchObj* match;
	int p, pix;
	pixinfo* pinfo;
	pl* agreelist;

	agreelist = (alist ? alist : NULL);

	match = pl_get(hlist->matchlist, moindex);

	// find which healpixel this hit belongs in
	pix = xyzarrtohealpix(match->center, hlist->Nside);
	pinfo = hlist->pix + pix;

	// look at all the MatchObjs in "pix" and its neighbouring pixels,
	// gathering agreements.
	for (p=-1;; p++) {
		int m, M;
		pixinfo* pixel;

		if (p == -1)
			// look at this pixel...
			pixel = pinfo;
		else if (pinfo->neighbours[p] == -1)
			break;
		else
			// look at the neighbour...
			pixel = hlist->pix + pinfo->neighbours[p];

		if (!pixel->matchinds) continue;

		// for each match in the pixel...
		M = il_size(pixel->matchinds);
		for (m=0; m<M; m++) {
			int ind;
			MatchObj* mo;

			// get the index of the MO
			ind = il_get(pixel->matchinds, m);
			// get the MO from the master list
			mo = (MatchObj*)pl_get(hlist->matchlist, ind);

			// don't count vacuous agreement with yourself...
			if (mo == match)
				continue;

			if (!hitlist_hits_agree(match, mo, hlist->agreedist2, NULL))
				continue;

			// this match agrees.
			if (!agreelist)
				agreelist = pl_new(4);

			pl_append(agreelist, mo);
			if (ilist)
				il_append(ilist, ind);
		}
	}
	return agreelist;
}

int hitlist_add_hit(hitlist* hlist, MatchObj* match) {
	int pix;
	int matchind;
	pixinfo* pinfo;

	// find which healpixel this hit belongs in
	pix = xyzarrtohealpix(match->center, hlist->Nside);
	pinfo = hlist->pix + pix;
	ensure_pixinfo_inited(pinfo, pix, hlist->Nside);

	// add this MatchObj to the master list
	pl_append(hlist->matchlist, match);
	matchind = pl_size(hlist->matchlist) - 1;

	// add this MatchObj's index to its pixel's list.
	il_append(pinfo->matchinds, matchind);

	return matchind;
}

static void do_clear(hitlist* hlist, int freeobjs) {
	int p;
	int m, M;
	for (p=0; p<hlist->npix; p++) {
		pixinfo* pix = hlist->pix + p;
		clear_pixinfo(pix);
	}
	M = pl_size(hlist->matchlist);
	if (freeobjs) {
		for (m=0; m<M; m++) {
			MatchObj* mo = (MatchObj*)pl_get(hlist->matchlist, m);
			if (!mo)
				continue;
			free_MatchObj(mo);
		}
	}
	pl_remove_all(hlist->matchlist);
}

/*
  void hitlist_remove_all(hitlist* hlist) {
  do_clear(hlist, 0);
  }
*/

void hitlist_clear(hitlist* hlist) {
	do_clear(hlist, 1);
}

void hitlist_free(hitlist* hl) {
	pl_free(hl->matchlist);
	free(hl->pix);
	free(hl);
}
