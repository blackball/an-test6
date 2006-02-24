#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "kdtree.h"

/*****************************************************************************/
/* Utility macros                                                            */
/*****************************************************************************/
#define KDTREE_MAX_DIM 10
#define KDTREE_MAX_RESULTS 1000

/* Most macros operate on a variable kdtree_t *kd, assumed to exist. */
/* x is a node index, d is a dimension, and n is a point index */
#define NODE_SIZE      (sizeof(kdtree_node_t) + sizeof(real)*kd->ndim*2)

#define LOW_HR(x, d)   ((real*) (((void*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*(d)))
#define HIGH_HR(x, d)  ((real*) (((void*)kd->tree) + NODE_SIZE*(x) \
                                 + sizeof(kdtree_node_t) \
                                 + sizeof(real)*((d) \
                                 + kd->ndim)))
#define NODE(x)        ((kdtree_node_t*) (((void*)kd->tree) + NODE_SIZE*(x)))
#define PARENT(x)      NODE(((x)-1)/2)
#define CHILD_NEG(x)   NODE(2*(x) + 1)
#define CHILD_POS(x)   NODE(2*(x) + 2)
#define ISLEAF(x)      ((2*(x)+1) >= kd->nnodes)
#define COORD(n,d)     ((real*)(kd->data + kd->ndim*(n) + (d)))


/*****************************************************************************/
/* Building routines                                                         */
/*****************************************************************************/
int kdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d)
{
    int beg[KDTREE_MAX_LEVELS], end[KDTREE_MAX_LEVELS], i=0, j, L, R;
    static real piv_vec[KDTREE_MAX_DIM];
    unsigned int piv_perm;
    real piv;

    beg[0] = l; end[0] = r;
    while (i >= 0) {
        L = beg[i];
        R = end[i];
        if (L < R) {
            piv = arr[D*L+d];
            for(j=0;j<D;j++)
                piv_vec[j] = arr[D*L+j];
            piv_perm = parr[L];
            if (i == KDTREE_MAX_LEVELS-1) /* Sanity */
                assert(0);
            while (L < R) {
                while (arr[D*R+d] >= piv && L < R) R--;
                if (L < R) {
                    for(j=0;j<D;j++) 
                        arr[D*L + j] = arr[D*R + j];
                    parr[L] = parr[R];
                    L++;
                }
                while (arr[D*L+d] <= piv && L < R) L++;
                if (L < R) {
                    for(j=0;j<D;j++) 
                        arr[D*R + j] = arr[D*L + j];
                    parr[R] = parr[L];
                    R--;
                }
            }
            for(j=0;j<D;j++)
                arr[D*L+j] = piv_vec[j];
            parr[L] = piv_perm;
            beg[i+1] = L+1;
            end[i+1] = end[i];
            end[i++] = L;
        } else 
            i--;
    }
    return 1;
}

/* If the root node is level 0, then maxlevel is the level at which there may
 * not be enough points to keep the tree complete (i.e. last level) */
kdtree_t *kdtree_build(real *data, int N, int D, int maxlevel) 
{
	int i;
	kdtree_t *kd; 
	int nnodes; 
	int level = 0, dim, t, m;
	real pivot;

    /* Parameters checking */
	if (!data || !N || !D)
        return NULL;
	/* Make sure we have enough data */
	if ((1<<maxlevel) - 1 > N)
        return NULL;

    /* Set the tree fields */
	kd = malloc(sizeof(kdtree_t));
	nnodes = (1<<maxlevel) - 1;
	kd->ndata = N;
	kd->ndim = D;
	kd->nnodes = nnodes;
	kd->data = data;
    /* perm stores the permutation indexes. This gets shuffeled around during
     * sorts to keep track of the original index. */
	kd->perm = malloc(sizeof(unsigned int)*N);
    for (i=0;i<N;i++)
        kd->perm[i] = i;
	assert(kd->perm);
	kd->tree = malloc(NODE_SIZE*(nnodes));
	assert(kd->tree);

	//printf("n=%d;d=%d;maxlvl=%d;nnodes=%d\n",N,D,maxlevel,nnodes);

	/* Root node owns the infinite hyperrectangle  and all data points */
	for(i=0;i<D;i++)
		*LOW_HR(0, i) = -KDT_INFTY;
	for(i=0;i<D;i++)
		*HIGH_HR(0, i) = KDT_INFTY;
	NODE(0)->l = 0;
	NODE(0)->r = N-1;

	/* And in one shot, make the kdtree. Each iteration we set our
	 * children's hyperrectangles and [l,r] array indexes and sort our own
	 * subset. */
	for (i=0; i<nnodes; i++) {

        /* Sanity */
        assert(NODE(i) == PARENT(2*i+1));
        assert(NODE(i) == PARENT(2*i+2));
        if (i && i % 2)
            assert(NODE(i) == CHILD_NEG((i-1)/2));
        else if (i) 
            assert(NODE(i) == CHILD_POS((i-1)/2));

		/* Calculate log2(i) inefficiently */
		level = 0; t = i+1;
		while(t>>=1) level++;
		//printf("   l=%d; i=%d\n",level, i);
		
		/* Find split dimension and sort on it */
		dim = NODE(i)->dim = level % D;
		kdtree_qsort(data, kd->perm, NODE(i)->l, NODE(i)->r, D, dim);
		m = (NODE(i)->r - NODE(i)->l)/2 + NODE(i)->l + 1;
		pivot = NODE(i)->pivot = *COORD(m, dim);

		/* Only do child operations if we're not the last layer */
		if (level < maxlevel-1) {
            assert(2*i+1 < nnodes);
            assert(2*i+2 < nnodes);
			CHILD_NEG(i)->l = NODE(i)->l;
			CHILD_NEG(i)->r = m-1;
			CHILD_POS(i)->l = m;
			CHILD_POS(i)->r	= NODE(i)->r;
            //printf("       children=(%d, %d)\n", 2*i+1,2*i+2);
            /* Split our hyperrectangle and pass the sides to our children.
             * The memcpy copies both high/low coords to the children. */
			memcpy(LOW_HR(2*i+1,0), LOW_HR(i,0), sizeof(real)*2*D);
			*HIGH_HR(2*i+1, dim) = pivot;
			memcpy(LOW_HR(2*i+2,0), LOW_HR(i,0), sizeof(real)*2*D);
			*LOW_HR(2*i+2, dim) = pivot;
		}
	}
	return kd;
}

