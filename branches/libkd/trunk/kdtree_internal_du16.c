#include "kdtree.h"
#include "kdtree_internal_common.h"

#include "kdtree_internal_real_d.h"
#include "kdtree_internal_kdtype_u16.h"

#define REAL2KDTYPE(kd, d, r) DOUBLE2U16(kd, d, r)
#define KDTYPE2REAL(kd, d, t) U16TODOUBLE(kd, d, t)

#include "kdtree_internal.c"

