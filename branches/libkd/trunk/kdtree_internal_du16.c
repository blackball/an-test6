#include "kdtree.h"

#define REAL double
#define KDTYPE u16

typedef double real;
typedef u16 kdtype;

#define KDT_INFTY KDT_INFTY_DOUBLE
#define REAL2KDTYPE(kd, d, r) DOUBLE2U16(kd, d, r)
#define KDTYPE_INTEGER 1
#define KDTYPE_MAX 0xffff

#include "kdtree_internal.c"