/*****************************************************************************/
/* Querying routines                                                         */
/*****************************************************************************/

real results[KDTREE_MAX_RESULTS];
real results_sqd[KDTREE_MAX_RESULTS];
unsigned int results_inds[KDTREE_MAX_RESULTS];
//unsigned int results_inds2[KDTREE_MAX_RESULTS];

// DEBUG
int overflow;

/* Range seach helper */
void kdtree_rangesearch_actual(kdtree_t *kd, int node, real *pt, real maxdistsqd, kdtree_qres_t *res)
{
    //printf("node=%d\n",node);
    
    real smallest_dsqd = 0, delta;
    int i, j;

    for ( i=0; i<kd->ndim; i++ ) {
        real alo = *LOW_HR(node,i);
        real ahi = *HIGH_HR(node,i);
        real b = pt[i];
        if (ahi < b)
            delta = b - ahi;
        else if (b < alo)
            delta = alo - b;
        else
            delta = 0.0;
        smallest_dsqd += delta * delta;
        /* Early exit - FIXME benchmark to see if this actually helps */
        if (smallest_dsqd > maxdistsqd) {
            return;
        }
    }

    /*printf("smallest=%lf\n",smallest_dsqd);*/
    if (smallest_dsqd < maxdistsqd) {
        if (ISLEAF(node)) {
            for (i=NODE(node)->l; i<=NODE(node)->r; i++) {
                real dsqd = 0.0;
                for (j=0; j<kd->ndim; j++) {
                    delta = (pt[j]) - (*COORD(i,j));
                    dsqd += delta*delta;
                    /*printf("cd(%d,%d)=%lf; delta=%lf; dsqd=%lf;\n",i,j,*COORD(i,j),delta,dsqd); */
                }
                if (dsqd < maxdistsqd) {
                    /*printf("BINGO\n");*/
                    results_sqd[res->nres] = dsqd;
					// DEBUG
                    //results_inds2[res->nres] = i;
                    results_inds[res->nres] = kd->perm[i];
                    memcpy(results+res->nres*kd->ndim, COORD(i,0), sizeof(real)*kd->ndim);
                    res->nres++;
					// DEBUG
					if (res->nres >= KDTREE_MAX_RESULTS) {
						fprintf(stderr, "\nkdtree rangesearch overflow.\n");
						overflow = 1;
						break;
					}
                }
            }
        } else {
            kdtree_rangesearch_actual(kd, 2*node+1, pt, maxdistsqd, res);
			if (overflow) return;
            kdtree_rangesearch_actual(kd, 2*node+2, pt, maxdistsqd, res);
        }
    } 
}

/* Sorts results by kq->sdists */
int kdtree_qsort_results(kdtree_qres_t *kq, int D)
{
    int beg[KDTREE_MAX_RESULTS], end[KDTREE_MAX_RESULTS], i=0, j, L, R;
    static real piv_vec[KDTREE_MAX_DIM];
    unsigned int piv_perm;
    real piv;

    beg[0] = 0; end[0] = kq->nres-1;
    while (i >= 0) {
        L = beg[i];
        R = end[i];
        if (L < R) {
            piv = kq->sdists[L];
            for(j=0;j<D;j++)
                piv_vec[j] = kq->results[D*L+j];
            piv_perm = kq->inds[L];
            if (i == KDTREE_MAX_RESULTS-1) /* Sanity */
                assert(0);
            while (L < R) {
                while (kq->sdists[R] >= piv && L < R) R--;
                if (L < R) {
                    for(j=0;j<D;j++) 
                        kq->results[D*L + j] = kq->results[D*R + j];
                    kq->inds[L] = kq->inds[R];
                    kq->sdists[L] = kq->sdists[R];
                    L++;
                }
                while (kq->sdists[L] <= piv && L < R) L++;
                if (L < R) {
                    for(j=0;j<D;j++) 
                        kq->results[D*R + j] = kq->results[D*L + j];
                    kq->inds[R] = kq->inds[L];
                    kq->sdists[R] = kq->sdists[L];
                    R--;
                }
            }
            for(j=0;j<D;j++)
                kq->results[D*L+j] = piv_vec[j];
            kq->inds[L] = piv_perm;
            kq->sdists[L] = piv;
            beg[i+1] = L+1;
            end[i+1] = end[i];
            end[i++] = L;
        } else 
            i--;
    }
    return 1;
}

