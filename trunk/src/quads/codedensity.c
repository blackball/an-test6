/**
   codedensity: investigate the distribution
   of codes in code space.

   Author: dstn

   Input:  .skdt or .ckdt
   Output: 
*/

#include <errno.h>

#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"
#include "KD/kdtree.h"
#include "KD/distances.h"
#include "dualtree.h"
#include "dualtree_max.h"

#define OPTIONS "ht:cprn:4"

extern char *optarg;
extern int optind, opterr, optopt;

bool radec = FALSE;
bool write_nearby = FALSE;
double nearby_radius = -1.0;
dyv_array* array = NULL;
bool do_perms = FALSE;
int permnum = 0;

void print_help(char* progname) {
    fprintf(stderr, "Usage: %s -t <tree-file> [-c] [-p] [-4]\n\n"
			"-c = check results against naive search.\n"
			"-4 = check all four permutations of codes (this only makes sense for code kdtrees).\n"
			"-p = write out a Matlab-literals file containing the positions of the stars.\n"
			"-n <radius> = write out Matlab literals containing the positions of nearby stars\n"
			"      where 'nearby' is defined as being within <radius> units.\n"
			"-r = convert XYZ to RA-DEC (this only make sense with -p or -n, and for star kdtrees).\n",
			progname);
}

hrect tmp_hrect;
dyv* tmp_dyv;

void swap_coords(dyv* v) {
	double tmp;
	if (permnum & 1) {
		// swap C, D:  0-2, 1-3
		tmp = dyv_ref(v, 0);
		dyv_set(v, 0, dyv_ref(v, 2));
		dyv_set(v, 2, tmp);

		tmp = dyv_ref(v, 1);
		dyv_set(v, 1, dyv_ref(v, 3));
		dyv_set(v, 3, tmp);
	}
	if (permnum & 2) {
		// swap A, B: x -> 1-x
		dyv_set(v, 0, 1.0 - dyv_ref(v, 0));
		dyv_set(v, 1, 1.0 - dyv_ref(v, 1));
		dyv_set(v, 2, 1.0 - dyv_ref(v, 2));
		dyv_set(v, 3, 1.0 - dyv_ref(v, 3));
	}
}

void swap_hrect(hrect* h) {
	if (permnum & 2) {
		// swap hi/lo box hrects.
		copy_dyv(h->lo, tmp_hrect.hi);
		copy_dyv(h->hi, tmp_hrect.lo);
	} else {
		copy_dyv(h->lo, tmp_hrect.lo);
		copy_dyv(h->hi, tmp_hrect.hi);
	}
	swap_coords(tmp_hrect.lo);
	swap_coords(tmp_hrect.hi);
}

// a=query, b=search
double box_to_box_min_dist2(hrect* a, hrect* b) {
	if (!do_perms)
		return hrect_hrect_min_dsqd(a, b);

	swap_hrect(b);
	return hrect_hrect_min_dsqd(a, &tmp_hrect);
}

double box_to_box_max_dist2(hrect* a, hrect* b) {
	if (!do_perms)
		return hrect_hrect_max_dsqd(a, b);

	swap_hrect(b);
	return hrect_hrect_max_dsqd(a, &tmp_hrect);
}

double box_to_point_min_dist2(hrect* b, dyv* a) {
	// query point, search box.
	if (!do_perms)
		return hrect_dyv_min_dsqd(b, a);

	swap_hrect(b);
	return hrect_dyv_min_dsqd(&tmp_hrect, a);
}

double point_to_point_dist2(dyv* a, dyv* b) {
	if (!do_perms)
		return dyv_dyv_dsqd(a, b);

	copy_dyv(b, tmp_dyv);
	swap_coords(tmp_dyv);
	return dyv_dyv_dsqd(a, tmp_dyv);
}


void bounds_function(void* extra, node* query, node* search, double thresh,
					 double* lower, double* upper) {
    double min2, max2;
    min2 = -box_to_box_min_dist2(query->box, search->box);
    *upper = min2;
    if (min2 < thresh) {
		return;
    }
    max2 = -box_to_box_max_dist2(query->box, search->box);
    *lower = max2;
}


double* nndists;
int* nninds;
int Ndone, Ntotal;

double* pruning_threshs;
int* bests;

/**
   This callback is called when we've reached the leaves in the search
   tree.  For each point in the query tree, we allocate space for a
   pruning threshold and the index of the best point in the search tree.
*/
void max_start_results(void* extra, node* query, blocklist* leaves, double* pruning_thresh) {
    int i, N;
    //printf("start results for %p: threshold %g\n", query, *pruning_thresh);

    N = query->num_points;
    pruning_threshs = (double*)malloc(N * sizeof(double));
    bests = (int*)malloc(N * sizeof(int));

	if (do_perms && permnum) {
		// set the pruning threshold to be the nearest neighbour found
		// so far.
		for (i=0; i<N; i++) {
			double oldval;
			double newval;
			int ind = ivec_ref(query->pindexes, i);
			oldval = -square(nndists[ind]);
			newval = *pruning_thresh;
			if (oldval > newval)
				newval = oldval;
			pruning_threshs[i] = newval;
			bests[i] = -1;
		}
	} else {
		for (i=0; i<N; i++) {
			pruning_threshs[i] = *pruning_thresh;
			bests[i] = -1;
		}
	}
}


