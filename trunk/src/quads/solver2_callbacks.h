#ifndef SOLVER2_CALLBACKS_H_
#define SOLVER2_CALLBACKS_H_

#define NO_KD_INCLUDES 1
#include "starutil.h"

/**
   You have to define these two functions!
*/
void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD);

void getstarcoord(uint iA, double *star);

#endif
