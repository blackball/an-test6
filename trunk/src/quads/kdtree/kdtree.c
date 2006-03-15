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

#ifdef AMNSLOW

real* kdqsort_arr;
int kdqsort_D;

int kdqsort_compare(const void* v1, const void* v2) {
	int i1, i2;
	real val1, val2;
	i1 = *((int*)v1);
	i2 = *((int*)v2);
	val1 = kdqsort_arr[i1*kdqsort_D];
	val2 = kdqsort_arr[i2*kdqsort_D];
	if (val1 < val2)
		return -1;
	else if (val1 > val2)
		return 1;
	return 0;
}

int kdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d) {
	int* permute;
	int i, j, N;
	real* tmparr;
	int* tmpparr;

	N = r - l + 1;
	permute = malloc(N * sizeof(int));
	for (i=0; i<N; i++)
		permute[i] = i;
	kdqsort_arr = arr + l*D + d;
	kdqsort_D = D;

	qsort(permute, N, sizeof(int), kdqsort_compare);

	tmparr = malloc(N*D * sizeof(real));
	tmpparr = malloc(N * sizeof(int));
	for (i=0; i<N; i++) {
		int pi = permute[i];
		for (j=0; j<D; j++) {
			tmparr[i*D + j] = arr[(l + pi)*D + j];
		}
		tmpparr[i] = parr[l + pi];
	}
	memcpy(arr + l*D, tmparr, N*D*sizeof(real));
	memcpy(parr + l, tmpparr, N*sizeof(int));
	free(tmparr);
	free(tmpparr);
	free(permute);
	return 1;
}

#else

int kdtree_dstn_partition(real *arr, unsigned int *parr, int l, int r,
						  int D, int d, real lower, real upper) {
	// we need to find the median and partition the data
	int BINS = 100;
	int counts[BINS];
	int i;
	real binsize;
	int sum;

	real* data;
	real median;

	int N;
	int m;
	int Nleft, Nmiddle;
	real newL, newU;
	int rank;

	int il, ir;
	int midbin;

	assert(r >= l);
	for (i=l; i<=r; i++) {
		assert(arr[i*D+d] >= lower);
		assert(arr[i*D+d] <= upper);
	}

	N = r - l + 1;
	rank = (N-1)/2;

	data = malloc(N*sizeof(real));
	for (i=0; i<N; i++)
		data[i] = arr[(l+i)*D + d];

	while ((upper != lower) && (N>2)) {
		real* newdata;
		int nm;

		assert(rank >= 0);

		// compute the new actual lower and upper bounds.
		// HACK - check if it's worthwhile computing this.
		// It helps ensure termination, but requires another pass through the data...
		if (N < 20) {
			newL = upper;
			newU = lower;
			for (i=0; i<N; i++) {
				if (data[i] < newL) newL = data[i];
				if (data[i] > newU) newU = data[i];
			}
			lower = newL;
			upper = newU;
			if (upper == lower)
				break;
		}

		// histogram the data.
		binsize = (upper - lower) / ((double)BINS - 0.1);
		for (i=0; i<BINS; i++)
			counts[i] = 0;
		for (i=0; i<N; i++) {
			int bin = (data[i] - lower) / binsize;
			assert(bin >= 0);
			assert(bin < BINS);
			counts[bin]++;
		}

		// "midbin" is the bin that contains the 'rank' (median)
		sum=0;
		midbin = 0;
		do {
			sum += counts[midbin];
			midbin++;
		} while (sum <= rank);
		midbin--;
		Nleft = sum - counts[midbin];
		Nmiddle = counts[midbin];

		// compute upper and lower bounds of the range of bins that could
		// contain the median.
		newL = lower + (midbin  ) * binsize;
		newU = lower + (midbin+1) * binsize;
		newdata = malloc(Nmiddle*sizeof(real));
		nm = 0;
		for (i=0; i<N; i++) {
			if ((data[i] >= newL) && (data[i] < newU)) {
				newdata[nm] = data[i];
				nm++;
			}
		}
		assert(nm == Nmiddle);
		free(data);
		data = newdata;

		rank -= Nleft;

		lower = newL;
		upper = newU;
		N = Nmiddle;
	}
	median = data[rank];
	free(data);

	N = r - l + 1;
	il=l;
	ir=r;
	while (1) {
		real tmpdata[D];
		int tmpperm;
		
		// scan from the left until we find an item out of place...
		while (arr[ il *D + d] <= median)
			il++;

		// scan from the right until we find an item out of place...
		while (arr[ ir *D + d] > median)
			ir--;

		if (il >= ir)
			break;

		// swap 'em!
		tmpperm  = parr[il];
		parr[il] = parr[ir];
		parr[ir] = tmpperm;

		memcpy(tmpdata,  arr+il*D, D*sizeof(real));
		memcpy(arr+il*D, arr+ir*D, D*sizeof(real));
		memcpy(arr+ir*D, tmpdata,  D*sizeof(real));

		il++;
		ir--;
	}
	m = il;

	// check that it worked.
	for (i=l; i<m; i++) {
		assert(arr[i*D + d] <= median);
	}
	for (i=m; i<=r; i++) {
		assert(arr[i*D + d] > median);
	}

	return m;
}

