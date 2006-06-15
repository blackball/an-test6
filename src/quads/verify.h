#ifndef VERIFY_H
#define VERIFY_H

#include "kdtree.h"
#include "matchobj.h"
#include "xylist.h"

void verify_hit(kdtree_t* startree, MatchObj* mo, xy* field,
				double verify_dist2,
				int* pmatches, int* punmatches, int* pconflicts);


#endif
