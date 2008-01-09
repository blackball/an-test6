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

static void assert_kdtrees_equal(CuTest* ct, const kdtree_t* kd, const kdtree_t* kd2) {
    double del = 1e-10;
    size_t sz, sz2;

    if (!kd) {
        CuAssertPtrEquals(ct, NULL, (kdtree_t*)kd2);
        return;
    }
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

    if (kd->lr) {
        CuAssertPtrNotNull(ct, kd2->lr);
        sz  = kdtree_sizeof_lr(kd );
        sz2 = kdtree_sizeof_lr(kd2);
        CuAssertIntEquals(ct, sz, sz2);
        CuAssert(ct, "lr equal", memcmp(kd->lr, kd2->lr, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->lr);
    }

    if (kd->perm) {
        CuAssertPtrNotNull(ct, kd2->perm);
        sz  = kdtree_sizeof_perm(kd );
        sz2 = kdtree_sizeof_perm(kd2);
        CuAssertIntEquals(ct, sz, sz2);
        CuAssert(ct, "perm equal", memcmp(kd->perm, kd2->perm, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->perm);
    }

    if (kd->data.any) {
        CuAssertPtrNotNull(ct, kd2->data.any);
        sz  = kdtree_sizeof_data(kd );
        sz2 = kdtree_sizeof_data(kd2);
        CuAssertIntEquals(ct, sz, sz2);
        CuAssert(ct, "data equal", memcmp(kd->data.any, kd2->data.any, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->data.any);
    }

    if (kd->splitdim) {
        CuAssertPtrNotNull(ct, kd2->splitdim);
        sz  = kdtree_sizeof_splitdim(kd );
        sz2 = kdtree_sizeof_splitdim(kd2);
        CuAssertIntEquals(ct, sz, sz2);
        CuAssert(ct, "splitdim equal", memcmp(kd->splitdim, kd2->splitdim, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->splitdim);
    }

    if (kd->split.any) {
        CuAssertPtrNotNull(ct, kd2->split.any);
        sz  = kdtree_sizeof_split(kd );
        sz2 = kdtree_sizeof_split(kd2);
        CuAssertIntEquals(ct, sz, sz2);
        CuAssert(ct, "split equal", memcmp(kd->split.any, kd2->split.any, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->split.any);
    }

    if (kd->bb.any) {
        CuAssertPtrNotNull(ct, kd2->bb.any);
        sz  = kdtree_sizeof_bb(kd );
        sz2 = kdtree_sizeof_bb(kd2);
        CuAssertIntEquals(ct, sz, sz2);
        CuAssert(ct, "bb equal", memcmp(kd->bb.any, kd2->bb.any, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->bb.any);
    }

    if (kd->nodes) {
        CuAssertPtrNotNull(ct, kd2->nodes);
        sz  = kdtree_sizeof_nodes(kd );
        sz2 = kdtree_sizeof_nodes(kd2);
        CuAssertIntEquals(ct, sz, sz2);
        CuAssert(ct, "nodes equal", memcmp(kd->nodes, kd2->nodes, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->nodes);
    }

    if (kd->minval) {
        sz  = kd->ndim * sizeof(double);
        CuAssertPtrNotNull(ct, kd2->minval);
        CuAssert(ct, "minval equal", memcmp(kd->minval, kd2->minval, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->minval);
    }

    if (kd->maxval) {
        sz  = kd->ndim * sizeof(double);
        CuAssertPtrNotNull(ct, kd2->maxval);
        CuAssert(ct, "maxval equal", memcmp(kd->maxval, kd2->maxval, sz) == 0);
    } else {
        CuAssertPtrEquals(ct, NULL, kd2->maxval);
    }
    

}

void test_read_write_single_tree_unnamed(CuTest* ct) {
    kdtree_t* kd;
    double * data;
    int N = 1000;
    int Nleaf = 5;
    int D = 3;
    char fn[1024];
    int rtn;
    kdtree_t* kd2;
    int fd;

    data = random_points_d(N, D);
    kd = build_tree(ct, data, N, D, Nleaf, KDTT_DOUBLE, KD_BUILD_SPLIT);
    kd->name = NULL;

    sprintf(fn, "/tmp/test_libkd_io_single_tree_unnamed.XXXXXX");
    fd = mkstemp(fn);
    if (fd == -1) {
        fprintf(stderr, "Failed to generate a temp filename: %s\n", strerror(errno));
        CuFail(ct, "mkstemp");
    }
    close(fd);
    printf("Single tree unnamed: writing to file %s.\n", fn);

    rtn = kdtree_fits_write(kd, fn, NULL);
    CuAssertIntEquals(ct, 0, rtn);

    kd2 = kdtree_fits_read(fn, NULL, NULL);

    assert_kdtrees_equal(ct, kd, kd2);

    free(data);
    kdtree_free(kd);
    kdtree_fits_close(kd2);
}



