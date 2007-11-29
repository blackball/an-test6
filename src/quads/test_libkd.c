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

static void run_test_nn(CuTest* tc, int treetype, int treeopts,
                        double eps) {
    double* data;
    int N = 1000;
    int Nleaf = 10;
    int D = 3;
    int Q = 10;
    kdtree_t* kd;
    int datatype;
    int convert = 0;
    double* datacopy;
    double query[D];
    int i, q, d;

    data = random_points_d(N, D);
    datacopy = malloc(N * D * sizeof(double));
    memcpy(datacopy, data, N*D*sizeof(double));

    datatype = treetype & KDT_DATA_MASK;
	if (datatype != KDT_DATA_DOUBLE)
		convert = 1;

    if (convert) {
        kd = kdtree_new(N, D, Nleaf);
        kd = kdtree_convert_data(kd, datacopy, N, D, Nleaf, treetype);
        kd = kdtree_build(kd, kd->data.any, N, D, Nleaf, treetype, treeopts);
    } else {
        kd = kdtree_build(NULL, datacopy, N, D, Nleaf, treetype, treeopts);
    }

    CuAssert(tc, "kd", kd != NULL);
    CuAssertIntEquals(tc, kdtree_check(kd), 0);

    for (q=0; q<Q; q++) {
        int ind;
        double d2;
        double trued2;
        int trueind;
        for (d=0; d<D; d++)
            query[d] = rand() / (double)RAND_MAX;

        ind = kdtree_nearest_neighbour(kd, query, &d2);

        trued2 = HUGE_VAL;
        trueind = -1;
        for (i=0; i<N; i++) {
            double d2 = distsq(query, data + i*D, D);
            if (d2 < trued2) {
                trueind = i;
                trued2 = d2;
            }
        }

        /*
         printf("Naive : ind %i, dist %g.\n", trueind, sqrt(trued2));
         printf("Kdtree: ind %i, dist %g.\n", kd->perm[ind], sqrt(d2));
         */
        CuAssertIntEquals(tc, kd->perm[ind], trueind);

        if (fabs(sqrt(d2) - sqrt(trued2)) >= eps) {
            printf("Naive : %.12g\n", sqrt(trued2));
            printf("Kdtree: %.12g\n", sqrt(d2));
        }
        
        CuAssertDblEquals(tc, sqrt(d2), sqrt(trued2), eps);
    }

    kdtree_free(kd);
    free(datacopy);
    free(data);
}

static void run_test_rs(CuTest* tc, int treetype, int treeopts,
                        double eps) {
    double* data;
    int N = 1000;
    int Nleaf = 10;
    int D = 3;
    int Q = 10;
    double rad2 = 0.01;
    kdtree_t* kd;
    int datatype;
    int convert = 0;
    double* datacopy;
    double query[D];
    int i, q, d;

    srand(0);

    data = random_points_d(N, D);
    datacopy = malloc(N * D * sizeof(double));
    memcpy(datacopy, data, N*D*sizeof(double));

    datatype = treetype & KDT_DATA_MASK;
	if (datatype != KDT_DATA_DOUBLE)
		convert = 1;

    if (convert) {
        kd = kdtree_new(N, D, Nleaf);
        kd = kdtree_convert_data(kd, datacopy, N, D, Nleaf, treetype);
        kd = kdtree_build(kd, kd->data.any, N, D, Nleaf, treetype, treeopts);
    } else {
        kd = kdtree_build(NULL, datacopy, N, D, Nleaf, treetype, treeopts);
    }

    CuAssert(tc, "kd", kd != NULL);
    CuAssertIntEquals(tc, kdtree_check(kd), 0);

    for (q=0; q<Q; q++) {
        int ind;
        double d2;
        double trued2;
        int ntrue;
        kdtree_qres_t* res;

        for (d=0; d<D; d++)
            query[d] = rand() / (double)RAND_MAX;

        res = kdtree_rangesearch(kd, query, rad2);

        ntrue = 0;
        printf("In range: ");
        for (i=0; i<N; i++) {
            double d2 = distsq(query, data + i*D, D);
            if (d2 <= rad2) {
                printf("%i ", i);
                ntrue++;
            }
        }
        printf("\n");

        CuAssertIntEquals(tc, res->nres, ntrue);

        for (i=0; i<res->nres; i++) {
            ind = res->inds[i];
            d2 = res->sdists[i];
            trued2 = distsq(query, data + ind*D, D);
            printf("ind %i\n", ind);
            printf("reported dist2 %g, trued2 %g\n", d2, trued2);
        }

        CuAssert(tc, "res", res != NULL);
        for (i=0; i<res->nres; i++) {
            ind = res->inds[i];
            d2 = res->sdists[i];
            trued2 = distsq(query, data + ind*D, D);

            //printf("ind %i (perm %i)\n", ind, kd->perm[ind]);

            CuAssert(tc, "ind pos", ind >= 0);
            CuAssert(tc, "ind pos", ind < N);
            CuAssert(tc, "inrange", d2 <= rad2);
            CuAssert(tc, "inrange", trued2 <= rad2);
            CuAssert(tc, "d2pos", d2 >= 0.0);
            CuAssert(tc, "trued2pos", trued2 >= 0.0);
            CuAssertDblEquals(tc, sqrt(d2), sqrt(trued2), eps);
        }
        /*
         printf("Naive : ind %i, dist %g.\n", trueind, sqrt(trued2));
         printf("Kdtree: ind %i, dist %g.\n", kd->perm[ind], sqrt(d2));
         */
    }

    kdtree_free(kd);
    free(datacopy);
    free(data);
}

