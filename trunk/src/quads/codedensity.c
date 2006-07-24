/**
  codedensity: investigate the distribution
  of codes in code space.

  Author: dstn

  Input:  .skdt or .ckdt
  Output: matlab literals
*/

#include <errno.h>
#include <string.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "codefile.h"
#include "kdtree.h"
#include "kdtree_access.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "dualtree_max.h"

#define OPTIONS "hf:H:D:cpr4"

extern char *optarg;
extern int optind, opterr, optopt;

bool radec = FALSE;
bool write_nearby = FALSE;
double nearby_radius = -1.0;
bool do_perms = FALSE;
int permnum = 0;
char* prefix;

kdtree_t* tree = NULL;

void print_help(char* progname) {
	fprintf(stderr, "Usage: %s -f <prefix> [-c] [-p] [-4] [-H <hists.m>] [-D <dists.m>]\n\n"
	        "-c = check results against naive search.\n"
	        "-4 = check all four permutations of codes (this only makes sense for code kdtrees).\n"
	        "-p = write out a Matlab-literals file containing the positions of the stars.\n"
	        "-r = convert XYZ to RA-DEC (this only make sense with -p or -n, and for star kdtrees).\n"
	        "-H = File where histogram data should go (matlab).\n"
	        "-D = File where distance data should go (matlab).\n",
	        progname);
}


static void swap_coords(double* v, int permnum) {
	double tmp;
	if (permnum & 1) {
		// swap C, D:  0-2, 1-3
		tmp = v[0];
		v[0] = v[2];
		v[2] = v[0];

		tmp = v[1];
		v[1] = v[3];
		v[3] = v[1];
	}
	if (permnum & 2) {
		// swap A, B: x -> 1-x
		v[0] = 1.0 - v[0];
		v[1] = 1.0 - v[1];
		v[2] = 1.0 - v[2];
		v[3] = 1.0 - v[3];
	}
}

static void swap_hrect(double* dstlo, double* dsthi,
					   double* srclo, double* srchi,
					   int permnum) {
	if (permnum & 2) {
		// swap hi/lo box hrects.
		memcpy(dstlo, srchi, 4*sizeof(double));
		memcpy(dsthi, srclo, 4*sizeof(double));
	} else {
		memcpy(dstlo, srclo, 4*sizeof(double));
		memcpy(dsthi, srchi, 4*sizeof(double));
	}
	swap_coords(dstlo, permnum);
	swap_coords(dsthi, permnum);
}

// a=query, b=search
static double box_to_box_min_dist2(kdtree_t* tree1, kdtree_node_t* node1,
								   kdtree_t* tree2, kdtree_node_t* node2) {
	double tmplo[4], tmphi[4];
	double *lo1, *hi1, *lo2, *hi2;

	lo1 = kdtree_get_bb_low(tree1, node1);
	hi1 = kdtree_get_bb_high(tree1, node1);
	lo2 = kdtree_get_bb_low(tree2, node2);
	hi2 = kdtree_get_bb_high(tree2, node2);

	if (do_perms) {
		swap_hrect(tmplo, tmphi, lo1, hi1, permnum);
		lo1 = tmplo;
		hi1 = tmphi;
	}
	return kdtree_bb_mindist2(lo1, hi1, lo2, hi2, tree->ndim);
}

static double box_to_box_max_dist2(kdtree_t* tree1, kdtree_node_t* node1,
                            kdtree_t* tree2, kdtree_node_t* node2) {
	double tmplo[4], tmphi[4];
	double *lo1, *hi1, *lo2, *hi2;

	lo1 = kdtree_get_bb_low(tree1, node1);
	hi1 = kdtree_get_bb_high(tree1, node1);
	lo2 = kdtree_get_bb_low(tree2, node2);
	hi2 = kdtree_get_bb_high(tree2, node2);

	if (do_perms) {
		swap_hrect(tmplo, tmphi, lo1, hi1, permnum);
		lo1 = tmplo;
		hi1 = tmphi;
	}
	return kdtree_bb_maxdist2(lo1, hi1, lo2, hi2, tree->ndim);
}

