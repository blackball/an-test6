#include <math.h>

#include "bl.h"
typedef pl hitlist;
#define DONT_DEFINE_HITLIST
#include "hitlist.h"

#define DEFAULT_AGREE_TOL 7.0

double AgreeArcSec = DEFAULT_AGREE_TOL;
double AgreeTol = 0.0;

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

hitlist* hitlist_new() {
	return pl_new(256);
}

void hitlist_free(hitlist* hl) {
	pl_free(hl);
}

int hitlist_count_best(hitlist* hl) {
    int i, N;
	int bestnum = 0;
    N = pl_size(hl);
    for (i=0; i<N; i++) {
		int M;
		pl* hits = (pl*)pl_get(hl, i);
		M = pl_size(hits);
		if (M > bestnum) {
			bestnum = M;
		}
	}
	return bestnum;
}

int hitlist_count_all(hitlist* hl) {
    int i, N;
	int sum = 0;
    N = pl_size(hl);
    for (i=0; i<N; i++) {
		int M;
		pl* hits = (pl*)pl_get(hl, i);
		M = pl_size(hits);
		sum += M;
	}
	return sum;
}

pl* hitlist_get_best(hitlist* hl) {
    int i, N;
    int bestnum;
	pl* bestlist;
	pl* bestcopy;

	bestnum = 0;
	bestlist = NULL;
    N = pl_size(hl);
    for (i=0; i<N; i++) {
		int M;
		pl* hits = (pl*)pl_get(hl, i);
		M = pl_size(hits);
		if (M > bestnum) {
			bestnum = M;
			bestlist = hits;
		}
	}

	// shallow copy...
	bestcopy = pl_new(256);
	if (bestlist) {
		for (i=0; i<bestnum; i++) {
			MatchObj* mo = (MatchObj*)pl_get(bestlist, i);
			pl_append(bestcopy, mo);
		}
	}
	return bestcopy;
}

/*
  void hitlist_add_hits(hitlist* hl, blocklist* hits) {
  int i, N;
  N = pl_size(hits);
  for (i=0; i<N; i++) {
  MatchObj* mo = (MatchObj*)pl_get(hits, i);
  hitlist_add_hit(hl, mo);
  }
  }
*/

pl* hitlist_get_all(hitlist* bl) {
	int i, j, M, N;

	pl* all = pl_new(256);
	N = pl_size(bl);
	for (i=0; i<N; i++) {
		pl* lst = (pl*)pl_get(bl, i);
		M = pl_size(lst);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)pl_get(lst, j);
			pl_append(all, mo);
		}
	}
	return all;
}

int hitlist_add_hit(hitlist* hlist, MatchObj* mo) {
    int i, N;
    pl* newlist;
	pl* mergelist = NULL;

    N = pl_size(hlist);

    for (i=0; i<N; i++) {
		int j, M;
		pl* hits = (pl*)pl_get(hlist, i);
		M = pl_size(hits);
		for (j=0; j<M; j++) {
			double d2;
			MatchObj* m = (MatchObj*)pl_get(hits, j);
			d2 = distsq(mo->vector, m->vector, MATCH_VECTOR_SIZE);
			if (d2 < square(AgreeTol)) {
				if (!mergelist) {
					blocklist_pointer_append(hits, mo);
					mergelist = hits;
				} else {
					// transitive merging...
					blocklist_append_list(mergelist, hits);
					blocklist_free(hits);
					// DEBUG
					blocklist_pointer_set(hlist, i, NULL);
					
					pl_remove_index(hlist, i);
					i--;
					N--;
				}
				break;
			}
		}
    }
	if (mergelist) {
		return pl_size(mergelist);
	}

    // no agreement - create new list.
    //newlist = blocklist_pointer_new(10);
    newlist = pl_new(1);
    pl_append(newlist, mo);
    pl_append(hlist, newlist);

    return 1;
}

void hitlist_free_extra(hitlist* hl, void (*free_function)(MatchObj* mo)) {
    int i, N;
    N = pl_size(hl);
    for (i=0; i<N; i++) {
		int j, M;
		pl* hits = (pl*)pl_get(hl, i);
		M = pl_size(hits);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)pl_get(hits, j);
			free_function(mo);
		}
	}
}

void hitlist_clear(hitlist* hl) {
    int i, N;
    N = pl_size(hl);
    for (i=0; i<N; i++) {
		int j, M;
		pl* hits = (pl*)pl_get(hl, i);
		M = pl_size(hits);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)pl_get(hits, j);
			free_MatchObj(mo);
		}
		pl_free(hits);
    }
    pl_remove_all(hl);
}