void test_rs_bb_duu(CuTest* tc) {
    printf("\n\n\n====================================\n");
    run_test_rs(tc, KDTT_DUU, KD_BUILD_BBOX, 1e-9);
    printf("====================================\n\n");
}

void test_drawsplit(CuTest* tc) {
    printf("------------------------------------\n\n");
}

void test_rs_bb_ddd(CuTest* tc) {
    run_test_rs(tc, KDTT_DOUBLE, KD_BUILD_BBOX, 1e-9);
}
void test_rs_split_ddd(CuTest* tc) {
    run_test_rs(tc, KDTT_DOUBLE, KD_BUILD_SPLIT, 1e-9);
}
void test_rs_both_ddd(CuTest* tc) {
    run_test_rs(tc, KDTT_DOUBLE, KD_BUILD_BBOX | KD_BUILD_SPLIT, 1e-9);
}

void test_rs_split_duu(CuTest* tc) {
    run_test_rs(tc, KDTT_DUU, KD_BUILD_SPLIT, 1e-9);
}

void test_rs_bb_dss(CuTest* tc) {
    run_test_rs(tc, KDTT_DSS, KD_BUILD_BBOX, 1e-5);
}
void test_rs_split_dss(CuTest* tc) {
    run_test_rs(tc, KDTT_DSS, KD_BUILD_SPLIT, 1e-5);
}




void test_nn_bb_ddd(CuTest* tc) {
    run_test_nn(tc, KDTT_DOUBLE, KD_BUILD_BBOX, 1e-9);
}

void test_nn_split_ddd(CuTest* tc) {
    run_test_nn(tc, KDTT_DOUBLE, KD_BUILD_SPLIT, 1e-9);
}

void test_nn_both_ddd(CuTest* tc) {
    run_test_nn(tc, KDTT_DOUBLE, KD_BUILD_SPLIT | KD_BUILD_BBOX, 1e-9);
}

void test_nn_split_duu(CuTest* tc) {
    run_test_nn(tc, KDTT_DUU, KD_BUILD_SPLIT, 1e-9);
}

void test_nn_bb_duu(CuTest* tc) {
    run_test_nn(tc, KDTT_DUU, KD_BUILD_BBOX, 1e-9);
}

void test_nn_split_dss(CuTest* tc) {
    run_test_nn(tc, KDTT_DSS, KD_BUILD_SPLIT, 1e-5);
}

void test_nn_bb_dss(CuTest* tc) {
    run_test_nn(tc, KDTT_DSS, KD_BUILD_BBOX, 1e-5);
}


