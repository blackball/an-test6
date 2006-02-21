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
	// list of indexes into the master MatchObj list, of MatchObjects
	// that belong to this bin.
	blocklist* matchinds;

	// note, the previous two lists are supposed to stay in sync,
	// so if you do an operation to one you'd better do the same
	// operation on the other!

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

	// master list of MatchObj*: matches
	blocklist* matchlist;
	// list in ints, same size as matchlist; containing indexes into
	// agreelist; the list to which the corresponding MatchObj
	// belongs.
	blocklist* memberlist;
	// list of blocklist*; the lists of agreeing matches.
	blocklist* agreelist;
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

void init_hitnode(hitnode* node) {
	node->matchinds = NULL;
	node->children = NULL;
}

hitlist* hitlist_new() {
	//int d;
	hitlist* hl = (hitlist*)malloc(sizeof(hitlist));
	hl->ntotal = 0;
	hl->nbest = 0;
	hl->best = NULL;
	init_hitnode(&(hl->root));
	hl->root.dim = 0;
	/*
	  for (d=0; d<DIMS; d++)
	  hl->scales[d] = 1.0;
	*/
	hl->levels = LEVELS;
	hl->agreedist2 = square(AGREE_DIST);

	hl->matchlist = blocklist_pointer_new(256);
	hl->memberlist = blocklist_int_new(256);
	hl->agreelist = blocklist_pointer_new(64);

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
			init_hitnode(&(node->children[i]));
			node->children[i].dim = level + 1;
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

int hits_agree(MatchObj* m1, MatchObj* m2, double agreedist2) {
	return 1;
}

int hitlist_add_hit(hitlist* hlist, MatchObj* match) {
	double mindist2;
	hitnode* target;
	blocklist* potlist;
	int p, P;
	blocklist* mergelist = NULL;
	int mergeind = -1;
	int matchind;

	mindist2 = find_right_bin(&(hlist->root), 0, hlist->levels, 0.0,
							  match->vector, &target);

	// potential bins that this match could agree with.
	potlist = blocklist_pointer_new(16);

	if (mindist2 < hlist->agreedist2) {
		// this vector is far enough from the boundary that it can't
		// agree with a match in any other bin.
		blocklist_pointer_append(potlist, target);
	} else {
		// gather up the set of bins that this match could agree with.
		find_potential_agreers(&(hlist->root), 0, hlist->levels, 0.0,
							   hlist->agreedist2, match->vector, potlist);
	}

	// add this MatchObj to the master list, and add its index to its bin's list.
	blocklist_pointer_append(hlist->matchlist, match);
	// add a placeholder agreement-list-membership value.
	blocklist_int_append(hlist->memberlist, -1);
	matchind = blocklist_count(hlist->matchlist) - 1;
	if (!target->matchinds) {
		target->matchinds = blocklist_int_new(32);
	}
	blocklist_int_append(target->matchinds, matchind);

	// look at all the matches in the potentially-matching bins.
	// for any that agree, record the agreeing list to which they belong.
	// at the end, merge all these lists (or create a new one if it
	// doesn't agree with any existing one), and add this match to the
	// bin to which it belongs.

	P = blocklist_count(potlist);
	mergelist = NULL;
	mergeind = -1;
	// for each potentially-matching bin...
	for (p=0; p<P; p++) {
		int m, M;
		hitnode* node = (hitnode*)blocklist_pointer_access(potlist, p);
		if (!node->matchinds) continue;
		M = blocklist_count(node->matchinds);
		// for each match in this bin...
		for (m=0; m<M; m++) {
			int ind = blocklist_int_access(node->matchinds, m);
			MatchObj* mo = (MatchObj*)blocklist_pointer_access(hlist->matchlist, ind);
			int agreeind;

			if (!hits_agree(match, mo, hlist->agreedist2))
				continue;

			// this match agrees.

			// to which agreement list does it belong?
			agreeind = blocklist_int_access(hlist->memberlist, ind);
			if (mergeind == -1) {
				// this is the first agreeing match we've found.  Add this MatchObj to
				// that list; we'll also merge any other agree-ers into that list.
				mergeind = agreeind;
				mergelist = (blocklist*)blocklist_pointer_access(hlist->agreelist, mergeind);
				blocklist_int_append(mergelist, matchind);
			} else if (agreeind == mergeind) {
				// this agreeing match is already a member of the right agreement list.
				// do nothing.
			} else {
				// merge lists.
				blocklist* agreelist;
				int a, A;
				agreelist = (blocklist*)blocklist_pointer_access(hlist->agreelist, agreeind);
				A = blocklist_count(agreelist);
				// go through this agreement list and tell all its members that they
				// now belong to mergelist.
				for (a=0; a<A; a++) {
					int aind = blocklist_int_access(agreelist, a);
					blocklist_int_set(hlist->memberlist, aind, mergeind);
				}
				// append all the elements of this agreement list to 'mergelist';
				blocklist_append_list(mergelist, agreelist);
				blocklist_free(agreelist);
				blocklist_pointer_set(hlist->agreelist, agreeind, NULL);
			}
		}
	}
	blocklist_free(potlist);

	if (mergeind == -1) {
		int agreeind = -1;
		int a, A;
		blocklist* newlist;

		// We didn't find any agreeing matches.  Create a new agreement
		// list and add this MatchObj to it.
		newlist = blocklist_int_new(4);

		// find an available spot in agreelist:
		A = blocklist_count(hlist->agreelist);
		for (a=0; a<A; a++) {
			if (blocklist_pointer_access(hlist->agreelist, a))
				continue;
			// found one!
			agreeind = a;
			blocklist_pointer_set(hlist->agreelist, a, newlist);
			break;
		}
		if (agreeind == -1) {
			// add it to the end.
			agreeind = blocklist_count(hlist->agreelist);
			blocklist_pointer_append(hlist->agreelist, newlist);
		}
		blocklist_int_append(newlist, matchind);
		mergeind = agreeind;
	}

	blocklist_int_set(hlist->memberlist, matchind, mergeind);

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