/* Range seach */
kdtree_qres_t *kdtree_rangesearch(kdtree_t *kd, real *pt, real maxdistsquared)
{
    kdtree_qres_t *res; 
    if (!kd || !pt)
        return NULL;
    res = malloc(sizeof(kdtree_qres_t));
    res->nres = 0;
	overflow = 0;

    /* Do the real rangesearch */
    kdtree_rangesearch_actual(kd, 0, pt, maxdistsquared, res);

    /* Store actual coordinates of results */
    res->results = malloc(sizeof(real)*kd->ndim*res->nres);
    memcpy(res->results, results, sizeof(real)*kd->ndim*res->nres);

    /* Store squared distances of results */
    res->sdists = malloc(sizeof(real)*res->nres);
    memcpy(res->sdists, results_sqd, sizeof(real)*res->nres);

    /* Store indexes of results */
    res->inds = malloc(sizeof(unsigned int)*res->nres);
    memcpy(res->inds, results_inds, sizeof(unsigned int)*res->nres);

    /* Sort by ascending distance away from target point before returning */
    kdtree_qsort_results(res, kd->ndim);

    return res;
}

/* Optimize the KDTree by by constricting hyperrectangles to minimum volume */
void kdtree_optimize(kdtree_t *kd)
{
    real high[KDTREE_MAX_DIM];
    real low[KDTREE_MAX_DIM];
    int i, j, k;

    for ( i=0; i<kd->nnodes; i++ ) {
        for (k=0; k<kd->ndim; k++) {
            high[k] = -KDT_INFTY;
            low[k] = KDT_INFTY;
        }
        for (j=NODE(i)->l; j<=NODE(i)->r; j++) {
            for (k=0; k<kd->ndim; k++) {
                if (high[k] < *COORD(j,k))
                    high[k] = *COORD(j,k);
                if (low[k] > *COORD(j,k))
                    low[k] = *COORD(j,k);
            }
        }
        for (k=0; k<kd->ndim; k++) {
            *LOW_HR(i,k) = low[k];
            *HIGH_HR(i,k) = high[k];
        }
    }
}

void kdtree_free_query(kdtree_qres_t *kq)
{
    assert(kq);
    assert(kq->results);
    assert(kq->sdists);
    assert(kq->inds);
    free(kq->results);
    free(kq->sdists);
    free(kq->inds);
    free(kq);
}

void kdtree_free(kdtree_t *kd)
{
    assert(kd);
    assert(kd->tree);
    assert(kd->perm);
    /* We don't free kd->data */
    free(kd->perm);
    free(kd->tree);
    free(kd);
}

/*****************************************************************************/
/* Output routines                                                           */
/*****************************************************************************/

/* Output a graphviz style description of the tree, for input to dot program */
void kdtree_output_dot(FILE* fid, kdtree_t* kd)
{
	int i, j, D = kd->ndim;
    fprintf(fid, "digraph {\nnode [shape = record];\n");

	for (i=0; i<kd->nnodes; i++) {
        fprintf(fid, "node%d [ label =\"<f0> %d | <f1> (%d) D%d p=%.2lf \\n L=(",
                i, NODE(i)->l, i, NODE(i)->dim, NODE(i)->pivot);
        for (j=0; j<D; j++) {
            fprintf(fid, "%.2lf", *LOW_HR(i, j));
            if (j<D-1)
                fprintf(fid,",");
        }
        fprintf(fid, ") \\n H=(");
        for (j=0; j<D; j++) {
            fprintf(fid, "%.2lf", *HIGH_HR(i, j));
            if (j<D-1)
                fprintf(fid,",");
        }
        fprintf(fid, ") | <f2> %d \"];\n", 
                NODE(i)->r);
        if ((2*i+2) < kd->nnodes) {
            fprintf(fid, "\"node%d\":f2 -> \"node%d\":f1;\n" , i, 2*i+2);
        }
        if ((2*i+1) < kd->nnodes) {
            fprintf(fid, "\"node%d\":f0 -> \"node%d\":f1;\n" , i, 2*i+1);
        }
    }

    fprintf(fid, "}\n");
}
