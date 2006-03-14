#ifndef HITLIST_H_
#define HITLIST_H_

#include "bl.h"
#include "solver.h"
#include "matchobj.h"

#ifndef DONT_DEFINE_HITLIST
typedef void hitlist;
#endif

char* hitlist_get_parameter_help(void);
char* hitlist_get_parameter_options(void);
int hitlist_process_parameter(char argchar, char* optarg);
void hitlist_set_default_parameters(void);

pl* hitlist_get_all_above_size(hitlist* hl, int len);

hitlist* hitlist_new(void);

void hitlist_clear(hitlist* hlist);

void hitlist_free_extra(hitlist* hlist, void (*free_function)(MatchObj* mo));

void hitlist_free(hitlist* hlist);

// returns a shallow copy of the best set of hits.
// you are responsible for calling blocklist_free.
pl* hitlist_get_best(hitlist* hlist);

pl* hitlist_get_all_best(hitlist* hlist);

// returns a shallow copy of the whole set of results.
// you are responsible for calling blocklist_free.
pl* hitlist_get_all(hitlist* hlist);

int hitlist_add_hit(hitlist* hlist, MatchObj* mo);

//void hitlist_add_hits(hitlist* hlist, blocklist* hits);

int hitlist_count_best(hitlist* hitlist);

int hitlist_count_all(hitlist* hitlist);

#endif