int kd_compare_reals(const void* v1, const void* v2) {
    real i1 = *(real*)v1;
    real i2 = *(real*)v2;
    if (i1 < i2) return -1;
    else if (i1 > i2) return 1;
    else return 0;
}

int kdtree_sam_partition(real *arr, unsigned int *parr, int l, int r,
						 int D, int d, real lower, real upper) {
	int i;
	int W; // windowsize
	real* window;
	int Nleft, Nright;
	real median;
	int N;
	int m;
	real L, U;
	int il, ir;

	assert(r >= l);
	for (i=l; i<=r; i++) {
		assert(arr[i*D+d] >= lower);
		assert(arr[i*D+d] <= upper);
	}

	N = r - l + 1;

	W = 10+(N/1000);
	window = malloc(W*sizeof(real));
	for (i=0; i<W; i++)
		window[i] = arr[(l+i)*D + d];

	qsort(window, W, sizeof(real), kd_compare_reals);

	L = window[0];
	U = window[W-1];
	Nleft = 0;
	Nright = 0;

	while (1) {

		for (i=W+1; i<N; i++) {
			real val = arr[(l+i)*D+d];
			if (val < L) {
				Nleft++;
			} else if (val > U) {
				Nright++;
			} else {
				// binary search for where it belongs...
			}
		}

	}

	// haha, this is much easier:
	median = (upper + lower) / 2.0;


	N = r - l + 1;
	il=l;
	ir=r;
	while (1) {
		real tmpdata[D];
		int tmpperm;
		
		// scan from the left until we find an item out of place...
		while (arr[ il *D + d] <= median)
			il++;

		// scan from the right until we find an item out of place...
		while (arr[ ir *D + d] > median)
			ir--;

		if (il >= ir)
			break;

		// swap 'em!
		tmpperm  = parr[il];
		parr[il] = parr[ir];
		parr[ir] = tmpperm;

		memcpy(tmpdata,  arr+il*D, D*sizeof(real));
		memcpy(arr+il*D, arr+ir*D, D*sizeof(real));
		memcpy(arr+ir*D, tmpdata,  D*sizeof(real));

		il++;
		ir--;
	}
	m = il;

	// check that it worked.
	for (i=l; i<m; i++) {
		assert(arr[i*D + d] <= median);
	}
	for (i=m; i<=r; i++) {
		assert(arr[i*D + d] > median);
	}

	return m;
}


int kdtree_qsort(real *arr, unsigned int *parr, int l, int r, int D, int d)
{
    int beg[KDTREE_MAX_LEVELS], end[KDTREE_MAX_LEVELS], i=0, j, L, R;
    static real piv_vec[KDTREE_MAX_DIM];
    unsigned int piv_perm;
    real piv;

	assert(l <= r);
	assert(l >= 0);
	assert(r >= 0);
	assert(d < D);
	assert(d >= 0);
	assert(D >= 0);

	/* startree_killer.objs:

	  fprintf(stderr, "d=%i, l=%i, r=%i\n", d, l, r);
	  if (l==6238022 && r==6287925) {
	  int i;
	  for (i=l; i<=r; i++) {
	  fprintf(stderr, "data[%i,%i]=  %g\n", i, d, arr[3*i+d]);
	  }
	  for (i=l; i<=r; i++) {
	  fprintf(stderr, "data[%i]=[%g,%g,%g]\n", i, arr[3*i], arr[3*i+1], arr[3*i+2]);
	  }
	  }
	*/

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
				assert(L<=R);
                if (L < R) {
                    for(j=0;j<D;j++) 
                        arr[D*L + j] = arr[D*R + j];
                    parr[L] = parr[R];
                    L++;
					assert(L<=R);
                }
                while (arr[D*L+d] <= piv && L < R) L++;
				assert(L<=R);
                if (L < R) {
                    for(j=0;j<D;j++) 
                        arr[D*R + j] = arr[D*L + j];
                    parr[R] = parr[L];
                    R--;
					assert(L<=R);
                }
            }
            for(j=0;j<D;j++)
                arr[D*L+j] = piv_vec[j];
            parr[L] = piv_perm;
            beg[i+1] = L+1;
            end[i+1] = end[i];
            end[i] = L;
			i++;
        } else 
            i--;
    }
    return 1;
}
#endif

