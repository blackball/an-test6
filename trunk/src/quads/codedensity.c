/**
   codedensity: investigate the distribution
   of codes in code space.

   Author: dstn
*/

#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"
#include "KD/kdtree.h"
#include "KD/distances.h"
#include "dualtree.h"
#include "dualtree_max.h"

#define OPTIONS "ht:"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname) {
    fprintf(stderr, "Usage: %s -t <tree-file>\n\n", progname);
}

inline double square(double d) {
    return d*d;
}

int Nbuckets;
//int bucket_sum;
int* bucket_counts;
double* bucket_mindist2;
double* bucket_maxdist2;

void pointwise_result(node* n1, node* n2) {
    int i, j, k;
    if (node_is_leaf(n1) && node_is_leaf(n2)) {
	for (i=0; i<n1->num_points; i++) {
	    dyv* p1 = dyv_array_ref(n1->points, i);
	    for (j=0; j<n2->num_points; j++) {
		double dist2;
		dyv* p2 = dyv_array_ref(n2->points, j);
		dist2 = dyv_dyv_dsqd(p1, p2);
		for (k=0; k<Nbuckets; k++) {
		    if ((dist2 <= bucket_maxdist2[k]) &&
			(dist2 >= bucket_mindist2[k])) {

			bucket_counts[k]++;
			/*
			  bucket_sum++;
			  printf("points: bucket %i (%i total)\n",
			  k, bucket_sum);
			*/
			break;
		    }
		}
	    }
	}
    } else if (!node_is_leaf(n1)) {
	pointwise_result(n1->child1, n2);
	pointwise_result(n1->child2, n2);
    } else { //if (!node_is_leaf(n2)) {
	pointwise_result(n1, n2->child1);
	pointwise_result(n1, n2->child2);
    }
}

void result(void* extra, node* n1, node* n2) {
    double min2, max2;
    int i;
    min2 = hrect_hrect_min_dsqd(n1->box, n2->box);
    max2 = hrect_hrect_max_dsqd(n1->box, n2->box);
    /*
      printf("result: %i x %i: bounds: [ %g, %g ].\n",
      n1->num_points, n2->num_points, sqrt(min2),
      sqrt(max2));
    */
    for (i=0; i<Nbuckets; i++) {
	if ((max2 <= bucket_maxdist2[i]) &&
	    (min2 >= bucket_mindist2[i])) {
	    // each pair of particles in these nodes have
	    // distances that fall inside bucket i.
	    bucket_counts[i] += (n1->num_points *
				 n2->num_points);
	    /*
	      bucket_sum += (n1->num_points * n2->num_points);
	      printf("bucket %i: %i x %i (%i total)\n", i,
	      n1->num_points, n2->num_points,
	      bucket_sum);
	    */
	    return;
	}
    }
    printf("no match.\n");
    pointwise_result(n1, n2);
}

bool within_range(void* vparams, node* n1, node* n2) {
    double min2, max2;
    int i;
    min2 = hrect_hrect_min_dsqd(n1->box, n2->box);
    max2 = hrect_hrect_max_dsqd(n1->box, n2->box);
    /*
      printf("%i x %i: bounds: [ %g, %g ].\n",
      n1->num_points, n2->num_points, sqrt(min2),
      sqrt(max2));
    */
    for (i=0; i<Nbuckets; i++) {
	if ((max2 <= bucket_maxdist2[i]) &&
	    (min2 >= bucket_mindist2[i])) {
	    // each pair of particles in these nodes have
	    // distances that fall inside bucket i.
	    bucket_counts[i] += (n1->num_points *
				 n2->num_points);
	    //printf("bucket %i\n", i);
	    // no need to recurse.
	    return FALSE;
	}
    }
    /*
      printf("no match: (leaf1,leaf2) = ( %c , %c )\n",
      slow_node_is_leaf(n1) ? 'y':'n',
      slow_node_is_leaf(n2) ? 'y':'n');
      if (n1->child1) {
      printf("n1->child1 has %i points.\n", n1->child1->num_points);
      }
      if (n1->child2) {
      printf("n1->child2 has %i points.\n", n1->child2->num_points);
      }
      if (n2->child1) {
      printf("n2->child1 has %i points.\n", n2->child1->num_points);
      }
      if (n2->child2) {
      printf("n2->child2 has %i points.\n", n2->child2->num_points);
      }
    */
    /*
      if (node_is_leaf(n1) || node_is_leaf(n2)) {
      // HACK - compute individual distances.
      printf("Warning: one node is a leaf, but the bounds don't fall "
      "within a bucket.\n");
      }
    */
    return TRUE;
}

