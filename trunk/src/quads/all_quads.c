/**
   Author: Dustin Lang

   -read a star kdtree

   -scale 's', lower-limit scale factor 'l'.
   -use dual-tree search to do:
   -for each star A, find all stars X within range [0, s].
   In X, build each quad using star B if |B-A| is in [l*s, s],
   and choose stars C, D in the box that has AB as the diagonal.

   -write .quad, .code, .qidx output files.
*/

#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include "kdutil.h"
#include "starutil.h"
#include "fileutil.h"
#include "dualtree.h"
#include "KD/ambs.h"
#include "KD/distances.h"
#include "KD/hrect.h"
#include "KD/amiv.h"
#include "blocklist.h"

bool within_range(void* params, node* search, node* query);
void handle_result(void* params, node* search, node* query);
void first_result(void* vparams, node* query);
void last_result(void* vparams, node* query);

#define printf_stats(a,...)
//#define printf_stats printf

struct params
{
    // radius-squared of the search range.
    double mindistsq;
    double maxdistsq;

    // all the stars.
    stararray* stars;

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

#define OPTIONS "hcqf:s:l:x"

extern char *optarg;
extern int optind, opterr, optopt;

FILE *quadfid = NULL;
FILE *codefid = NULL;

// this is an array of lists, one for each star.
// each list contains the indices of the quads to which
// that star belongs.
blocklist** quadlist = NULL;

sidx numstars;
int nstarsdone = 0;
int lastpercent = 0;
int lastcount = 0;
int justcount = 0;
int quiet = 0;
bool writeqidx = TRUE;

void print_help(char* progname) {
    printf("\nUsage:\n"
		   "  %s -f <filename-base>\n"
		   "     [-s <scale>]         (default scale is 5 arcmin)\n"
		   "     [-l <range>]         (lower bound on scale of quads - fraction of the scale; default 0)\n"
		   "     [-c]                 (just count the quads, don't write anything)\n"
		   "     [-q]                 (be quiet!  No progress reports)\n"
		   "     [-x]                 (don't write quad index .qidx file)\n"
		   "\n"
		   , progname);
}

int main(int argc, char *argv[]) {
    char* progname;
    int argchar; //  opterr = 0;
    double ramin, ramax, decmin, decmax;
    int hdrlength = 0;

    kdtree* startree = NULL;
    stararray* stars = NULL;

    // max radius of the search (in radians)
    double radius;

    // lower bound of quad scale (fraction of radius)
    double lower = 0.0;

    params range_params;

    // dual-tree search callback functions
    dualtree_callbacks callbacks;
  
    char *treefname = NULL;
    char *quadfname = NULL;
    char *codefname = NULL;
    char *qidxfname = NULL;

    FILE *treefid = NULL;
    FILE *qidxfid = NULL;

    progname = argv[0];

    radius = arcmin2rad(5.0);

    if (argc == 1) {
		print_help(progname);
		exit(0);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			print_help(progname);
			exit(0);
		case 'x':
			writeqidx = FALSE;
			break;
		case 'c':
			justcount = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'f':
			treefname = mk_streefn(optarg);
			quadfname = mk_quadfn(optarg);
			codefname = mk_codefn(optarg);
			qidxfname = mk_qidxfn(optarg);
			break;
		case 's':
			radius = atof(optarg);
			if (radius == 0.0) {
				printf("Couldn't parse desired scale \"%s\"\n", optarg);
				exit(-1);
			}
			radius = arcmin2rad(radius);
			break;
		case 'l':
			lower = atof(optarg);
			if (lower > 1.0) {
				printf("You really don't want to make lower > 1.\n");
				exit(-1);
			}
			break;
		default:
			return (OPT_ERR);
		}
    
    fprintf(stderr, "%s: reading star kd-tree %s\n", progname, treefname);
    if (treefname == NULL) {
		fprintf(stderr, "\nYou must specify the file names to use (-f <prefix>), (without the suffix)\n\n");
		exit(0);
    }
    fopenin(treefname, treefid);
    free_fn(treefname);
    startree = read_starkd(treefid, &ramin, &ramax, &decmin, &decmax);
    fclose(treefid);
    if (startree == NULL)
		return (2);
    numstars = startree->root->num_points;

    fprintf(stderr, "done\n    (%lu stars, %d nodes, depth %d).\n",
			numstars, startree->num_nodes, startree->max_depth);
    fprintf(stderr, "    (dim %d) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
			kdtree_num_dims(startree),
			rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

    stars = (stararray *)mk_dyv_array_from_kdtree(startree);

    if (!justcount) {

		fopenout(quadfname, quadfid);
		fopenout(codefname, codefid);

		// we have to write an updated header after we've processed all the quads.
		{
			uint nelems = 1000000000;
			hdrlength = 10;
			write_code_header(codefid, nelems, numstars,
							  DIM_CODES, radius);
			write_quad_header(quadfid, nelems, numstars,
							  DIM_QUADS, radius);
		}
    }

    printf("%sing all quads in the range [%f, %f] arcmin\n", (justcount ? "Count" : "Creat"),
		   lower * rad2arcmin(radius), rad2arcmin(radius));

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

	// set search params
	range_params.maxdistsq = radius * radius;
	range_params.mindistsq = radius * radius * lower * lower;
	range_params.stars = stars;
	range_params.nquads = 0;

	if (writeqidx) {
		quadlist = (blocklist**)malloc(numstars * sizeof(blocklist*));
		memset(quadlist, 0, numstars * sizeof(blocklist*));
	}

	if (!quiet)
		printf("Running dual-tree search (scale %g)...\n", radius);
	dualtree_search(startree, startree, &callbacks);
	printf("Quads: %i.\n", range_params.nquads);
	printf("%g %g %i\n",
		   rad2arcmin(radius * lower),
		   rad2arcmin(radius), range_params.nquads);

    if (!quiet) {
		printf("Done doing dual-tree search.\n");
		fflush(stdout);
	}

	free_kdtree(startree);
	startree = NULL;

	free_dyv_array_from_kdtree(stars);
	stars = NULL;

    if (!justcount) {
		int i, j;
		sidx numused = 0;
		magicval magic = MAGIC_VAL;

		// fix output file headers.
		fix_code_header(codefid, range_params.nquads, hdrlength);
		fix_quad_header(quadfid, range_params.nquads, hdrlength);

		// close .code and .quad files
		if (fclose(codefid)) {
			printf("Couldn't write code output file: %s\n", strerror(errno));
			exit(-1);
		}
		if (fclose(quadfid)) {
			printf("Couldn't write quad output file: %s\n", strerror(errno));
			exit(-1);
		}

		if (writeqidx) {
			fopenout(qidxfname, qidxfid);
			// write qidx (adapted from quadidx.c)

			printf("Writing qidx file...\n");
			fflush(stdout);

			// first count numused:
			// how many stars are members of quads.
			for (i=0; i<numstars; i++) {
				blocklist* list = quadlist[i];
				if (!list) continue;
				numused++;
			}
			printf("%li stars used\n", numused);
			if (fwrite(&magic, sizeof(magic), 1, qidxfid) != 1) {
				printf("Error writing magic: %s\n", strerror(errno));
				exit(-1);
			}
			if (fwrite(&numused, sizeof(numused), 1, qidxfid) != 1) {
				printf("Error writing numused: %s\n", strerror(errno));
				exit(-1);
			}
			for (i=0; i<numstars; i++) {
				qidx thisnumq;
				sidx thisstar;
				blocklist* list = quadlist[i];
				if (!list) continue;
				thisnumq = (qidx)blocklist_count(list);
				thisstar = i;
				if ((fwrite(&thisstar, sizeof(thisstar), 1, qidxfid) != 1) ||
					(fwrite(&thisnumq, sizeof(thisnumq), 1, qidxfid) != 1)) {
					printf("Error writing qidx entry for star %i: %s\n", i,
						   strerror(errno));
					exit(-1);
				}
				for (j=0; j<thisnumq; j++) {
					qidx kk;
					blocklist_get(list, j, &kk);
					if (fwrite(&kk, sizeof(kk), 1, qidxfid) != 1) {
						printf("Error writing qidx quads for star %i: %s\n",
							   i, strerror(errno));
						exit(-1);
					}
				}
				blocklist_free(list);
				quadlist[i] = NULL;
			}
		
			if (fclose(qidxfid)) {
				printf("Couldn't write quad index file: %s\n", strerror(errno));
				exit(-1);
			}
			free(quadlist);
		}
    }

	printf("Done.\n");
	fflush(stdout);

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



// from quadidx.c
void insertquad(qidx quadid,
                sidx iA, sidx iB, sidx iC, sidx iD) {
	blocklist* list;
	sidx inds[4];
	int i;

	inds[0] = iA;
	inds[1] = iB;
	inds[2] = iC;
	inds[3] = iD;

	// append this quad index to the lists of each of
	// its four stars.
	for (i=0; i<4; i++) {
		sidx starind = inds[i];
		list = quadlist[starind];
		// create the list if necessary
		if (!list) {
			list = blocklist_new(10, sizeof(qidx));
			quadlist[starind] = list;
		}
		blocklist_append(list, &quadid);
	}
}

void accept_quad(int quadnum, sidx iA, sidx iB, sidx iC, sidx iD,
                 double Cx, double Cy, double Dx, double Dy) {
    sidx itmp;
    double tmp;
    if (iC > iD) { // swap C and D if iC>iD, involves swapping Cxy/Dxy
		itmp = iC;
		iC = iD;
		iD = itmp;
		tmp = Cx;
		Cx = Dx;
		Dx = tmp;
		tmp = Cy;
		Cy = Dy;
		Dy = tmp;
    }
    if (iA > iB) { //swap A,B if iA>iB, involves C/Dxy->1-C/Dxy (??HOPE THIS IS OK)
		itmp = iA;
		iA = iB;
		iB = itmp;
		Cx = 1.0 - Cx;
		Cy = 1.0 - Cy;
		Dx = 1.0 - Dx;
		Dy = 1.0 - Dy;
    }

    writeonecode(codefid, Cx, Cy, Dx, Dy);
    writeonequad(quadfid, iA, iB, iC, iD);

	if (writeqidx)
		insertquad(quadnum, iA, iB, iC, iD);
}

/**
   \c inds contains real star indices.
   \c iA   is a real star index.
*/
void build_quads(dyv_array* points, ivec* inds, int ninds, int iA,
                 double minrsq, double maxrsq, int* pnquads) {
    int b;
    dyv* pA;
    ivec* cdinds = NULL;
    dyv* cdx = NULL;
    dyv* cdy = NULL;

    int ncd;
    dyv* midAB;
    dyv* delt;
    double distsq;
    // projected coordinates:
    double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
    double AAx, AAy;
    double ABx, ABy;
    double scale, costheta, sintheta;

    pA = dyv_array_ref(points, iA);

	printf_stats("iA=%i\n", iA);
	printf_stats("pA=(%g,%g,%g)\n",
				dyv_ref(pA,0),
				dyv_ref(pA,1),
				dyv_ref(pA,2));


    midAB = mk_dyv(dyv_size(pA));
    delt = mk_dyv(dyv_size(pA));
    if (!justcount) {
		cdinds = mk_ivec(ninds);
		cdx = mk_dyv(ninds);
		cdy = mk_dyv(ninds);
    }

    star_coords(pA, pA, &AAx, &AAy);

    // find all points B that are in [lower*r, r]
    for (b=0; b<ninds; b++) {
        int iB = ivec_ref(inds, b);
        dyv* pB;
        int c, d, iC=0, iD;

		// avoid creating permutations
		//if (iB < iA) continue;
		if (iB <= iA) continue;

		pB = dyv_array_ref(points, iB);

		printf_stats("  iB=%i\n", iB);
		printf_stats("  pB=(%g,%g,%g)\n",
					dyv_ref(pB,0),
					dyv_ref(pB,1),
					dyv_ref(pB,2));

		star_coords(pB, pA, &ABx, &ABy);

		printf_stats("  coords (%g, %g)\n", ABx, ABy);

		distsq = (ABx-AAx)*(ABx-AAx) + (ABy-AAy)*(ABy-AAy);

		printf_stats("  distsq %g   range [%g, %g]\n", distsq,
					minrsq, maxrsq);

        if ((distsq < minrsq) || (distsq > maxrsq)) {
			/*printf_stats("AB dist %g out of range [%g, %g]\n",
			  sqrt(distsq), sqrt(minrsq), sqrt(maxrsq));
			*/
			continue;
        }

		star_midpoint(midAB, pA, pB);
		star_coords(pA, midAB, &Ax, &Ay);
		star_coords(pB, midAB, &Bx, &By);

		ABx = Bx - Ax;
		ABy = By - Ay;

		scale = (ABx*ABx) + (ABy*ABy);
		costheta = (ABx + ABy) / scale;
		sintheta = (ABy - ABx) / scale;

        ncd = 0;
        // find all points C,D that are inside the box with AB diagonal.
        for (c=0; c<ninds; c++) {
			double ACx, ACy;
			double thisx, thisy;
			dyv* pC;
			if (c == b) continue;
			iC = ivec_ref(inds, c);
			if (iC == iA) continue;
			pC = dyv_array_ref(points, iC);

			star_coords(pC, midAB, &Cx, &Cy);

			printf_stats("    iC=%i\n", iC);
			printf_stats("    pC (%g, %g, %g)\n",
						 dyv_ref(pC, 0),
						 dyv_ref(pC, 1),
						 dyv_ref(pC, 2));
			printf_stats("    coords (%g, %g)\n", Cx, Cy);

			ACx = Cx - Ax;
			ACy = Cy - Ay;

			thisx =  ACx * costheta + ACy * sintheta;
			thisy = -ACx * sintheta + ACy * costheta;

			printf_stats("    box (%g, %g)\n", thisx, thisy);

			if (!((thisx <= 1.0) && (thisx >= 0.0) &&
				  (thisy <= 1.0) && (thisy >= 0.0))) {
				printf_stats("    -> outside box.\n");
				continue;
			}
			printf_stats("    -> inside box.\n");

			if (!justcount) {
				ivec_set(cdinds, ncd, iC);
				dyv_set(cdx, ncd, thisx);
				dyv_set(cdy, ncd, thisy);
			}
			ncd++;
        }
        for (c=0; c<ncd; c++) {
			if (!justcount) {
				iC = ivec_ref(cdinds, c);
				Cx = dyv_ref(cdx, c);
				Cy = dyv_ref(cdy, c);
			}
			// avoid C-D permutations
			for (d=c+1; d<ncd; d++) {
				if (!justcount) {
					iD = ivec_ref(cdinds, d);
					Dx = dyv_ref(cdx, d);
					Dy = dyv_ref(cdy, d);
					//printf_stats("    quad inds %i, %i, %i, %i\n", iA, iB, iC, iD);
					accept_quad(*pnquads, iA, iB, iC, iD,
								Cx, Cy, Dx, Dy);
				}
				(*pnquads)++;
			}
        }
    }

    printf_stats("    created %i quads.\n", *pnquads);

    free_dyv(delt);
    free_dyv(midAB);
    if (!justcount) {
		free_ivec(cdinds);
		free_dyv(cdx);
		free_dyv(cdy);
    }
}

void first_result(void* vparams, node* query) {
    // init "result_info" struct.
    rinfo.cands = NULL;
}


// all the candidate leaf nodes have been gathered.
// form all the quads.
void last_result(void* vparams, node* query) {
    int i, j, pi;
    int nnodes;
    int npoints;
    cnode* n;
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
    pinds = mk_ivec(npoints);

    // look at individual points in the query tree.
    for (i = 0; i < query->num_points; i++) {
		dyv* qp = dyv_array_ref(query->points, i);
        int qi  = ivec_ref(query->pindexes, i);

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
				ivec_set(pinds, pi + k, ivec_ref(search->pindexes, k));
			}
			pi += search->num_points;
		}

		printf_stats("    collected %i points.\n", pi);

		build_quads(p->stars, pinds, pi, qi,
					p->mindistsq, p->maxdistsq,
					&(p->nquads));

		if (!quiet) {
			int pct;
			double percent;
			nstarsdone++;
			percent = 100.0 * (double)nstarsdone / (double)numstars;
			pct = (int)(0.5 + percent);
			if ((pct != lastpercent) ||
				((p->nquads - lastcount) > 1000000)) {
				double quadest = (100.0 / percent) * (double)(p->nquads);
				printf("%i of %i (%i%%) done.  %i quads created, rough estimate %.2g total.\n",
					   nstarsdone, (int)numstars, pct, p->nquads, quadest);
				fflush(stdout);
				lastpercent = pct;
				lastcount = p->nquads;
			}
		}
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

