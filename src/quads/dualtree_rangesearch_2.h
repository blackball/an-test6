#ifndef DUALTREE_RANGE_SEARCH_2_H
#define DUALTREE_RANGE_SEARCH_2_H

extern double RANGESEARCH2_NO_LIMIT;

#include "kdtree/kdtree.h"

typedef void (*result_callback_2)(void* extra, int xind, int yind,
								  double dist2);

void dualtree_rangesearch_2(kdtree_t* x, kdtree_t* y,
							double mindist, double maxdist,
							result_callback_2 callback,
							void* callback_param);

#endif
