#include "blocklist.h"
typedef blocklist hitlist;
#define DONT_DEFINE_HITLIST
#include "hitlist.h"

char* hitlist_get_parameter_help() {
	return "";
}
char* hitlist_get_parameter_options() {
	return "";
}
int hitlist_process_parameter(char argchar, char* optarg) {
	return 0;
}

hitlist* hitlist_new() {
	return blocklist_pointer_new(256);
}

void hitlist_free(hitlist* hl) {
	blocklist_free(hl);
}

blocklist* hitlist_get_best(hitlist* hl) {
	blocklist* bestlist;

	if (!blocklist_count(hl))
		return NULL;

	// return an arbitrary list of length 1.
	bestlist = blocklist_pointer_new(32);
	blocklist_pointer_append(bestlist, blocklist_pointer_access(hl, 0));
	return bestlist;
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
	int i, N;

	blocklist* all = blocklist_pointer_new(256);
	// shallow copy.
	N = blocklist_count(bl);
	for (i=0; i<N; i++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(bl, i);
		blocklist_pointer_append(all, mo);
	}
	return all;
}

int hitlist_add_hit(hitlist* hlist, MatchObj* mo) {
	blocklist_pointer_append(hlist, mo);
    return 1;
}

void hitlist_clear(hitlist* hl) {
    int i, N;
    N = blocklist_count(hl);
	for (i=0; i<N; i++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hl, i);
		free_star(mo->sMin);
		free_star(mo->sMax);
		free_MatchObj(mo);
	}
    blocklist_remove_all_but_first(hl);
}