static double box_to_point_min_dist2(double* bblo, double* bbhi, double* point) {
	double tmplo[4], tmphi[4];
	if (do_perms) {
		swap_hrect(tmplo, tmphi, bblo, bbhi, permnum);
		bblo = tmplo;
		bbhi = tmphi;
	}
	return kdtree_bb_point_mindist2(bblo, bbhi, point, tree->ndim);
}

static double point_to_point_dist2(double* a, double* b) {
	double tmppt[4];
	if (do_perms) {
		memcpy(tmppt, b, 4*sizeof(double));
		swap_coords(tmppt, permnum);
		b = tmppt;
	}
	return distsq(a, b, 4);
}


static void bounds_function(void* extra, kdtree_node_t* ynode,
							kdtree_node_t* xnode,
							double pruning_thresh, double* lower, double* upper)
{
	double min2, max2;
	min2 = -box_to_box_min_dist2(tree, xnode, tree, ynode);
	*upper = min2;
	if (min2 < pruning_thresh) {
		return ;
	}
	max2 = -box_to_box_max_dist2(tree, xnode, tree, ynode);
	*lower = max2;
}


static double* nndists;
static int* nninds;
static int Ndone, Ntotal;

static double* pruning_threshs;
static int* bests;

/**
   This callback is called when we've reached the leaves in the search
   tree.  For each point in the query tree, we allocate space for a
   pruning threshold and the index of the best point in the search tree.
*/
static void max_start_results(void* extra, kdtree_node_t* ynode,
							  double* pruning_thresh)
{
	int i, N;

	N = kdtree_node_npoints(ynode);
	pruning_threshs = malloc(N * sizeof(double));
	bests = malloc(N * sizeof(int));

	if (do_perms && permnum) {
		// set the pruning threshold to be the nearest neighbour found
		// so far.
		for (i = 0; i < N; i++) {
			double oldval;
			double newval;
			//int ind = tree->inds[ynode->l + i];
			int ind = ynode->l + i;
			oldval = -square(nndists[ind]);
			newval = *pruning_thresh;
			if (oldval > newval)
				newval = oldval;
			pruning_threshs[i] = newval;
			bests[i] = -1;
		}
	} else {
		for (i = 0; i < N; i++) {
			pruning_threshs[i] = *pruning_thresh;
			bests[i] = -1;
		}
	}
}


/**
   This callback is called once for each search node.
*/
static
void max_results(void* extra, kdtree_node_t* ynode, kdtree_node_t* xnode,
                 double* pruning_threshold, double lower, double upper) {
	int i, N, j, M;
	int ix, iy;
	double prune;
	double *xbblo, *xbbhi;

	xbblo = kdtree_get_bb_low(tree, xnode);
	xbbhi = kdtree_get_bb_high(tree, xnode);

	N = kdtree_node_npoints(ynode);
	M = kdtree_node_npoints(xnode);
	ix = -1;
	for (i = 0; i < N; i++) {
		double* ypoint;
		double d2;
		if (upper < pruning_threshs[i]) {
			continue;
		}
		ypoint = kdtree_node_get_point(tree, ynode, i);
		iy = kdtree_node_get_index(tree, ynode, i);
		d2 = -box_to_point_min_dist2(xbblo, xbbhi, ypoint);
		if (d2 < pruning_threshs[i]) {
			continue;
		}
		for (j = 0; j < M; j++) {
			double* xpoint = kdtree_node_get_point(tree, xnode, j);
			d2 = -point_to_point_dist2(ypoint, xpoint);
			/*
			  if (d2 < pruning_threshs[i]) {
			  continue;
			  }
			*/ 
			// nearest neighbour (except yourself)
			if (d2 == 0.0) {
				ix = kdtree_node_get_index(tree, xnode, i);
				if (ix == iy)
					continue;
			}
			if (d2 >= pruning_threshs[i]) {
				ix = kdtree_node_get_index(tree, xnode, i);
				if ((do_perms) && (ix == iy))
					continue;
				pruning_threshs[i] = d2;
				bests[i] = ix;
			}
		}
	}

	// new pruning threshold for this query point is the smallest of the thresholds.
	prune = 1e300;
	for (i = 0; i < N; i++) {
		if (pruning_threshs[i] < prune) {
			prune = pruning_threshs[i];
		}
	}
	if (prune > *pruning_threshold) {
		*pruning_threshold = prune;
	}
}

