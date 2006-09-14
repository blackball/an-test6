#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL double
#define KDTYPE u16

typedef double real;
typedef u16 kdtype;

#define REAL_MAX  KDT_INFTY_DOUBLE
#define REAL_MIN -KDT_INFTY_DOUBLE
#define KDT_INFTY KDT_INFTY_DOUBLE
#define REAL2KDTYPE(kd, d, r) DOUBLE2U16(kd, d, r)
#define KDTYPE2REAL(kd, d, t) U16TODOUBLE(kd, d, t)

#define KDTYPE_INTEGER 1
#define KDTYPE_MIN 0
#define KDTYPE_MAX 0xffff

#define LOW_HR(kd, D, i)  LOW_HR_S(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_S(kd, D, i)

#define KD_SPLIT(kd, i) KD_SPLIT_S(kd, i)

#include "kdtree_internal.c"

