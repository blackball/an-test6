#include "blocklist.h"
#include "starutil.h"

//const static int NBINS = 20;
//const static int DIMS = 6;

#define NBINS 20
#define DIMS   6
#define LEVELS 3
#define AGREE_DIST 0.001

/*
  Assume the points have been unitized and that each dimension
  will be split evenly into NBINS bins.
 */

struct hitnode;
typedef struct hitnode hitnode;

struct hitnode {
	// if non-NULL, this is a leaf.
	// list of MatchObj*: matches
	blocklist* matchlist;
	// list of blocklist*, one for each entry in matchlist; the
	//    list of agreeing matches to which that match belongs.
	blocklist* agreelist;

	hitnode* children;
	// the dimension along which this node splits.
	int dim;
};

struct hitlist_struct {
	int nbest;
	int ntotal;
	blocklist* best;
	hitnode root;
	/*
	  double scales[DIMS];
	  double lowers[DIMS];
	  double uppers[DIMS];
	*/
	int levels;
	double agreedist2;
};
typedef struct hitlist_struct hitlist;

#define DONT_DEFINE_HITLIST
#include "hitlist.h"

char* hitlist_get_parameter_help() {
	return "";
}
char* hitlist_get_parameter_options() {
	return "";
}
int hitlist_process_parameter(char argchar, char* optarg) {
	return 0;
}
void hitlist_set_default_parameters() {
}


hitlist* hitlist_new() {
	//int d;
	hitlist* hl = (hitlist*)malloc(sizeof(hitlist));
	hl->ntotal = 0;
	hl->nbest = 0;
	hl->best = NULL;
	hl->root.matchlist = NULL;
	hl->root.agreelist = NULL;
	hl->root.children = NULL;
	hl->root.dim = 0;
	/*
	  for (d=0; d<DIMS; d++)
	  hl->scales[d] = 1.0;
	*/
	hl->levels = LEVELS;
	hl->agreedist2 = square(AGREE_DIST);
	return hl;
}

void find_potential_agreers(hitnode* node, int level, int maxlevels,
							double dist2sofar, double maxdist2,
							double* vec, blocklist* potentials) {
	double val;
	int i;
	int bin;
	double dleft, dright;
	double mindist2;

	if (level == maxlevels) {
		blocklist_pointer_append(potentials, node);
		return;
	}

	if (!node->children) {
		return;
	}

	// compute which bin this vector belongs in.
	val = vec[node->dim];
	bin = (int)(val * NBINS);
	if (bin < 0) bin = 0;
	if (bin >= NBINS) bin = NBINS-1;

	// compute the distances to this bin boundary.
	dleft  = square(val - (double)bin / (double)NBINS);
	dright = square((double)(bin+1) / (double)NBINS - val);
	mindist2 = (dleft < dright) ? dleft : dright;

	if (mindist2 + dist2sofar <= maxdist2) {
		find_potential_agreers(node->children + bin, level+1, maxlevels,
							   dist2sofar + mindist2, maxdist2,
							   vec, potentials);
	}

	for (i=bin+1; i<NBINS; i++) {
		// compute the distances to the bin boundary.
		dleft  = square(val - (double)i / (double)NBINS);
		if (dist2sofar + dleft > maxdist2)
			continue;

		find_potential_agreers(node->children + i, level+1, maxlevels,
							   dist2sofar + dleft, maxdist2, vec, potentials);
	}

	for (i=bin-1; i>=0; i--) {
		// compute the distances to the bin boundary.
		dright = square((double)(i+1) / (double)NBINS - val);
		if (dist2sofar + dright > maxdist2)
			continue;

		find_potential_agreers(node->children + i, level+1, maxlevels,
							   dist2sofar + dright, maxdist2, vec, potentials);
	}
}

/**
   Finds the bin where this vector belongs, creating it if necessary,
   and returns the minimum distance squared to any of the bin boundaries.
*/
double find_right_bin(hitnode* node, int level, int maxlevels,
					  double dist2sofar, double* vec,
					  hitnode** p_result_node) {
	double val;
	int i;
	int bin;
	double dleft, dright;
	double mindist2;

	//printf("level %i of %i.\n", level, maxlevels);

	if (level == maxlevels) {
		*p_result_node = node;
		//printf("returning mindist2=%g\n", dist2sofar);
		return dist2sofar;
	}

	if (!node->children) {
		// create this node's children...
		//printf("creating children nodes.\n");
		node->children = (hitnode*)malloc(NBINS * sizeof(hitnode));
		for (i=0; i<NBINS; i++) {
			node->children[i].dim = level + 1;
			node->children[i].matchlist = NULL;
			node->children[i].agreelist = NULL;
			node->children[i].children = NULL;
		}
	}

	// compute which bin this vector belongs in.
	val = vec[node->dim];
	bin = (int)(val * NBINS);
	if (bin < 0) bin = 0;
	if (bin >= NBINS) bin = NBINS-1;

	// compute the distances to the bin boundaries.
	dleft  = square(val - (double)bin / (double)NBINS);
	dright = square((double)(bin+1) / (double)NBINS - val);
	mindist2 = (dleft < dright) ? dleft : dright;

	//printf("val %g, bin %i, mindist2 %g, mindist %g\n", val, bin, mindist2, sqrt(mindist2));

	return find_right_bin(node->children + bin, level+1, maxlevels,
						  dist2sofar + mindist2, vec, p_result_node);
}

int hitlist_add_hit(hitlist* hlist, MatchObj* mo) {
	double mindist2;
	hitnode* target;
	blocklist* potlist;

	mindist2 = find_right_bin(&(hlist->root), 0, hlist->levels, 0.0,
							  mo->vector, &target);

	// potential bins that this match could agree with.
	potlist = blocklist_pointer_new(16);

	if (mindist2 < hlist->agreedist2) {
		// this vector is far enough from the boundary that it can't
		// agree with a match in any other bin.
		blocklist_pointer_append(potlist, target);
	} else {
		// gather up the set of bins that this match could agree with.
		find_potential_agreers(&(hlist->root), 0, hlist->levels, 0.0,
							   hlist->agreedist2, mo->vector, potlist);
	}

	// find any sets of matches that this match could agree with
	// and merge them.
	

	blocklist_free(potlist);

	return 0;
}

void hitlist_clear(hitlist* hlist) {
}

void hitlist_free_extra(hitlist* hlist, void (*free_function)(MatchObj* mo)) {
}

void hitlist_free(hitlist* hlist) {
}

// returns a shallow copy of the best set of hits.
// you are responsible for calling blocklist_free.
blocklist* hitlist_get_best(hitlist* hlist) {
	return NULL;
	// COPY return hlist->best;
}

// returns a shallow copy of the whole set of results.
// you are responsible for calling blocklist_free.
blocklist* hitlist_get_all(hitlist* hlist) {
	return NULL;
}

void hitlist_add_hits(hitlist* hlist, blocklist* hits) {
}

int hitlist_count_best(hitlist* hitlist) {
	return hitlist->nbest;
}

int hitlist_count_all(hitlist* hitlist) {
	return hitlist->ntotal;
}

