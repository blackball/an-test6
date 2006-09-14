#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL u16
#define KDTYPE u16

typedef u16 real;
typedef u16 kdtype;

#define REAL_MAX 0xffffu
#define REAL_MIN 0

#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE2REAL(kd, d, t) (t)

#define KDTYPE_INTEGER 1
#define KDTYPE_MIN 0
#define KDTYPE_MAX 0xffffu

#define LOW_HR(kd, D, i)  LOW_HR_S(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_S(kd, D, i)

#define KD_SPLIT(kd, i) KD_SPLIT_S(kd, i)

#include "kdtree_internal.c"