void bounds_function(void* extra, node* query, node* search, double thresh,
		     double* lower, double* upper) {
    double min2, max2;
    min2 = -hrect_hrect_min_dsqd(query->box, search->box);
    *upper = min2;
    if (min2 < thresh) {
	return;
    }
    max2 = -hrect_hrect_max_dsqd(query->box, search->box);
    *lower = max2;
}


double* nndists;
int Ndone, Ntotal;

double* pruning_threshs;
int* bests;

void max_start_results(void* extra, node* query, blocklist* leaves, double* pruning_thresh) {
    int i, N;
    //printf("start results for %p: threshold %g\n", query, *pruning_thresh);

    N = query->num_points;
    pruning_threshs = (double*)malloc(N * sizeof(double));
    bests = (int*)malloc(N * sizeof(int));
    for (i=0; i<N; i++) {
	pruning_threshs[i] = *pruning_thresh;
	bests[i] = -1;
    }
}

void max_results(void* extra, node* query, node* search, double* pruning_threshold,
		 double lower, double upper) {
    int i, N, j, M;
    double prune;

    //printf("  %p: %p (%i): [%g, %g]   %g\n",
    //query, search, search->num_points, lower, upper, *pruning_threshold);

    N = query->num_points;
    M = search->num_points;
    for (i=0; i<N; i++) {
	dyv* ypoint;
	double d2;
	if (upper < pruning_threshs[i]) {
	    continue;
	}
	ypoint = dyv_array_ref(query->points, i);
	d2 = -hrect_dyv_min_dsqd(search->box, ypoint);
	if (d2 < pruning_threshs[i]) {
	    continue;
	}
	for (j=0; j<M; j++) {
	    dyv* xpoint = dyv_array_ref(search->points, j);
	    d2 = -dyv_dyv_dsqd(xpoint, ypoint);
	    /*
	      if (d2 < pruning_threshs[i]) {
	      continue;
	      }
	    */
	    // nearest neighbour (except yourself)
	    if ((d2 == 0.0) && (ivec_ref(search->pindexes, j) == ivec_ref(query->pindexes, i))) {
		continue;
	    }
	    if (d2 >= pruning_threshs[i]) {
		pruning_threshs[i] = d2;
		bests[i] = ivec_ref(search->pindexes, j);
	    }
	}
    }

    // new pruning threshold for this query point is the smallest of the thresholds.
    prune = 1e300;
    for (i=0; i<N; i++) {
	if (pruning_threshs[i] < prune) {
	    prune = pruning_threshs[i];
	}
    }
    //printf("prune=%g\n", prune);
    if (prune > *pruning_threshold) {
	//printf("updated pruning threshold to %g\n", prune);
	*pruning_threshold = prune;
    }
}

void write_histogram(char* fn) {
    FILE* fout;
    int Nbins = 100;
    int i;
    double bin_size;
    int* bincounts;
    double mn, mx;

    fout = fopen(fn, "w");
    if (!fout) {
	printf("Couldn't open output file %s\n", fn);
	return;
    }

    mn = 1e300;
    mx = -1e300;
    for (i=0; i<Ntotal; i++) {
	double d = nndists[i];
	if (d < 0.0) continue;
	if (d > mx) mx = d;
	if (d < mn) mn = d;
    }

    bin_size = mx / (double)(Nbins-1);
    bincounts = (int*)malloc(Nbins * sizeof(int));
    memset(bincounts, 0, Nbins * sizeof(int));

    for (i=0; i<Ntotal; i++) {
	double d = nndists[i];
	int bin;
	if (d < 0.0) continue;
	bin = (int)(d / bin_size);
	bincounts[bin]++;
    }

    fprintf(fout, "binsize=%g;\n", bin_size);
    fprintf(fout, "nbins=%i;\n", Nbins);
    fprintf(fout, "bins=[");
    for (i=0; i<Nbins; i++) {
	fprintf(fout, "%g,", bin_size * i);
    }
    fprintf(fout, "];\n");
    fprintf(fout, "counts=[");
    for (i=0; i<Nbins; i++) {
	fprintf(fout, "%i,", bincounts[i]);
    }
    fprintf(fout, "];\n");

    free(bincounts);

    fclose(fout);
    printf("Wrote file %s\n", fn);
}

