#include "kdtree.h"

#define REAL double
#define KDTYPE u32

typedef double real;
typedef u32 kdtype;

#define KDT_INFTY KDT_INFTY_DOUBLE
#define REAL2KDTYPE(kd, d, r) DOUBLE2U32(kd, d, r)
#define KDTYPE_INTEGER 1
//#define KDTYPE_MAX 4294967295.0
#define KDTYPE_MAX 0xffffffff

#include "kdtree_internal.c"

