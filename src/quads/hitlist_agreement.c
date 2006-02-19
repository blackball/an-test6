#include "blocklist.h"
typedef blocklist hitlist;
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
	return blocklist_pointer_new(256);
}

void hitlist_free(hitlist* hl) {
	blocklist_free(hl);
}

int hitlist_count_best(hitlist* hl) {
    int i, N;
	int bestnum = 0;
    N = blocklist_count(hl);
    for (i=0; i<N; i++) {
		int M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hl, i);
		M = blocklist_count(hits);
		if (M > bestnum) {
			bestnum = M;
		}
	}
	return bestnum;
}

int hitlist_count_all(hitlist* hl) {
    int i, N;
	int sum = 0;
    N = blocklist_count(hl);
    for (i=0; i<N; i++) {
		int M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hl, i);
		M = blocklist_count(hits);
		sum += M;
	}
	return sum;
}

double distsq(double* d1, double* d2, int D) {
    double dist2;
    int i;
    dist2 = 0.0;
    for (i=0; i<D; i++) {
		dist2 += square(d1[i] - d2[i]);
    }
    return dist2;
}

blocklist* hitlist_get_best(hitlist* hl) {
    int i, N;
    int bestnum;
	blocklist* bestlist;
	blocklist* bestcopy;

	bestnum = 0;
	bestlist = NULL;
    N = blocklist_count(hl);
    for (i=0; i<N; i++) {
		int M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hl, i);
		M = blocklist_count(hits);
		if (M > bestnum) {
			bestnum = M;
			bestlist = hits;
		}
	}

	// shallow copy...
	bestcopy = blocklist_pointer_new(256);
	if (bestlist) {
		for (i=0; i<bestnum; i++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(bestlist, i);
			blocklist_pointer_append(bestcopy, mo);
		}
	}
	return bestcopy;
}

void hitlist_add_hits(hitlist* hl, blocklist* hits) {
	int i, N;
	N = blocklist_count(hits);
	for (i=0; i<N; i++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, i);
		hitlist_add_hit(hl, mo);
	}
}

blocklist* hitlist_get_all(hitlist* bl) {
	int i, j, M, N;

	blocklist* all = blocklist_pointer_new(256);
	N = blocklist_count(bl);
	for (i=0; i<N; i++) {
		blocklist* lst = (blocklist*)blocklist_pointer_access(bl, i);
		M = blocklist_count(lst);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(lst, j);
			blocklist_pointer_append(all, mo);
		}
	}
	return all;
}

int hitlist_add_hit(hitlist* hlist, MatchObj* mo) {
    int i, N;
    blocklist* newlist;
	blocklist* mergelist = NULL;

    N = blocklist_count(hlist);

    for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hlist, i);
		M = blocklist_count(hits);
		for (j=0; j<M; j++) {
			double d2;
			MatchObj* m = (MatchObj*)blocklist_pointer_access(hits, j);
			d2 = distsq(mo->vector, m->vector, MATCH_VECTOR_SIZE);
			if (d2 < square(AgreeTol)) {
				if (!mergelist) {
					blocklist_pointer_append(hits, mo);
					mergelist = hits;
				} else {
					// transitive merging...
					blocklist_append_list(mergelist, hits);
					blocklist_free(hits);
					blocklist_remove_index(hlist, i);
					i--;
					N--;
				}
				break;
			}
		}
    }
	if (mergelist) {
		return blocklist_count(mergelist);
	}

    // no agreement - create new list.
    newlist = blocklist_pointer_new(10);
    blocklist_pointer_append(newlist, mo);
    blocklist_pointer_append(hlist, newlist);

    return 1;
}

void hitlist_free_extra(hitlist* hl, void (*free_function)(MatchObj* mo)) {
    int i, N;
    N = blocklist_count(hl);
    for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hl, i);
		M = blocklist_count(hits);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, j);
			free_function(mo);
		}
	}
}

void hitlist_clear(hitlist* hl) {
    int i, N;
    N = blocklist_count(hl);
    for (i=0; i<N; i++) {
		int j, M;
		blocklist* hits = (blocklist*)blocklist_pointer_access(hl, i);
		M = blocklist_count(hits);
		for (j=0; j<M; j++) {
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, j);
			free_star(mo->sMin);
			free_star(mo->sMax);
			free_MatchObj(mo);
		}
		blocklist_pointer_free(hits);
    }
    blocklist_remove_all(hl);
}
