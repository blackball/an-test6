/**

   -read a kdtree of stars
   -for each star A, find all stars X within range [0, 2s].
    In X, build each quad using star B if |B-A| is in [s/2, 2s],
	and choose stars C, D in the box that has AB as the diagonal.

 */

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include "starutil.h"
#include "fileutil.h"
#include "dualtree.h"
#include "KD/ambs.h"
#include "KD/distances.h"
#include "KD/hrect.h"
#include "KD/amiv.h"

bool within_range(void* params, node* search, node* query);
void handle_result(void* params, node* search, node* query);
void first_result(void* vparams, node* query);
void last_result(void* vparams, node* query);

#define printf_stats(a,...)
//#define printf_stats printf

struct params
{
    double radius;

	// radius-squared of the search range.
	double maxdistsq;

    // all the stars.
	dyv_array* stararray;

    // total number of quads created.
    int nquads;
};
typedef struct params params;

void handle_leaves(params* p, node* search, node* query);


struct cnode {
	node* n;
	struct cnode* next;
};
typedef struct cnode cnode;

// a struct that contains info about all the candidate
// points returned by a query.
struct result_info {
	// the list of candidate kdtree leaves
	cnode* cands;
};
typedef struct result_info result_info;

result_info rinfo;



// dimension of the space
int D = 2;

#define OPTIONS "hf:s:t:n:k:"

extern char *optarg;
extern int optind, opterr, optopt;

char *catfname = NULL;
FILE *catfid = NULL;
off_t catposmarker = 0;
char cASCII = (char)READ_FAIL;
char buff[100], oneobjWidth;

