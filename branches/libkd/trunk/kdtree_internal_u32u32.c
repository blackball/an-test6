#include "kdtree.h"
#include "kdtree_internal_common.h"

#include "kdtree_internal_real_u32.h"
#include "kdtree_internal_kdtype_u32.h"

#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE2REAL(kd, d, t) (t)

#include "kdtree_internal.c"

