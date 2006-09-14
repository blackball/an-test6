#include "kdtree.h"
#include "kdtree_internal_common.h"

#include "kdtree_internal_real_u16.h"
#include "kdtree_internal_kdtype_u16.h"

#define REAL2KDTYPE(kd, d, r) (r)
#define KDTYPE2REAL(kd, d, t) (t)
#define REAL2KDTYPE_FLOOR(kd, d, r) (r)
#define REAL2KDTYPE_CEIL( kd, d, r) (r)

#include "kdtree_internal.c"

