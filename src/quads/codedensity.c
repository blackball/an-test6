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

int main(int argc, char *argv[])
{
    int argchar;
    char *treefname = NULL;
    FILE *treefid = NULL;
    kdtree *tree = NULL;
    //dyv_array* codearray = NULL;
    //double index_scale;
    int i;
    double maxdist2;
    dualtree_callbacks callbacks;

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

    fprintf(stderr, "  Reading KD tree from %s...", treefname);
    fflush(stderr);
    fopenin(treefname, treefid);
    tree = fread_kdtree(treefid);
    fclose(treefid);
    treefid = NULL;
    if (!tree)
	return (2);
    fprintf(stderr, "done\n    (%d points, %d nodes, depth %d).\n",
	    kdtree_num_points(tree), kdtree_num_nodes(tree),
	    kdtree_max_depth(tree));
    /*
      {
      fprintf(stderr, "  Making dyv_array from kdtree...\n");
      codearray = mk_dyv_array_from_kdtree(tree);
      }
    */

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
}
