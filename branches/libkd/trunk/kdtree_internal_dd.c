#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL double
#define KDTYPE double

typedef double real;
typedef double kdtype;

#define KDT_INFTY KDT_INFTY_DOUBLE
#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE_INTEGER 0

#define LOW_HR(kd, D, i)  LOW_HR_D(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_D(kd, D, i)

#include "kdtree_internal.c"

