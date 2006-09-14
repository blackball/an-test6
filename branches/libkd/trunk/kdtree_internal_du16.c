#include "kdtree.h"
#include "kdtree_internal_common.h"

#include "kdtree_internal_real_d.h"
#include "kdtree_internal_kdtype_u16.h"

#define REAL2KDTYPE(kd, d, r) DOUBLE2U16(kd, d, r)
#define KDTYPE2REAL(kd, d, t) U16TODOUBLE(kd, d, t)
#define REAL2KDTYPE_FLOOR(kd, d, r) DOUBLE2U16_FLOOR(kd, d, r)
#define REAL2KDTYPE_CEIL( kd, d, r) DOUBLE2U16_CEIL( kd, d, r)

#include "kdtree_internal.c"

