#ifndef HITLIST_H_
#define HITLIST_H_

#include "blocklist.h"
#include "solver2.h"
#include "matchobj.h"

#ifndef DONT_DEFINE_HITLIST
typedef void hitlist;
#endif

char* hitlist_get_parameter_help();
char* hitlist_get_parameter_options();
int hitlist_process_parameter(char argchar, char* optarg);


hitlist* hitlist_new();

void hitlist_clear(hitlist* hlist);

void hitlist_free(hitlist* hlist);

blocklist* hitlist_get_best(hitlist* hlist);

blocklist* hitlist_get_all(hitlist* hlist);

int hitlist_add_hit(hitlist* hlist, MatchObj* mo);

void hitlist_add_hits(hitlist* hlist, blocklist* hits);

int hitlist_get_n_best(hitlist* hitlist);

#endif
