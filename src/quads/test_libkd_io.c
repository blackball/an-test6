/*
  This file is part of the Astrometry.net suite.
  Copyright 2008 Dustin Lang.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <strings.h>
#include <errno.h>

#include "cutest.h"
#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "mathutil.h"
#include "fls.h"

#include "test_libkd_common.c"

void test_read_write_single_tree_unnamed(CuTest* ct) {
    kdtree_t* kd;
    double * data;
    int N = 1000;
    int Nleaf = 5;
    int D = 3;
    char fn[1024];
    int rtn;
    kdtree_t* kd2;
    size_t sz, sz2;
    double del = 1e-10;
    int fd;

    data = random_points_d(N, D);
    kd = build_tree(ct, data, N, D, Nleaf, KDTT_DOUBLE, KD_BUILD_SPLIT);
    kd->name = NULL;

    sprintf(fn, "/tmp/test_libkd_io_single_tree_unnamed.XXXXXX");
    fd = mkstemp(fn);
    if (fd == -1) {
        fprintf(stderr, "Failed to generate a temp filename: %s\n", strerror(errno));
        CuAssert(ct, "mkstemp", 0);
    }
    close(fd);
    printf("Single tree unnamed: writing to file %s.\n", fn);

    rtn = kdtree_fits_write(kd, fn, NULL);
    CuAssertIntEquals(ct, 0, rtn);

    kd2 = kdtree_fits_read(fn, NULL, NULL);
    CuAssertPtrNotNull(ct, kd2);

    CuAssertIntEquals(ct, kd->treetype, kd2->treetype);
    CuAssertIntEquals(ct, kd->dimbits, kd2->dimbits);
    CuAssertIntEquals(ct, kd->dimmask, kd2->dimmask);
    CuAssertIntEquals(ct, kd->splitmask, kd2->splitmask);

    CuAssertIntEquals(ct, kd->ndata, kd2->ndata);
    CuAssertIntEquals(ct, kd->ndim, kd2->ndim);
    CuAssertIntEquals(ct, kd->nnodes, kd2->nnodes);
    CuAssertIntEquals(ct, kd->nbottom, kd2->nbottom);
    CuAssertIntEquals(ct, kd->ninterior, kd2->ninterior);
    CuAssertIntEquals(ct, kd->nlevels, kd2->nlevels);

    CuAssertIntEquals(ct, kd->has_linear_lr, kd2->has_linear_lr);
    CuAssertIntEquals(ct, kd->converted_data, kd2->converted_data);

    CuAssertDblEquals(ct, kd->scale,    kd2->scale,    del);
    CuAssertDblEquals(ct, kd->invscale, kd2->invscale, del);

    sz  = kdtree_sizeof_lr(kd );
    sz2 = kdtree_sizeof_lr(kd2);
    CuAssertIntEquals(ct, sz, sz2);
    CuAssertPtrNotNull(ct, kd ->lr);
    CuAssertPtrNotNull(ct, kd2->lr);
    CuAssert(ct, "lr equal", memcmp(kd->lr, kd2->lr, sz) == 0);

    sz  = kdtree_sizeof_perm(kd );
    sz2 = kdtree_sizeof_perm(kd2);
    CuAssertIntEquals(ct, sz, sz2);
    CuAssertPtrNotNull(ct, kd ->perm);
    CuAssertPtrNotNull(ct, kd2->perm);
    CuAssert(ct, "perm equal", memcmp(kd->perm, kd2->perm, sz) == 0);

    sz  = kdtree_sizeof_data(kd );
    sz2 = kdtree_sizeof_data(kd2);
    CuAssertIntEquals(ct, sz, sz2);
    CuAssertPtrNotNull(ct, kd->data.any);
    CuAssertPtrNotNull(ct, kd2->data.any);
    CuAssert(ct, "data equal", memcmp(kd->data.any, kd2->data.any, sz) == 0);

    sz  = kdtree_sizeof_split(kd );
    sz2 = kdtree_sizeof_split(kd2);
    CuAssertIntEquals(ct, sz, sz2);
    CuAssertPtrNotNull(ct, kd->split.any);
    CuAssertPtrNotNull(ct, kd2->split.any);
    CuAssert(ct, "split equal", memcmp(kd->split.any, kd2->split.any, sz) == 0);

    sz  = kdtree_sizeof_splitdim(kd );
    sz2 = kdtree_sizeof_splitdim(kd2);
    CuAssertIntEquals(ct, sz, sz2);
    CuAssertPtrNotNull(ct, kd->splitdim);
    CuAssertPtrNotNull(ct, kd2->splitdim);
    CuAssert(ct, "splitdim equal", memcmp(kd->splitdim, kd2->splitdim, sz) == 0);

    CuAssertPtrEquals(ct, NULL, kd->bb.any);
    CuAssertPtrEquals(ct, NULL, kd2->bb.any);

    CuAssertPtrEquals(ct, NULL, kd->minval);
    CuAssertPtrEquals(ct, NULL, kd2->minval);

    CuAssertPtrEquals(ct, NULL, kd->maxval);
    CuAssertPtrEquals(ct, NULL, kd2->maxval);

    CuAssertPtrEquals(ct, NULL, kd->nodes);
    CuAssertPtrEquals(ct, NULL, kd2->nodes);

}



