#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL float
#define KDTYPE float

typedef float real;
typedef float kdtype;

#define REAL_MAX  KDT_INFTY_FLOAT
#define REAL_MIN -KDT_INFTY_FLOAT
#define KDT_INFTY KDT_INFTY_FLOAT
#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE_INTEGER 0
#define KDTYPE_MIN -KDT_INFTY
#define KDTYPE_MAX  KDT_INFTY

#define LOW_HR(kd, D, i)  LOW_HR_F(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_F(kd, D, i)

// ????
#define KD_SPLIT(kd, i) KD_SPLIT_I(kd, i)

#include "kdtree_internal.c"

