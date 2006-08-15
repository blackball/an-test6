#ifndef VERIFY_H
#define VERIFY_H

#include "kdtree.h"
#include "matchobj.h"
#include "xylist.h"
#include "bl.h"

void verify_hit(kdtree_t* startree, MatchObj* mo, double* field,
				int nfield, double verify_dist2,
				int* pmatches, int* punmatches, int* pconflicts,
				il* indexstars, dl* bestd2s);


#endif
