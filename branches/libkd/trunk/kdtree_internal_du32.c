#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL double
#define KDTYPE u32

typedef double real;
typedef u32 kdtype;

#define KDT_INFTY KDT_INFTY_DOUBLE
#define REAL2KDTYPE(kd, d, r) DOUBLE2U32(kd, d, r)
#define KDTYPE_INTEGER 1
//#define KDTYPE_MAX 4294967295.0
#define KDTYPE_MAX 0xffffffff

#define LOW_HR(kd, D, i)  LOW_HR_I(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_I(kd, D, i)

#include "kdtree_internal.c"

