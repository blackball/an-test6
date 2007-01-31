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

#include "handlehits.h"
#include "verify.h"
#include "blind_wcs.h"

handlehits* handlehits_new() {
	handlehits* hh;
	hh = calloc(1, sizeof(handlehits));
	return hh;
}

void handlehits_free_matchobjs(handlehits* hh) {
	if (!hh) return;
	if (hh->hits)
		hitlist_free_matchobjs(hh->hits);
	if (hh->singles) {
		int i;
		for (i=0; i<pl_size(hh->singles); i++)
			free(pl_get(hh->singles, i));
		pl_remove_all(hh->singles);
	}
}

void handlehits_clear(handlehits* hh) {
	if (hh->hits)
		hitlist_clear(hh->hits);
	hh->nverified = 0;
	hh->bestmo = NULL;
	hh->bestmoindex = 0;
	free(hh->bestcorr);
	hh->bestcorr = NULL;
	if (hh->keepers)
		pl_free(hh->keepers);
	hh->keepers = NULL;
	if (hh->singles)
		pl_free(hh->singles);
	hh->singles = NULL;
}

void handlehits_free(handlehits* hh) {
	if (!hh) return;
	handlehits_clear(hh);
	if (hh->hits)
		hitlist_free(hh->hits);
	free(hh);
}

static bool verify(handlehits* hh, MatchObj* mo, int moindex) {
	int* corr = NULL;

	// this check is here to handle the "agreeable" case, where the overlap
	// has already been computed and stored in the matchfile, and we no
	// longer have access to the startree, etc.
	if (mo->overlap == 0.0) {
		tan_t origwcs;

		//if (hh->do_wcs) {
		if (hh->iter_wcs_steps) {
			int i;
			corr = malloc(hh->nfield * sizeof(int));
			for (i=0; i<hh->nfield; i++)
				corr[i] = -1;

			// copy the original WCS
			memcpy(&origwcs, &(mo->wcstan), sizeof(tan_t));
		}

		verify_hit(hh->startree, mo, hh->field, hh->nfield, hh->verify_dist2,
				   NULL, NULL, NULL,
				   NULL, NULL,
				   corr);

		mo->nverified = hh->nverified++;

		if (hh->iter_wcs_steps &&
			(mo->overlap >= hh->iter_wcs_thresh)) {

			verify_iterate_wcs(hh->startree, mo, hh->field,
							   hh->nfield, hh->verify_dist2,
							   corr,
							   hh->iter_wcs_steps,
							   hh->iter_wcs_rads);
		}


		if (hh->verified)
			hh->verified(hh, mo);
	}

	if ((mo->overlap < hh->overlap_tokeep) ||
		(mo->ninfield < hh->ninfield_tokeep)) {
		free(corr);
		return FALSE;
	}

	// This computes a WCS solution that uses all of the stars
	// that are (supposedly) in correspondence.
	/*
	  if (corr) {
	  blind_wcs_compute(mo, hh->field, hh->nfield, corr, &(mo->wcstan));
	  mo->wcs_valid = 1;
	  }
	*/

	if (!hh->keepers)
		hh->keepers = pl_new(32);
	pl_append(hh->keepers, mo);

	if ((mo->overlap < hh->overlap_tosolve) ||
		(mo->ninfield < hh->ninfield_tosolve)) {
		free(corr);
		return FALSE;
	}

	if (!hh->bestmo || (mo->overlap > hh->bestmo->overlap)) {
		hh->bestmo = mo;
		hh->bestmoindex = moindex;
		free(hh->bestcorr);
		hh->bestcorr = corr;
		corr = NULL;
	}

	free(corr);
	return TRUE;
}

bool handlehits_add(handlehits* hh, MatchObj* mo) {
	int moindex;
	bool solved = FALSE;

	if (hh->nagree_toverify == 0) {

		if (!hh->singles)
			hh->singles = pl_new(1024);

		// Required?
		//hitlist_compute_vector(mo);

		moindex = pl_size(hh->singles);
		pl_append(hh->singles, mo);
		solved |= verify(hh, mo, moindex);

	} else {
		pl* agreelist;
		int nagree;
		il* indlist;

		if (!hh->hits)
			hh->hits = hitlist_new(hh->agreetol, 0);

		// compute field center.
		hitlist_compute_vector(mo);
		indlist = il_new(8);
		moindex = hitlist_add_hit(hh->hits, mo);
		agreelist = hitlist_get_agreeing(hh->hits, moindex, NULL, indlist);
		nagree = 1 + (agreelist ? pl_size(agreelist) : 0);

		// This little jig dances around a strange case where not all the
		// matches that are part of an agreement cluster are written out, and as
		// a result, "agreeable" gives different answers than "blind".
		if (nagree > mo->nagree)
			mo->nagree = nagree;
		else
			nagree = mo->nagree;

		if (nagree < hh->nagree_toverify)
			goto cleanup;

		solved |= verify(hh, mo, moindex);

		if (nagree == hh->nagree_toverify) {
			// run verification on the other matches
			int j;
			for (j=0; agreelist && j<pl_size(agreelist); j++) {
				MatchObj* mo1 = pl_get(agreelist, j);
				if (mo1->overlap == 0.0) {
					int moind1 = il_get(indlist, j);
					solved |= verify(hh, mo1, moind1);
				}
			}
		}

	cleanup:
		if (agreelist) pl_free(agreelist);
		il_free(indlist);
	}
	return solved;
}

