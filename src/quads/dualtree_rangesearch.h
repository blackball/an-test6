#ifndef DUALTREE_RANGE_SEARCH_H
#define DUALTREE_RANGE_SEARCH_H

extern double RANGESEARCH_NO_LIMIT;

#include "kdtree.h"

// note, 'xind' and 'yind' are indices IN THE KDTREE; to get back to
// 'normal' ordering you must use the kdtree permutation vector.
typedef void (*result_callback)(void* extra, int xind, int yind,
								double dist2);

typedef void (*progress_callback)(void* extra, int ydone);

void dualtree_rangesearch(kdtree_t* x, kdtree_t* y,
						  double mindist, double maxdist,
						  result_callback callback,
						  void* callback_param,
						  progress_callback progress,
						  void* progress_param);

void dualtree_rangecount(kdtree_t* x, kdtree_t* y,
						 double mindist, double maxdist,
						 int* counts);

#endif
