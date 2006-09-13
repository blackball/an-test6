#include "kdtree.h"

#define REAL float
#define KDTYPE float

typedef float real;
typedef float kdtype;

#define KDT_INFTY KDT_INFTY_FLOAT
#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE_INTEGER 0

#include "kdtree_internal.c"

