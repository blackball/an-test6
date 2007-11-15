/*
  This file is part of the Astrometry.net suite.
  Copyright 2007 Dustin Lang.

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

#include "cutest.h"
#include "kdtree.h"
#include "mathutil.h"

double* random_points_d(int N, int D) {
    int i;
    double* data = malloc(N * D * sizeof(double));
    for (i=0; i<(N*D); i++) {
        data[i] = rand() / (double)RAND_MAX;
    }
    return data;
}

void test_nn_bb_ddd(CuTest* tc) {
    double* data;
    int N = 1000;
    int D = 3;
    int Q = 10;
    kdtree_t* kd1;
    kdtree_t* kd2;
    double* datacopy1;
    double* datacopy2;
    double query[D];
    int i, q, d;

    data = random_points_d(N, D);
    datacopy1 = malloc(N * D * sizeof(double));
    memcpy(datacopy1, data, N*D*sizeof(double));
    datacopy2 = malloc(N * D * sizeof(double));
    memcpy(datacopy2, data, N*D*sizeof(double));
    kd1 = kdtree_build(NULL, datacopy1, N, D, 10, KDTT_DOUBLE, KD_BUILD_BBOX);
    kd2 = kdtree_build(NULL, datacopy2, N, D, 10, KDTT_DOUBLE, KD_BUILD_SPLIT);
    CuAssert(tc, "kd1", kd1 != NULL);
    CuAssert(tc, "kd2", kd2 != NULL);

    for (q=0; q<Q; q++) {
        int ind1;
        double query1d2;
        int ind2;
        double query2d2;
        double bestd2;
        int bestind;
        for (d=0; d<D; d++)
            query[d] = rand() / (double)RAND_MAX;

        ind1 = kdtree_nearest_neighbour(kd1, query, &query1d2);
        ind2 = kdtree_nearest_neighbour(kd2, query, &query2d2);

        bestd2 = HUGE_VAL;
        bestind = -1;
        for (i=0; i<N; i++) {
            double d2 = distsq(query, data + i*D, D);
            if (d2 < bestd2) {
                bestind = i;
                bestd2 = d2;
            }
        }

        CuAssertIntEquals(tc, kd2->perm[ind1], bestind);
        CuAssertDblEquals(tc, query1d2, bestd2, 1e-10);

        CuAssertIntEquals(tc, kd2->perm[ind2], bestind);
        CuAssertDblEquals(tc, query2d2, bestd2, 1e-10);
    }

    kdtree_free(kd1);
    free(datacopy1);
    kdtree_free(kd2);
    free(datacopy2);
    free(data);
}