int main(int argc, char *argv[]) {
  char* progname;
  int argchar; //  opterr = 0;
  double ramin, ramax, decmin, decmax;
  sidx numstars;
  int i;
  int nkeep = 0;

  kdtree* startree = NULL;
  // arrays for points
  dyv_array* stararray = NULL;

  // radius of the search (arcmin?)
  //double radius = 5.0;
  double radius = 0.04;

  double step = 1.0;
  int nsteps = 1;

  params range_params;

  // dual-tree search callback functions
  dualtree_callbacks callbacks;
  
  // maximum number of points in a leaf node.
  int Nleaf = 5;

  progname = argv[0];

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
	switch (argchar) {
	case 'f':
	  catfname = mk_catfn(optarg);
	  break;
	case 's':
	  radius = atof(optarg);
	  if (radius == 0.0) {
		printf("Couldn't parse desired scale \"%s\"\n", optarg);
		exit(-1);
	  }
	  break;
	case 't':
	  step = atof(optarg);
	  if (step == 0.0) {
		printf("Couldn't parse desired scale step: \"%s\"\n", optarg);
		exit(-1);
	  }
	  break;
	case 'n':
	  nsteps = atoi(optarg);
	  if (nsteps == 0) {
		printf("Couldn't parse desired number of steps: \"%s\"\n", optarg);
		exit(-1);
	  }
	  break;
	case 'k':
	  nkeep = atoi(optarg);
	  if (nkeep == 0) {
		printf("Couldn't parse desired number of stars: \"%s\"\n", optarg);
		exit(-1);
	  }
	  break;
	default:
	  return (OPT_ERR);
	}
    
  dimension DimStars;
  fprintf(stderr, "%s: reading catalogue  %s\n", progname, catfname);
  if (catfname == NULL) {
	fprintf(stderr, "\nYou must specify input catalogue file (-f <input>), without the .objs suffix)\n\n");
	exit(0);
  }
  fopenin(catfname, catfid);
  free_fn(catfname);

  cASCII = read_objs_header(catfid, &numstars, &DimStars,
							&ramin, &ramax, &decmin, &decmax);
  if (cASCII == (char)READ_FAIL)
	return (1);
  if (cASCII) {
	sprintf(buff, "%lf,%lf,%lf\n", 0.0, 0.0, 0.0);
	oneobjWidth = strlen(buff);
  }
  catposmarker = ftello(catfid);

  fprintf(stderr, "    (%lu stars) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
		  numstars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

  printf("dimension: %i\n", DimStars);
  if (DimStars != 3) {
	printf("DimStars isn't 3!\n");
	exit(-1);
  }

  if (nkeep && (nkeep < numstars)) {
	printf("Keeping only the first %i stars.\n", nkeep);
	numstars = nkeep;
  }

  printf("Allocating star array...\n");
  // create stars array
  stararray = mk_dyv_array(numstars);
  if (!stararray) {
	printf("Couldn't create stararray.\n");
	exit(-1);
  }
  // first (try to) allocate memory...
  for (i=0; i<numstars; i++) {
	dyv* star = mk_dyv(DimStars);
	if (i % 100000 == 0) { printf("."); fflush(stdout); }
	dyv_array_ref(stararray, i) = star;
	if (!star) {
	  printf("Couldn't allocate memory for star %i.", i);
	  exit(-1);
	}
  }
  printf("\n");
  {
	double minx, miny, minz, maxx, maxy, maxz;
	minx = miny = minz = 1e100;
	maxx = maxy = maxz = -1e100;
	// then read the catalogue.
	for (i=0; i<numstars; i++) {
	  dyv* star = dyv_array_ref(stararray, i);
	  if (cASCII) {
		double tmpx, tmpy, tmpz;
		fscanf(catfid, "%lf,%lf,%lf\n", &tmpx, &tmpy, &tmpz);
		star_set(star, 0, tmpx);
		star_set(star, 1, tmpy);
		star_set(star, 2, tmpz);
	  } else {
		freadstar(star, catfid);
	  }
	  {
		double v;
		v = dyv_ref(star, 0);
		if (v > maxx) maxx = v;
		if (v < minx) minx = v;
		v = dyv_ref(star, 1);
		if (v > maxy) maxy = v;
		if (v < miny) miny = v;
		v = dyv_ref(star, 1);
		if (v > maxz) maxz = v;
		if (v < minz) minz = v;
	  }
	}
	printf("ranges: x = [%g, %g], y = [%g, %g], z = [%g, %g]\n",
		   minx, maxx, miny, maxy, minz, maxz);
  }

  // create search tree
  printf("Creating kdtree of stars...\n");
  startree = mk_kdtree_from_points(stararray, Nleaf);
  printf("Done creating kdtree.\n");

  // set search params
  range_params.radius = radius;
  range_params.maxdistsq = 4.0  * radius * radius;
  range_params.stararray = stararray;
  range_params.nquads = 0;

  memset(&callbacks, 0, sizeof(dualtree_callbacks));
  callbacks.decision = within_range;
  callbacks.decision_extra = &range_params;
  callbacks.result = handle_result;
  callbacks.result_extra = &range_params;
  callbacks.start_results = first_result;
  callbacks.start_extra = &range_params;
  callbacks.end_results = last_result;
  callbacks.end_extra = &range_params;

  // run dual-tree search
  for (i=0; i<nsteps; i++) {
	printf("Running dual-tree search (scale %g)...\n", radius);
	dualtree_search(startree, startree, &callbacks);
	printf("Quads: %i.\n", range_params.nquads);
	printf("%g %i\n", radius, range_params.nquads);
	radius *= step;
	range_params.radius = radius;
	range_params.maxdistsq = 4.0  * radius * radius;
	range_params.nquads = 0;
  }
  printf("Done doing dual-tree search.\n");
  
  //printf("%i\n", range_params.nquads);

  return 0;
}

bool within_range(void* vparams, node* search, node* query) {
	params* p = (params*)vparams;
	double mindistsq;
	bool result;
	// compute the minimum distance between the search and query nodes.
	// accept it iff it's inside the search radius.
	mindistsq = hrect_hrect_min_dsqd(search->box, query->box);
	result = (mindistsq <= p->maxdistsq);
	return result;
}


void first_result(void* vparams, node* query) {
	// init "result_info" struct.
	rinfo.cands = NULL;
}


void build_quads(dyv_array* points, ivec* inds, int ninds, int iA,
                 double radius, int* pnquads) {
    int b;
    dyv* pA;
    double minrsq, maxrsq;
    ivec* cdinds;
    int ncd;
    dyv* midAB;
    dyv* delt;
    int i;
    int nquads = 0;

    pA = dyv_array_ref(points, iA);
    cdinds = mk_ivec(ninds);
    minrsq = 0.25 * radius * radius;
    maxrsq = 4.0  * radius * radius;
    midAB = mk_dyv(dyv_size(pA));
    delt = mk_dyv(dyv_size(pA));

    // find all points B that are in [r/2, 2r].
    for (b=0; b<ninds; b++) {
        int iB = ivec_ref(inds, b);
        dyv* pB = dyv_array_ref(points, iB);
        double distsq = dyv_dyv_dsqd(pA, pB);
        int c, d, iC, iD;
        double distAB;
        if ((distsq <= minrsq) || (distsq >= maxrsq)) {
            /*printf_stats("AB dist %g out of range [%g, %g]\n",
              sqrt(distsq), sqrt(minrsq), sqrt(maxrsq));
            */
            continue;
        }
        distAB = sqrt(distsq);
        // compute the midpoint between A and B.
        dyv_plus(pA, pB, midAB);
        dyv_scalar_mult(midAB, 0.5, midAB);
        ncd = 0;
        // find all points C,D that are inside the box with AB diagonal.
        for (c=0; c<ninds; c++) {
            double onenorm;
            dyv* pC;
            if (c == b) continue;
            iC = ivec_ref(inds, c);
            if (iC == iA) continue;
            pC = dyv_array_ref(points, iC);
            // delt = pC - midAB
            dyv_subtract(pC, midAB, delt);
            // 1-norm of delt
            onenorm = 0.0;
            for (i=0; i<dyv_size(delt); i++) {
                onenorm += real_abs(dyv_ref(delt, i));
            }
            // is pC inside the diamond?
            if (onenorm > (distAB/2)) {
                continue;
            }
            ivec_ref(cdinds, ncd) = iC;
            ncd++;
        }
        for (c=0; c<ncd; c++) {
            iC = ivec_ref(cdinds, c);
            for (d=0; d<ncd; d++) {
                if (d == c) continue;
                iD = ivec_ref(cdinds, d);
                //printf_stats("    quad inds %i, %i, %i, %i\n", iA, iB, iC, iD);
                nquads++;
            }
        }
    }

    printf_stats("    created %i quads.\n", nquads);

    if (pnquads) *pnquads = nquads;

    free_dyv(delt);
    free_dyv(midAB);
    free_ivec(cdinds);
}

// all the candidate leaf nodes have been gathered.
// form all the quads.
void last_result(void* vparams, node* query) {
	int i, j, pi;
	int nnodes;
	int npoints;
	cnode* n;
	//int* allpinds;
	ivec* pinds;
	params* p = (params*)vparams;

	// how many points are in the search node list?
	npoints = 0;
	nnodes = 0;
	for (n=rinfo.cands; n; n=n->next) {
		nnodes++;
		npoints += n->n->num_points;
	}

	printf_stats("query node: %i points.  search nodes: %i.  search points: %i.\n",
		   query->num_points, nnodes, npoints);

	// collect all the points.
	//allpinds = (int*)malloc(sizeof(int) * npoints);
	pinds = mk_ivec(npoints);

	// look at individual points in the query tree.
	for (i = 0; i < query->num_points; i++) {
		dyv* qp = dyv_array_ref(query->points, i);
        int qi  = ivec_ref(query->pindexes, i);
        int nquads;

		printf_stats("  query point %i:\n", i);
		pi = 0;
		for (n=rinfo.cands, j=0; n; n=n->next, j++) {
			int k;
			node* search = n->n;
			// prune search nodes that aren't within range for this point.
			double mindistsq = hrect_dyv_min_dsqd(search->box, qp);
			if (mindistsq > p->maxdistsq) {
				printf_stats("    pruned box %i.\n", j);
				continue;
			}
			// copy pindices.
			for (k=0; k<search->num_points; k++) {
				ivec_ref(pinds, pi + k) = ivec_ref(search->pindexes, k);
			}
			pi += k;
		}

		printf_stats("    collected %i points.\n", pi);

		build_quads(p->stararray, pinds, pi, qi, p->radius, &nquads);

        p->nquads += nquads;
	}

	free_ivec(pinds);

	// free the list.
	for (n = rinfo.cands; n;) {
		cnode* tmp = n->next;
		free(n);
		n = tmp;
	}
	rinfo.cands = NULL;
}

void handle_leaves(params* p, node* search, node* query) {
	// append this node to the list of candidates.
	cnode* newnode = (cnode*)malloc(sizeof(cnode));
	newnode->n = search;
	newnode->next = rinfo.cands;
	rinfo.cands = newnode;
}


void handle_result(void* vparams, node* search, node* query) {
	params* p = (params*)vparams;

	// this function gets called on the children of accepted nodes, so this
	// pair of nodes may not be acceptable.
	if (!within_range(vparams, search, query)) {
		return ;
	}

	// either node can be a non-leaf.  in this case, recurse until we
	// hit leaves.
	if (!node_is_leaf(search)) {
		handle_result(vparams, search->child1, query);
		handle_result(vparams, search->child2, query);
		return ;
	}

	if (!node_is_leaf(query)) {
		handle_result(vparams, search, query->child1);
		handle_result(vparams, search, query->child2);
		return ;
	}

	// okay, here we are, we've got a pair of valid leaf nodes.
	handle_leaves(p, search, query);
}