static void write_array(char* fn) {
	FILE* fout;
	int i;
	fout = fopen(fn, "w");
	if (!fout) {
		printf("Couldn't open output file %s\n", fn);
		return ;
	}
	fprintf(fout, "nndists=[");
	for (i = 0; i < Ntotal; i++) {
		fprintf(fout, "%g,", nndists[i]);
	}
	fprintf(fout, "];");
	fprintf(fout, "nninds=[");
	for (i = 0; i < Ntotal; i++) {
		fprintf(fout, "%i,", nninds[i]);
	}
	fprintf(fout, "];");
	fclose(fout);
	printf("Wrote array to %s\n", fn);
}

static void write_histogram(char* fn) {
	FILE* fout;
	int Nbins = 200;
	int i;
	double bin_size;
	int* bincounts;
	double mn, mx;

	fout = fopen(fn, "w");
	if (!fout) {
		printf("Couldn't open output file %s\n", fn);
		return ;
	}

	mn = 1e300;
	mx = -1e300;
	for (i = 0; i < Ntotal; i++) {
		double d = nndists[i];
		if (d < 0.0)
			continue;
		if (d > mx)
			mx = d;
		if (d < mn)
			mn = d;
	}

	bin_size = mx / (double)(Nbins - 1);
	bincounts = calloc(Nbins, sizeof(int));

	for (i = 0; i < Ntotal; i++) {
		double d = nndists[i];
		int bin;
		if (d < 0.0)
			continue;
		bin = (int)(d / bin_size);
		bincounts[bin]++;
	}

	fprintf(fout, "binsize=%g;\n", bin_size);
	fprintf(fout, "nbins=%i;\n", Nbins);
	fprintf(fout, "bins=[");
	for (i = 0; i < Nbins; i++) {
		fprintf(fout, "%g,", bin_size * i);
	}
	fprintf(fout, "];\n");
	fprintf(fout, "counts=[");
	for (i = 0; i < Nbins; i++) {
		fprintf(fout, "%i,", bincounts[i]);
	}
	fprintf(fout, "];\n");

	free(bincounts);

	fprintf(fout, "bar(bins, counts);\n");
	fprintf(fout, "xlabel(\"4D distance\");\n");
	fprintf(fout, "ylabel(\"Count\");\n");
	fprintf(fout, "title(\"Histogram of nearist neighbour distance in codespace\");\n");
	fprintf(fout, "legend(\"hide\");\n");
	fprintf(fout, "print(\"%s.nnhists.eps\");\n", prefix);
	fprintf(fout, "print(\"%s.nnhists.png\", \"-color\");\n", prefix);

	fclose(fout);
	printf("Wrote histogram to %s\n", fn);
	fflush(stdout);
}

static void max_end_results(void* extra, kdtree_node_t* ynode) {
	int i, N;
	N = kdtree_node_npoints(ynode);
	for (i = 0; i < N; i++) {
		int ind;
		double newd;

		ind = ynode->l + i;
		newd = sqrt( -pruning_threshs[i]);

		if (newd < nndists[ind]) {
			nndists[ind] = newd;
			nninds[ind] = bests[i];
		}
	}
	free(pruning_threshs);
	free(bests);
	if ((Ndone + N) / 10000 > (Ndone / 10000)) {
		double mn = 1e300;
		double mx = -1e300;
		for (i = 0; i < Ntotal; i++) {
			if (nndists[i] > 0.0) {
				if (nndists[i] > mx) {
					mx = nndists[i];
				}
				if (nndists[i] < mn) {
					mn = nndists[i];
				}
			}
		}
		printf("%i done.  Range [%g, %g]\n", Ndone + N, mn, mx);
		fflush(stdout);
	}
	if ((Ndone + N) / 100000 > (Ndone / 100000)) {
		// write partial result.
		char fn[256];
		sprintf(fn, "/tmp/nnhists_%i.m", (Ndone + N) / 100000);
		write_histogram(fn);
	}
	Ndone += N;
}

