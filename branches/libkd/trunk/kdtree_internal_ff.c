#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL float
#define KDTYPE float

typedef float real;
typedef float kdtype;

#define KDT_INFTY KDT_INFTY_FLOAT
#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE_INTEGER 0

#define LOW_HR(kd, D, i)  LOW_HR_F(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_F(kd, D, i)

#include "kdtree_internal.c"

