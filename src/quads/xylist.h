#ifndef XYLIST_H
#define XYLIST_H

#include <stdio.h>
#include "starutil.h"
#include "bl.h"

typedef dl xy;
#define mk_xy(n) dl_new((n)*2)
#define free_xy(x) dl_free(x)
#define xy_ref(x, i) dl_get(x, i)
#define xy_refx(x, i) dl_get(x, 2*(i))
#define xy_refy(x, i) dl_get(x, (2*(i))+1)
#define xy_size(x) (dl_size(x)/2)
#define xy_set(x,i,v) dl_set(x,i,v)
#define xy_setx(x,i,v) dl_set(x,2*(i),v)
#define xy_sety(x,i,v) dl_set(x,2*(i)+1,v)

typedef pl xyarray;
#define mk_xyarray(n)         pl_new(n)
#define free_xyarray(l)       pl_free(l)
#define xya_ref(l, i)    (xy*)pl_get((l), (i))
#define xya_set(l, i, v)      pl_set((l), (i), (v))
#define xya_size(l)           pl_size(l)

xyarray *readxy(FILE *fid, char ParityFlip);

#endif
