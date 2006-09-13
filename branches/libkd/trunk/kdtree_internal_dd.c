#include "kdtree.h"

#define REAL double
#define KDTYPE double

typedef double real;
typedef double kdtype;

#define KDT_INFTY KDT_INFTY_DOUBLE
#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE_INTEGER 0

#include "kdtree_internal.c"