/* If the root node is level 0, then maxlevel is the level at which there may
 * not be enough points to keep the tree complete (i.e. last level) */
kdtree_t *kdtree_build(real *data, int N, int D, int maxlevel) 
{
	int i;
	kdtree_t *kd; 
	int nnodes; 
	int level = 0, dim, t, m;
	int pivot;
	real lowers[D];
	real uppers[D];

	assert(maxlevel>0);

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

    for (i=0;i<D;i++) {
		lowers[i] = 1e300;
		uppers[i] = -1e300;
	}
    for (i=0;i<N;i++) {
		for (dim=0;dim<D;dim++) {
			if (data[i*D+dim] < lowers[dim])
				lowers[dim] = data[i*D+dim];
			if (data[i*D+dim] > uppers[dim])
				uppers[dim] = data[i*D+dim];
		}
	}

	//printf("n=%d;d=%d;maxlvl=%d;nnodes=%d\n",N,D,maxlevel,nnodes);

	/* Root node owns the infinite hyperrectangle  and all data points */
	/*
	  for(i=0;i<D;i++)
	  *LOW_HR(0, i) = -KDT_INFTY;
	  for(i=0;i<D;i++)
	  *HIGH_HR(0, i) = KDT_INFTY;
	  */
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
		/*
		  kdtree_qsort(data, kd->perm, NODE(i)->l, NODE(i)->r, D, dim);
		  m = (NODE(i)->r - NODE(i)->l)/2 + NODE(i)->l + 1;
		  pivot = NODE(i)->pivot = *COORD(m, dim);
		*/
		//m = kdtree_partition(data, kd->perm, NODE(i)->l, NODE(i)->r, D, dim, lowers[dim], uppers[dim]);
		m = kdtree_sam_partition(data, kd->perm, NODE(i)->l, NODE(i)->r, D, dim, lowers[dim], uppers[dim]);

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
			/*
			  memcpy(LOW_HR(2*i+1,0), LOW_HR(i,0), sizeof(real)*2*D);
			  *HIGH_HR(2*i+1, dim) = pivot;
			  memcpy(LOW_HR(2*i+2,0), LOW_HR(i,0), sizeof(real)*2*D);
			  *LOW_HR(2*i+2, dim) = pivot;
			  */
		}
	}
	kdtree_optimize(kd);
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

kdtree_node_t* kdtree_get_root(kdtree_t* kd) {
	return kd->tree;
}

real* kdtree_node_get_point(kdtree_t* tree, kdtree_node_t* node, int ind) {
	return tree->data + (node->l + ind) * tree->ndim;
}

int kdtree_node_get_index(kdtree_t* tree, kdtree_node_t* node, int ind) {
	return tree->perm[(node->l + ind)];
}

real* kdtree_get_bb_low(kdtree_t* tree, kdtree_node_t* node) {
	return (real*)(node + 1);
}

real* kdtree_get_bb_high(kdtree_t* tree, kdtree_node_t* node) {
	return (real*)((char*)(node + 1) + sizeof(real) * tree->ndim);
}

real kdtree_bb_mindist2(real* bblow1, real* bbhigh1, real* bblow2, real* bbhigh2, int dim) {
	real d2 = 0.0;
	real delta;
	int i;
    for (i=0; i<dim; i++) {
		real alo, ahi, blo, bhi;
        alo = bblow1[i];
        ahi = bbhigh1[i];
        blo = bblow2[i];
		bhi = bbhigh2[i];

		if ( ahi < blo )
			delta = blo - ahi;
		else if ( bhi < alo )
			delta = alo - bhi;
		else
			delta = 0.0;

        d2 += delta * delta;
	}
	return d2;
}

real kdtree_bb_maxdist2(real* bblow1, real* bbhigh1, real* bblow2, real* bbhigh2, int dim) {
	real d2 = 0.0;
	real delta;
	int i;
    for (i=0; i<dim; i++) {
		real alo, ahi, blo, bhi;
		real delta2;
        alo = bblow1[i];
        ahi = bbhigh1[i];
        blo = bblow2[i];
		bhi = bbhigh2[i];

		delta = bhi - alo;
		delta2 = ahi - blo;
		if (delta2 > delta) delta = delta2;

        d2 += delta * delta;
	}
	return d2;
}

