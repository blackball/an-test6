#ifndef SOLVER_CALLBACKS_H
#define SOLVER_CALLBACKS_H

#include <stdint.h>

#include "starutil.h"

/**
   You have to define these three functions!
*/
void getquadids(uint thisquad, uint *iA, uint *iB, uint *iC, uint *iD);

void getstarcoord(uint iA, double *star);

uint64_t getstarid(uint iA);

#endif