void max_end_results(void* extra, node* query) {
    int i, N;
    N = query->num_points;
    for (i=0; i<N; i++) {
	/*
	  printf("  %i (%i): best %i, dist %g (%g)\n",
	  i, ivec_ref(query->pindexes, i),
	  bests[i], sqrt(-pruning_threshs[i]),
	  pruning_threshs[i]);
	*/

	nndists[ivec_ref(query->pindexes, i)] = sqrt(-pruning_threshs[i]);
    }
    free(pruning_threshs);
    free(bests);
    if ((Ndone+N)/10000 > (Ndone/10000)) {
	double mn = 1e300;
	double mx = -1e300;
	for (i=0; i<Ntotal; i++) {
	    if (nndists[i] > 0.0) {
		if (nndists[i] > mx) {
		    mx = nndists[i];
		}
		if (nndists[i] < mn) {
		    mn = nndists[i];
		}
	    }
	}
	printf("%i done.  Range [%g, %g]\n", Ndone+N, mn, mx);
    }
    if ((Ndone+N)/100000 > (Ndone/100000)) {
	// write partial result.
	char fn[256];
	sprintf(fn, "/tmp/nndists_%i.m", (Ndone+N)/100000);
	write_histogram(fn);
    }
    Ndone += N;
}


int main(int argc, char *argv[]) {
    int argchar;
    char *treefname = NULL;
    FILE *treefid = NULL;
    kdtree *tree = NULL;
    dualtree_max_callbacks max_callbacks;
    int i, N;
    /*
      double maxdist2;
      dualtree_callbacks callbacks;
    */
    dyv_array* array;
    bool check = FALSE;

    if (argc <= 2) {
	print_help(argv[0]);
	return(OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
	switch (argchar) {
	case 't':
	    treefname = optarg;
	    break;
	case 'h':
	    print_help(argv[0]);
	    return(HELP_ERR);
	default:
	    return (OPT_ERR);
	}

    if (!treefname) {
	print_help(argv[0]);
	return(OPT_ERR);
    }

    fprintf(stdout, "  Reading KD tree from %s...", treefname);
    fflush(stdout);
    fopenin(treefname, treefid);
    tree = fread_kdtree(treefid);
    fclose(treefid);
    treefid = NULL;
    if (!tree)
	return (2);
    fprintf(stdout, "done\n    (%d points, %d nodes, depth %d).\n",
	    kdtree_num_points(tree), kdtree_num_nodes(tree),
	    kdtree_max_depth(tree));


    if (check) {
	array = mk_dyv_array_from_kdtree(tree);
    }

    //
    memset(&max_callbacks, 0, sizeof(max_callbacks));
    max_callbacks.bounds = bounds_function;
    max_callbacks.start_results = max_start_results;
    max_callbacks.result = max_results;
    max_callbacks.end_results = max_end_results;

    N = tree->root->num_points;

    nndists = (double*)malloc(N * sizeof(double));
    for (i=0; i<N; i++) {
	nndists[i] = -1.0;
    }
    Ndone = 0;
    Ntotal = N;

    /*
      Nbins = 100;
      bin_size = (0.05 / (double)Nbins);
      bincounts = (int*)malloc(Nbins * sizeof(int));
      memset(bincounts, 0, Nbins * sizeof(int));
    */

    dualtree_max(tree, tree, &max_callbacks);

    if (check) {
	// make sure it agrees with naive search.
	double best;
	int i, j;
	double eps = 1e-15;
	for (i=0; i<N; i++) {
	    best = 1e300;
	    dyv* point1 = dyv_array_ref(array, i);
	    for (j=0; j<N; j++) {
		dyv* point2;
		double d2;
		if (i == j) continue;
		point2 = dyv_array_ref(array, j);
		d2 = dyv_dyv_dsqd(point1, point2);
		if (d2 < best) {
		    best = d2;
		}
	    }
	    best = sqrt(best);
	    if (fabs(best - nndists[i]) > eps) {
		printf("Error: %i: %g, %g\n",
		       i, best, nndists[i]);
	    }
	}
	printf("Correctness check complete.\n");
	free_dyv_array_from_kdtree(array);
    }


    /*
      fprintf(stderr, "nndists=[");
      for (i=0; i<N; i++) {
      fprintf(stderr, "%g,", nndists[i]);
      }
      fprintf(stderr, "];");
    */

    write_histogram("/tmp/nndists.m");

    free_kdtree(tree);
    return 0;

    /*
      ;
      // find max dist-squared in the tree.
      maxdist2 = hrect_hrect_max_dsqd(tree->root->box,
      tree->root->box);
      fprintf(stderr, "Max distance in tree: %g\n", sqrt(maxdist2));

      Nbuckets = 40;
      bucket_counts = (int*)malloc(sizeof(int) * Nbuckets);
      bucket_mindist2 = (double*)malloc(sizeof(double) * Nbuckets);
      bucket_maxdist2 = (double*)malloc(sizeof(double) * Nbuckets);
      //bucket_sum = 0;
      {
      double maxdist = sqrt(maxdist2);
      double diststep = maxdist / (double)Nbuckets;
      for (i=0; i<Nbuckets; i++) {
      bucket_counts[i] = 0;
      bucket_mindist2[i] = square(i * diststep);
      bucket_maxdist2[i] = square((i+1) * diststep);
      }
      }

      callbacks.decision = within_range;
      callbacks.decision_extra = NULL;
      callbacks.start_results = NULL;
      callbacks.start_extra = NULL;
      callbacks.result = result;
      callbacks.result_extra = NULL;
      callbacks.end_results = NULL;
      callbacks.end_extra = NULL;

      dualtree_search(tree, tree, &callbacks);

      {
      int totalcounts = 0;
      for (i=0; i<Nbuckets; i++) {
      totalcounts += bucket_counts[i];
      }
      printf("Total counts: %i\n", totalcounts);

      for (i=0; i<Nbuckets; i++) {
      if (bucket_counts[i]) {
      printf("Bucket %i: [%g, %g]: %i (%i %%)\n",
      i,
      sqrt(bucket_mindist2[i]),
      sqrt(bucket_maxdist2[i]),
      bucket_counts[i],
      (int)rint(100.0 * bucket_counts[i] /
      (double)totalcounts));
      }
      }
      }
      printf("bucket_x_min=[");
      for (i=0; i<Nbuckets; i++) {
      printf("%s%g", (i?",":""), 
      sqrt(bucket_mindist2[i]));
      }
      printf("];\n");
      printf("bucket_x_max=[");
      for (i=0; i<Nbuckets; i++) {
      printf("%s%g", (i?",":""), 
      sqrt(bucket_maxdist2[i]));
      }
      printf("];\n");
      printf("bucket_x_mid=[");
      for (i=0; i<Nbuckets; i++) {
      printf("%s%g", (i?",":""), 
      (sqrt(bucket_mindist2[i]) +
      sqrt(bucket_maxdist2[i]))/2.0);
      }
      printf("];\n");
      printf("bucket_count=[");
      for (i=0; i<Nbuckets; i++) {
      printf("%s%i", (i?",":""), bucket_counts[i]);
      }
      printf("];\n");


      free(bucket_counts);
      free(bucket_mindist2);
      free(bucket_maxdist2);

      free_kdtree(tree);
      return 0;
    */
}
