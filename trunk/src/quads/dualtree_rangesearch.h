#ifndef DUALTREE_RANGE_SEARCH_H
#define DUALTREE_RANGE_SEARCH_H

//#define _GNU_SOURCE 1
//#include <math.h>

extern double RANGESEARCH_NO_LIMIT;

#include "KD/kdtree.h"

typedef void (*result_callback)(void* extra, int xind, int yind,
				double dist2);

void dualtree_rangesearch(kdtree* x, kdtree* y,
			  double mindist, double maxdist,
			  result_callback callback,
			  void* callback_param);

#endif