int main(int argc, char *argv[]) {
	int argchar;
	char *treefname = NULL;
	char *distsfname = NULL;
	char *histsfname = NULL;
	dualtree_max_callbacks max_callbacks;
	int i, N, D;
	bool check = FALSE;
	bool write_points = FALSE;

	if (argc <= 2) {
		print_help(argv[0]);
		return (OPT_ERR);
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
		case 'r':
			radec = TRUE;
			break;
		case 'f':
			treefname = mk_ctreefn(optarg);
			prefix = optarg;
			break;
		case 'H':
			histsfname = optarg;
			break;
		case 'D':
			distsfname = optarg;
			break;
		case 'h':
			print_help(argv[0]);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

	if (!treefname) {
		print_help(argv[0]);
		return (OPT_ERR);
	}

	if (radec && !(write_points || write_nearby)) {
		printf("Warning: -r only makes sense when -p or -n is set.  Ignoring -r.\n");
	}

	fprintf(stdout, "  Reading KD tree from %s...", treefname);

	tree = kdtree_fits_read_file(treefname);
	if (!tree) {
		fprintf(stderr, "Couldn't read tree from file %s.\n", treefname);
		exit( -1);
	}
	fprintf(stdout, "done\n    (%d points, %d nodes).\n",
	        tree->ndata, tree->nnodes);

	memset(&max_callbacks, 0, sizeof(max_callbacks));
	max_callbacks.bounds = bounds_function;
	max_callbacks.start_results = max_start_results;
	max_callbacks.result = max_results;
	max_callbacks.end_results = max_end_results;

	N = tree->ndata;
	D = tree->ndim;

	nndists = malloc(N * sizeof(double));
	nninds  = malloc(N * sizeof(int));
	for (i = 0; i < N; i++) {
		nndists[i] = 1e300;
		nninds [i] = -1;
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
		for (i = 0; i < N; i++) {
			int d;
			double* v;
			fprintf(fout, "points(%i,:)=[", (i + 1));
			v = tree->data + i * D;
			for (d = 0; d < D; d++) {
				fprintf(fout, "%g,", v[d]);
			}
			fprintf(fout, "];\n");
		}
		if (radec) {
			fprintf(fout, "radec=zeros([%i,2]);\n", N);
			for (i = 0; i < N; i++) {
				double x, y, z, ra, dec;
				double* v;
				v = tree->data + i * D;
				x = v[0];
				y = v[1];
				z = v[2];
				ra = xy2ra(x, y);
				dec = z2dec(z);
				fprintf(fout, "radec(%i,:)=[%g,%g];\n",
				        (i + 1), ra, dec);
			}
		}
		fclose(fout);
		printf("Wrote positions to %s.\n", fn);
		break;
	}

	dualtree_max(tree, tree, &max_callbacks, 1, 0);
	if (do_perms) {
		int i;
		for (i = 1; i < 4; i++) {
			printf("Doing permutation %i...\n", i);
			permnum = i;
			dualtree_max(tree, tree, &max_callbacks, 1, 0);
		}
	}

	if (histsfname)
		write_histogram(histsfname);
	else
		write_histogram("nnhists.m");

	if (distsfname)
		write_array(distsfname);
	else
		write_array("nndists.m");

	if (check) {
		// make sure it agrees with naive search.
		double best;
		int bestind;
		int i, j;
		double eps = 1e-15;
		printf("Checking against exhaustive search...\n");
		fflush(stdout);
		for (i = 0; i < N; i++) {
			double* point1;
			if (i && (i % 1000 == 0)) {
				printf("%i checked.\n", i);
				fflush(stdout);
			}
			best = 1e300;
			bestind = -1;
			point1 = tree->data + i * D;
			if (do_perms) {
				for (j = 0; j < N; j++) {
					double* point2;
					double d2;
					if (i == j)
						continue;
					point2 = tree->data + j * D;
					for (permnum = 0; permnum < 4; permnum++) {
						d2 = distsq(point1, point2, D);
						if (d2 < best) {
							best = d2;
							bestind = j;
						}
					}
				}
			} else {
				for (j = 0; j < N; j++) {
					double* point2;
					double d2;
					if (i == j)
						continue;
					point2 = tree->data + j * D;
					d2 = distsq(point1, point2, D);
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

	free(nninds);
	free(nndists);
	kdtree_close(tree);
	return 0;
}
