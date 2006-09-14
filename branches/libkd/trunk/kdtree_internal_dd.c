#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL double
#define KDTYPE double

typedef double real;
typedef double kdtype;

#define REAL_MAX  KDT_INFTY_DOUBLE
#define REAL_MIN -KDT_INFTY_DOUBLE
#define KDT_INFTY KDT_INFTY_DOUBLE

#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE_INTEGER 0

#define KDTYPE_MIN -KDT_INFTY
#define KDTYPE_MAX  KDT_INFTY

#define LOW_HR(kd, D, i)  LOW_HR_D(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_D(kd, D, i)

// ????
#define KD_SPLIT(kd, i) KD_SPLIT_I(kd, i)

#include "kdtree_internal.c"

