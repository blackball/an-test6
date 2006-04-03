#ifndef DUALTREE_RANGE_SEARCH_H
#define DUALTREE_RANGE_SEARCH_H

extern double RANGESEARCH_NO_LIMIT;

#include "kdtree/kdtree.h"

typedef void (*result_callback)(void* extra, int xind, int yind,
								double dist2);

void dualtree_rangesearch(kdtree_t* x, kdtree_t* y,
						  double mindist, double maxdist,
						  result_callback callback,
						  void* callback_param);

void dualtree_rangecount(kdtree_t* x, kdtree_t* y,
						 double mindist, double maxdist,
						 int* counts);

#endif