real kdtree_bb_point_mindist2(real* bblow, real* bbhigh,
							  real* point, int dim) {
	real d2 = 0.0;
	real delta;
	int i;
    for (i=0; i<dim; i++) {
		real lo, hi;
		lo = bblow[i];
		hi = bbhigh[i];

		if (point[i] < lo)
			delta = lo - point[i];
		else if (point[i] > hi)
			delta = point[i] - hi;
		else
			continue;
		d2 += delta * delta;
	}
	return d2;
}

real kdtree_bb_point_maxdist2(real* bblow, real* bbhigh,
							  real* point, int dim) {
	real d2 = 0.0;
	real delta1, delta2;
	int i;
    for (i=0; i<dim; i++) {
		real lo, hi;
		lo = bblow[i];
		hi = bbhigh[i];

		delta1 = (point[i] - lo) * (point[i] - lo);
		delta2 = (point[i] - hi) * (point[i] - hi);

		if (delta1 > delta2)
			d2 += delta1;
		else
			d2 += delta2;
	}
	return d2;
}

inline int kdtree_node_to_nodeid(kdtree_t* kd, kdtree_node_t* node) {
	return ((char*)node - (char*)kd->tree) / NODE_SIZE;
}

inline kdtree_node_t* kdtree_nodeid_to_node(kdtree_t* kd, int nodeid) {
	return NODE(nodeid);
}

int kdtree_get_childid1(kdtree_t* kd, int nodeid) {
	if (ISLEAF(nodeid))
		return -1;
	return 2*nodeid + 1;
}

int kdtree_get_childid2(kdtree_t* kd, int nodeid) {
	if (ISLEAF(nodeid))
		return -1;
	return 2*nodeid + 2;
}

kdtree_node_t* kdtree_get_child1(kdtree_t* kd, kdtree_node_t* node) {
	int nodeid = kdtree_node_to_nodeid(kd, node);
	//assert(ISLEAF(nodeid) == 0);
	if (ISLEAF(nodeid))
		return NULL;
	return CHILD_POS(nodeid);
}

kdtree_node_t* kdtree_get_child2(kdtree_t* kd, kdtree_node_t* node) {
	int nodeid = kdtree_node_to_nodeid(kd, node);
	//assert(ISLEAF(nodeid) == 0);
	if (ISLEAF(nodeid))
		return NULL;
	return CHILD_NEG(nodeid);
}

int kdtree_node_is_leaf(kdtree_t* kd, kdtree_node_t* node) {
	int nodeid = kdtree_node_to_nodeid(kd, node);
	return ISLEAF(nodeid);
}

int kdtree_nodeid_is_leaf(kdtree_t* kd, int nodeid) {
	return ISLEAF(nodeid);
}

int kdtree_node_npoints(kdtree_node_t* node) {
	return 1 + node->r - node->l;
}

real kdtree_node_node_mindist2(kdtree_t* tree1, kdtree_node_t* node1,
							   kdtree_t* tree2, kdtree_node_t* node2) {
	int dim = tree1->ndim;
	real *hrloa, *hrhia, *hrlob, *hrhib;

	hrloa = (real*)((char*)node1 + sizeof(kdtree_node_t));
	hrhia = (real*)((char*)node1 + sizeof(kdtree_node_t) + sizeof(real) * dim);
	hrlob = (real*)((char*)node2 + sizeof(kdtree_node_t));
	hrhib = (real*)((char*)node2 + sizeof(kdtree_node_t) + sizeof(real) * dim);

	return kdtree_bb_mindist2(hrloa, hrhia, hrlob, hrhib, dim);
}

real kdtree_node_node_maxdist2(kdtree_t* tree1, kdtree_node_t* node1,
							   kdtree_t* tree2, kdtree_node_t* node2) {
	int dim = tree1->ndim;
	real *hrloa, *hrhia, *hrlob, *hrhib;

	hrloa = (real*)((char*)node1 + sizeof(kdtree_node_t));
	hrhia = (real*)((char*)node1 + sizeof(kdtree_node_t) + sizeof(real) * dim);
	hrlob = (real*)((char*)node2 + sizeof(kdtree_node_t));
	hrhib = (real*)((char*)node2 + sizeof(kdtree_node_t) + sizeof(real) * dim);

	return kdtree_bb_maxdist2(hrloa, hrhia, hrlob, hrhib, dim);
}

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
