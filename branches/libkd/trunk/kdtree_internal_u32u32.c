#include "kdtree.h"
#include "kdtree_internal_common.h"

#define REAL u32
#define KDTYPE u32

typedef u32 real;
typedef u32 kdtype;

#define REAL_MAX 0xffffffffu
#define REAL_MIN 0

#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE2REAL(kd, d, t) (t)

#define KDTYPE_INTEGER 1
#define KDTYPE_MIN 0
#define KDTYPE_MAX 0xffffffffu

#define LOW_HR(kd, D, i)  LOW_HR_I(kd, D, i)
#define HIGH_HR(kd, D, i) HIGH_HR_I(kd, D, i)

#define KD_SPLIT(kd, i) KD_SPLIT_I(kd, i)

#include "kdtree_internal.c"

