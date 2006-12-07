#include "handlehits.h"
#include "verify.h"

handlehits* handlehits_new() {
	handlehits* hh;
	hh = calloc(1, sizeof(handlehits));
	return hh;
}

void handlehits_clear(handlehits* hh) {
	if (hh->hits)
		hitlist_clear(hh->hits);
	hh->hits = NULL;
	hh->nverified = 0;
	hh->bestmo = NULL;
	hh->bestmoindex = 0;
	free(hh->bestcorr);
	hh->bestcorr = NULL;
	if (hh->keepers)
		pl_free(hh->keepers);
	hh->keepers = NULL;
}

void handlehits_free(handlehits* hh) {
	handlehits_clear(hh);
	hitlist_free(hh->hits);
	free(hh);
}

static bool verify(handlehits* hh, MatchObj* mo, int moindex) {
	int* corr = NULL;

	if (hh->do_corr) {
		int i;
		corr = malloc(hh->nfield * sizeof(int));
		for (i=0; i<hh->nfield; i++)
			corr[i] = -1;
	}

	verify_hit(hh->startree, mo, hh->field, hh->nfield, hh->verify_dist2,
			   NULL, NULL, NULL,
			   NULL, NULL,
			   corr);
	mo->nverified = hh->nverified++;

	if ((mo->overlap < hh->overlap_tokeep) ||
		(mo->ninfield < hh->ninfield_tokeep)) {
		free(corr);
		return FALSE;
	}

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
	pl* agreelist;
	int moindex;
	bool solved = FALSE;
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
	if (agreelist) pl_remove_all(agreelist);
	il_free(indlist);
	return solved;
}