/**
   This callback is called once for each search node.
*/
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
		d2 = -box_to_point_min_dist2(search->box, ypoint);
		if (d2 < pruning_threshs[i]) {
			continue;
		}
		for (j=0; j<M; j++) {
			dyv* xpoint = dyv_array_ref(search->points, j);
			d2 = -point_to_point_dist2(ypoint, xpoint);
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
				if (do_perms && (ivec_ref(search->pindexes, j) == ivec_ref(query->pindexes, i)))
					continue;
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

void write_nearby_positions(char* fn, double radius, bool radec, dyv_array* array) {
    FILE* fout;
    int i;
    fout = fopen(fn, "w");
    if (!fout) {
		printf("Couldn't open output file %s\n", fn);
		return;
    }
    fprintf(fout, "nnposns=[");
    for (i=0; i<Ntotal; i++) {
		dyv* v;
		int d, D;
		if (nninds[i] < 0) continue;
		if (nndists[i] > radius) continue;
		v = dyv_array_ref(array, i);
		D = v->size;
		for (d=0; d<D; d++) {
			double x = dyv_ref(v, d);
			fprintf(fout,"%s%g", (d?",":""), x);
		}
		fprintf(fout, ";\n");
    }
    fprintf(fout, "];");
    if (radec) {
		fprintf(fout, "nnposnsradec=[");
		for (i=0; i<Ntotal; i++) {
			double x,y,z,ra,dec;
			dyv* v;
			if (nninds[i] < 0) continue;
			if (nndists[i] > radius) continue;
			v = dyv_array_ref(array, i);
			x = dyv_ref(v, 0);
			y = dyv_ref(v, 1);
			z = dyv_ref(v, 2);
			ra  = xy2ra(x, y);
			dec = z2dec(z);
			fprintf(fout, "%g,%g;\n", ra, dec);
		}
		fprintf(fout, "];");
		fprintf(fout, "ra=nnposnsradec(:,1); dec=nnposnsradec(:,2);\n");
    }
    fclose(fout);
    printf("Wrote positions to %s\n", fn);
}

void write_array(char* fn) {
    FILE* fout;
    int i;
    fout = fopen(fn, "w");
    if (!fout) {
		printf("Couldn't open output file %s\n", fn);
		return;
    }
    fprintf(fout, "nndists=[");
    for (i=0; i<Ntotal; i++) {
		fprintf(fout, "%g,", nndists[i]);
    }
    fprintf(fout, "];");
    fprintf(fout, "nninds=[");
    for (i=0; i<Ntotal; i++) {
		fprintf(fout, "%i,", nninds[i]);
    }
    fprintf(fout, "];");
    fclose(fout);
    printf("Wrote array to %s\n", fn);
}

void write_histogram(char* fn) {
    FILE* fout;
    int Nbins = 200;
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
    printf("Wrote histogram to %s\n", fn);
    fflush(stdout);
}

void max_end_results(void* extra, node* query) {
    int i, N;
    N = query->num_points;
    for (i=0; i<N; i++) {
		int ind;
		double newd;

		ind = ivec_ref(query->pindexes, i);
		newd = sqrt(-pruning_threshs[i]);

		if (newd < nndists[ind]) {
			/*
			  printf("  %i (%i): best %i, dist %g (%g), nndist %g, nnind %i\n",
			  i, ind,
			  bests[i], sqrt(-pruning_threshs[i]),
			  pruning_threshs[i], nndists[ind], nninds[ind]);
			*/

			nndists[ind] = newd;
			nninds[ind] = bests[i];
		}
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
		fflush(stdout);
    }
    if ((Ndone+N)/100000 > (Ndone/100000)) {
		// write partial result.
		char fn[256];
		sprintf(fn, "/tmp/nnhists_%i.m", (Ndone+N)/100000);
		write_histogram(fn);

		if (write_nearby) {
			sprintf(fn, "/tmp/nearby_%i.m", (Ndone+N)/100000);
			write_nearby_positions(fn, nearby_radius, radec, array);
		}
    }
    Ndone += N;
}


int main(int argc, char *argv[]) {
    int argchar;
    char *treefname = NULL;
    FILE *treefid = NULL;
    kdtree *tree = NULL;
    dualtree_max_callbacks max_callbacks;
    int i, N, D;
    bool check = FALSE;
    bool write_points = FALSE;

    if (argc <= 2) {
		print_help(argv[0]);
		return(OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case '4':
			do_perms = TRUE;
			break;
		case 'c':
			check = TRUE;
			break;
		case 'p':
			write_points = TRUE;
			break;
		case 'n':
			write_nearby = TRUE;
			nearby_radius = atof(optarg);
			if (nearby_radius == 0.0) {
				printf("Couldn't parse nearby-radius: %s\n", optarg);
				print_help(argv[0]);
				exit(-1);
			}
			break;
		case 'r':
			radec = TRUE;
			break;
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

    if (radec && !(write_points || write_nearby)) {
		printf("Warning: -r only makes sense when -p or -n is set.  Ignoring -r.\n");
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

    if (check || write_points || write_nearby) {
		array = mk_dyv_array_from_kdtree(tree);
    }

    //
    memset(&max_callbacks, 0, sizeof(max_callbacks));
    max_callbacks.bounds = bounds_function;
    max_callbacks.start_results = max_start_results;
    max_callbacks.result = max_results;
    max_callbacks.end_results = max_end_results;

    N = tree->root->num_points;
    D = kdtree_num_dims(tree);

    nndists = (double*)malloc(N * sizeof(double));
    nninds = (int*)malloc(N * sizeof(int));
    for (i=0; i<N; i++) {
		//nndists[i] = -1.0;
		nndists[i] = 1e300;
		nninds[i] = -1;
    }
    Ndone = 0;
    Ntotal = N;

    while (write_points) {
		FILE* fout;
		char* fn = "/tmp/positions.m";
		if (radec && (D != 3)) {
			printf("Warning: -r option only make sense when D=3, but in this file D=%i.\n", D);
			break;
		}
		fout = fopen(fn, "w");
		if (!fout) {
			printf("Couldn't open file: %s: %s\n",
				   fn, strerror(errno));
			break;
		}
		fprintf(fout, "points=zeros([%i,%i]);\n", N, D);
		for (i=0; i<N; i++) {
			int d;
			dyv* v;
			fprintf(fout, "points(%i,:)=[", (i+1));
			v = dyv_array_ref(array, i);
			for (d=0; d<D; d++) {
				fprintf(fout, "%g,", dyv_ref(v, d));
			}
			fprintf(fout, "];\n");
		}
		if (radec) {
			fprintf(fout, "radec=zeros([%i,2]);\n", N);
			for (i=0; i<N; i++) {
				double x,y,z,ra,dec;
				dyv* v = dyv_array_ref(array, i);
				x = dyv_ref(v, 0);
				y = dyv_ref(v, 1);
				z = dyv_ref(v, 2);
				ra  = xy2ra(x, y);
				dec = z2dec(z);
				fprintf(fout, "radec(%i,:)=[%g,%g];\n",
						(i+1), ra, dec);
			}
		}
		fclose(fout);
		printf("Wrote positions to %s.\n", fn);
		break;
    }

	if (do_perms) {
		//int D = dyv_size(dyv_array_ref(tree->root->points, 0));
		int D = 4;
		tmp_hrect.lo = mk_dyv(D);
		tmp_hrect.hi = mk_dyv(D);
		tmp_dyv = mk_dyv(D);
	}

    dualtree_max(tree, tree, &max_callbacks, 1);
	if (do_perms) {
		int i;
		for (i=1; i<4; i++) {
			printf("Doing permutation %i...\n", i);
			permnum = i;
			dualtree_max(tree, tree, &max_callbacks, 1);
		}
	}

    if (write_nearby) {
		write_nearby_positions("/tmp/nearby.m", nearby_radius, radec, array);
    }

    write_histogram("/tmp/nnhists.m");

    write_array("/tmp/nndists.m");

    if (check) {
		// make sure it agrees with naive search.
		double best;
		int bestind;
		int i, j;
		double eps = 1e-15;
		printf("Checking against exhaustive search...\n");
		fflush(stdout);
		for (i=0; i<N; i++) {
			dyv* point1;
			if (i && (i % 1000 == 0)) {
				printf("%i checked.\n", i);
				fflush(stdout);
			}
			best = 1e300;
			bestind = -1;
			point1 = dyv_array_ref(array, i);
			if (do_perms) {
				for (j=0; j<N; j++) {
					dyv* point2;
					double d2;
					if (i == j) continue;
					point2 = dyv_array_ref(array, j);
					for (permnum = 0; permnum<4; permnum++) {
						d2 = point_to_point_dist2(point1, point2);
						if (d2 < best) {
							best = d2;
							bestind = j;
						}
					}
				}
			} else {
				for (j=0; j<N; j++) {
					dyv* point2;
					double d2;
					if (i == j) continue;
					point2 = dyv_array_ref(array, j);
					d2 = dyv_dyv_dsqd(point1, point2);
					if (d2 < best) {
						best = d2;
						bestind = j;
					}
				}
			}
			best = sqrt(best);
			if (fabs(best - nndists[i]) > eps) {
				printf("Error: %i: %g, %g\n",
					   i, best, nndists[i]);
			}
			if (bestind != nninds[i]) {
				printf("Index error: %i: %i, %i\n", i, bestind, nninds[i]);
			}
		}
		printf("Correctness check complete.\n");
    }

	if (do_perms) {
		free_dyv(tmp_hrect.lo);
		free_dyv(tmp_hrect.hi);
		free_dyv(tmp_dyv);
	}

    if (array) {
		free_dyv_array_from_kdtree(array);
    }

    free(nninds);
    free(nndists);
    free_kdtree(tree);
    return 0;

}
